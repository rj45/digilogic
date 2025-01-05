use crate::*;
use std::hash::Hash;
use std::net::Ipv4Addr;
use std::ops::Index;
use std::time::Instant;

pub use renet::ClientId;

trait RenetServerExt {
    fn send_command_message(&mut self, client_id: ClientId, message: ServerMessage);
    fn send_sim_state(&mut self, client_id: ClientId, sim_state: &SimState);
}

impl RenetServerExt for RenetServer {
    fn send_command_message(&mut self, client_id: ClientId, message: ServerMessage) {
        let message = rmp_serde::to_vec(&message).unwrap();
        self.send_message(client_id, COMMAND_CHANNEL_ID, message);
    }

    fn send_sim_state(&mut self, client_id: ClientId, sim_state: &SimState) {
        let message = rmp_serde::to_vec(sim_state).unwrap();
        self.send_message(client_id, DATA_CHANNEL_ID, message);
    }
}

pub type ServerResult<T> = Result<T, ServerError>;

macro_rules! gate_stub {
    ($name:ident) => {
        fn $name(
            &mut self,
            client_id: ClientId,
            width: NonZeroU8,
            inputs: &[Self::NetId],
            output: Self::NetId,
        ) -> ServerResult<Self::CellId> {
            let _ = (client_id, width, inputs, output);
            Err(ServerError::Unsupported)
        }
    };
}

pub trait SimServer {
    type NetId: Copy + Eq + Hash;
    type CellId: Copy + Eq + Hash;

    fn max_clients(&mut self) -> usize {
        1
    }

    fn client_connected(&mut self, client_id: ClientId);
    fn client_disconnected(&mut self, client_id: ClientId);

    fn begin_build(&mut self, client_id: ClientId) -> ServerResult<()>;
    fn end_build(&mut self, client_id: ClientId) -> ServerResult<()>;

    fn add_net(&mut self, client_id: ClientId, width: NonZeroU8) -> ServerResult<Self::NetId>;
    gate_stub!(add_and_gate);
    gate_stub!(add_or_gate);
    gate_stub!(add_xor_gate);
    gate_stub!(add_nand_gate);
    gate_stub!(add_nor_gate);
    gate_stub!(add_xnor_gate);
    fn add_not_gate(
        &mut self,
        client_id: ClientId,
        width: NonZeroU8,
        input: Self::NetId,
        output: Self::NetId,
    ) -> ServerResult<Self::CellId> {
        let _ = (client_id, width, input, output);
        Err(ServerError::Unsupported)
    }

    fn add_mux(
        &mut self,
        client_id: ClientId,
        width: NonZeroU8,
        inputs: &[Self::NetId],
        output: Self::NetId,
    ) -> ServerResult<Self::CellId> {
        let _ = (client_id, width, inputs, output);
        Err(ServerError::Unsupported)
    }

    fn set_net_drive(
        &mut self,
        client_id: ClientId,
        net: Self::NetId,
        bit_plane_0: &[u8],
        bit_plane_1: &[u8],
    ) -> ServerResult<()>;

    fn eval(&mut self, client_id: ClientId, max_steps: u64) -> ServerResult<()>;

    // TODO: instead of asking for each state individually, get some kind of read only view object once
    fn get_net_state(
        &mut self,
        client_id: ClientId,
        net: Self::NetId,
    ) -> ServerResult<(NonZeroU8, &[u8], &[u8])>;
}

#[derive(Debug)]
#[repr(transparent)]
struct NetMap<T> {
    map: Vec<T>,
}

impl<T> Default for NetMap<T> {
    #[inline]
    fn default() -> Self {
        Self { map: Vec::new() }
    }
}

impl<T> NetMap<T> {
    #[inline]
    fn clear(&mut self) {
        self.map.clear();
    }

    fn insert(&mut self, value: T) -> ServerResult<NetId> {
        let index = self
            .map
            .len()
            .checked_add(1)
            .and_then(|index| u32::try_from(index).ok())
            .ok_or(ServerError::OutOfResources)?;

        self.map.push(value);
        Ok(NetId(index))
    }

    #[inline]
    fn values(&self) -> impl Iterator<Item = &T> {
        self.map.iter()
    }
}

impl<T> Index<NetId> for NetMap<T> {
    type Output = T;

    #[inline]
    fn index(&self, id: NetId) -> &Self::Output {
        &self.map[id.0 as usize]
    }
}

#[derive(Debug)]
#[repr(transparent)]
struct CellMap<T> {
    map: Vec<T>,
}

impl<T> Default for CellMap<T> {
    #[inline]
    fn default() -> Self {
        Self { map: Vec::new() }
    }
}

impl<T> CellMap<T> {
    #[inline]
    fn clear(&mut self) {
        self.map.clear();
    }

    fn insert(&mut self, value: T) -> ServerResult<CellId> {
        let index = self
            .map
            .len()
            .checked_add(1)
            .and_then(|index| u32::try_from(index).ok())
            .ok_or(ServerError::OutOfResources)?;

        self.map.push(value);
        Ok(CellId(index))
    }

    #[inline]
    fn values(&self) -> impl Iterator<Item = &T> {
        self.map.iter()
    }
}

impl<T> Index<CellId> for CellMap<T> {
    type Output = T;

    #[inline]
    fn index(&self, id: CellId) -> &Self::Output {
        &self.map[id.0 as usize]
    }
}

fn server_config(max_clients: usize, server_addr: SocketAddr) -> ServerConfig {
    let current_time = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap();

    ServerConfig {
        current_time,
        max_clients,
        protocol_id: PROTOCOL_VERSION,
        public_addresses: vec![server_addr],
        authentication: ServerAuthentication::Unsecure, // TODO: use secure authentication
    }
}

struct AdapterClientState<S: SimServer> {
    net_map: NetMap<S::NetId>,
    cell_map: CellMap<S::CellId>,
    sim_state_order: u64,
}

impl<S: SimServer> Default for AdapterClientState<S> {
    fn default() -> Self {
        Self {
            net_map: NetMap::default(),
            cell_map: CellMap::default(),
            sim_state_order: 0,
        }
    }
}

impl<S: SimServer> AdapterClientState<S> {
    fn reset(&mut self) {
        self.net_map.clear();
        self.cell_map.clear();
    }
}

struct Adapter<S: SimServer> {
    inner: S,
    client_state: HashMap<ClientId, AdapterClientState<S>>,
    net_id_buffer: Vec<S::NetId>,
    sim_state: SimState,
}

impl<S: SimServer> Adapter<S> {
    fn new(inner: S) -> Self {
        Self {
            inner,
            client_state: HashMap::default(),
            net_id_buffer: Vec::new(),
            sim_state: SimState::default(),
        }
    }
}

macro_rules! client_state {
    ($this:ident, $client_id:ident) => {
        $this
            .client_state
            .get(&$client_id)
            .expect("invalid client ID")
    };
    (mut $this:ident, $client_id:ident) => {
        $this
            .client_state
            .get_mut(&$client_id)
            .expect("invalid client ID")
    };
}

macro_rules! gate_impl {
    ($name:ident) => {
        fn $name(
            &mut self,
            client_id: ClientId,
            width: NonZeroU8,
            inputs: &[NetId],
            output: NetId,
        ) -> ServerResult<()> {
            let client_state = client_state!(mut self, client_id);
            self.net_id_buffer.clear();
            self.net_id_buffer
                .extend(inputs.iter().map(|&id| client_state.net_map[id]));
            let output = client_state.net_map[output];
            let cell_id = self
                .inner
                .$name(client_id, width, &self.net_id_buffer, output)?;
            client_state.cell_map.insert(cell_id)?;
            Ok(())
        }
    };
}

impl<S: SimServer> Adapter<S> {
    fn client_connected(&mut self, client_id: ClientId) {
        self.client_state
            .insert(client_id, AdapterClientState::default());
        self.inner.client_connected(client_id);
    }

    fn client_disconnected(&mut self, client_id: ClientId) {
        self.client_state.remove(&client_id);
        self.inner.client_connected(client_id);
    }

    fn begin_build(&mut self, client_id: ClientId) -> ServerResult<()> {
        client_state!(mut self, client_id).reset();
        self.inner.begin_build(client_id)
    }

    fn end_build(&mut self, client_id: ClientId) -> ServerResult<()> {
        self.inner.end_build(client_id)
    }

    fn add_net(&mut self, client_id: ClientId, width: NonZeroU8) -> ServerResult<()> {
        let net_id = self.inner.add_net(client_id, width)?;
        client_state!(mut self, client_id).net_map.insert(net_id)?;
        Ok(())
    }

    gate_impl!(add_and_gate);
    gate_impl!(add_or_gate);
    gate_impl!(add_xor_gate);
    gate_impl!(add_nand_gate);
    gate_impl!(add_nor_gate);
    gate_impl!(add_xnor_gate);

    fn add_not_gate(
        &mut self,
        client_id: ClientId,
        width: NonZeroU8,
        input: NetId,
        output: NetId,
    ) -> ServerResult<()> {
        let client_state = client_state!(mut self, client_id);
        let input = client_state.net_map[input];
        let output = client_state.net_map[output];
        let cell_id = self.inner.add_not_gate(client_id, width, input, output)?;
        client_state.cell_map.insert(cell_id)?;
        Ok(())
    }

    fn add_mux(
        &mut self,
        client_id: ClientId,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<()> {
        let client_state = client_state!(mut self, client_id);
        self.net_id_buffer.clear();
        self.net_id_buffer
            .extend(inputs.iter().map(|&id| client_state.net_map[id]));
        let output = client_state.net_map[output];
        let cell_id = self
            .inner
            .add_mux(client_id, width, &self.net_id_buffer, output)?;
        client_state.cell_map.insert(cell_id)?;
        Ok(())
    }

    fn set_net_drive(
        &mut self,
        client_id: ClientId,
        net: NetId,
        bit_plane_0: &[u8],
        bit_plane_1: &[u8],
    ) -> ServerResult<()> {
        let client_state = client_state!(self, client_id);
        let net = client_state.net_map[net];
        self.inner
            .set_net_drive(client_id, net, bit_plane_0, bit_plane_1)
    }

    #[inline]
    fn eval(&mut self, client_id: ClientId, max_steps: u64) -> ServerResult<()> {
        self.inner.eval(client_id, max_steps)
    }

    fn sim_state(&mut self, client_id: ClientId) -> ServerResult<&SimState> {
        let client_state = client_state!(self, client_id);
        self.sim_state.reset(client_state.sim_state_order);
        for &net in client_state.net_map.values() {
            let (bit_width, bit_plane_0, bit_plane_1) = self.inner.get_net_state(client_id, net)?;
            self.sim_state.push_net(bit_width, bit_plane_0, bit_plane_1);
        }
        Ok(&self.sim_state)
    }
}

fn process_message<S: SimServer>(
    server: &mut RenetServer,
    adapter: &mut Adapter<S>,
    client_id: ClientId,
    message_kind: ClientMessageKind,
) -> ServerResult<()> {
    match message_kind {
        ClientMessageKind::BeginBuild => {
            adapter.begin_build(client_id)?;
        }
        ClientMessageKind::EndBuild => {
            adapter.end_build(client_id)?;
            server.send_command_message(client_id, ServerMessage::BuildingFinished);
        }

        ClientMessageKind::AddNet { width } => {
            adapter.add_net(client_id, width)?;
        }
        ClientMessageKind::AddAndGate {
            width,
            inputs,
            output,
        } => adapter.add_and_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddOrGate {
            width,
            inputs,
            output,
        } => adapter.add_or_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddXorGate {
            width,
            inputs,
            output,
        } => adapter.add_xor_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddNandGate {
            width,
            inputs,
            output,
        } => adapter.add_nand_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddNorGate {
            width,
            inputs,
            output,
        } => adapter.add_nor_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddXnorGate {
            width,
            inputs,
            output,
        } => adapter.add_xnor_gate(client_id, width, &inputs, output)?,
        ClientMessageKind::AddNotGate {
            width,
            input,
            output,
        } => adapter.add_not_gate(client_id, width, input, output)?,

        ClientMessageKind::AddMux {
            width,
            inputs,
            output,
        } => adapter.add_mux(client_id, width, &inputs, output)?,

        ClientMessageKind::SetNetDrive {
            net,
            bit_plane_0,
            bit_plane_1,
        } => {
            adapter.set_net_drive(client_id, net, &bit_plane_0, &bit_plane_1)?;
        }

        ClientMessageKind::Eval { max_steps } => {
            adapter.eval(client_id, max_steps)?;
            let sim_state = adapter.sim_state(client_id)?.clone(); // TODO: avoid cloning
            server.send_command_message(client_id, ServerMessage::Report(sim_state));
        }
        ClientMessageKind::QueryReport => {
            let sim_state = adapter.sim_state(client_id)?.clone(); // TODO: avoid cloning
            server.send_command_message(client_id, ServerMessage::Report(sim_state));
        }
        ClientMessageKind::QueryUpdate => {
            let sim_state = adapter.sim_state(client_id)?;
            server.send_sim_state(client_id, sim_state);
        }
    }

    Ok(())
}

pub fn run_server<S: SimServer>(
    port: Option<u16>,
    mut sim_server: S,
) -> Result<(), NetcodeTransportError> {
    let mut server = RenetServer::new(common_config());

    let server_addr: SocketAddr = (Ipv4Addr::UNSPECIFIED, port.unwrap_or(DEFAULT_PORT)).into();
    let socket = UdpSocket::bind(server_addr)?;
    let config = server_config(sim_server.max_clients().min(1024), server_addr);
    let mut transport = NetcodeServerTransport::new(config, socket)?;

    println!("server listening on {server_addr}");

    let mut adapter = Adapter::new(sim_server);
    let mut client_ids = Vec::new();

    let mut prev_time = Instant::now();
    loop {
        let current_time = Instant::now();
        let delta = prev_time - current_time;
        prev_time = current_time;

        server.update(delta);
        transport.update(delta, &mut server)?;

        while let Some(event) = server.get_event() {
            match event {
                ServerEvent::ClientConnected { client_id } => {
                    println!("client {client_id} connected");
                    adapter.client_connected(client_id);

                    server.send_command_message(client_id, ServerMessage::Ready);
                }
                ServerEvent::ClientDisconnected { client_id, .. } => {
                    println!("client {client_id} disconnected");
                    adapter.client_disconnected(client_id);
                }
            }
        }

        client_ids.clear();
        client_ids.extend(server.clients_id_iter());
        for &client_id in &client_ids {
            while let Some(message) = server.receive_message(client_id, COMMAND_CHANNEL_ID) {
                let message: ClientMessage =
                    rmp_serde::from_slice(&message).expect("invalid client message");

                if let Err(error) =
                    process_message(&mut server, &mut adapter, client_id, message.kind)
                {
                    server.send_command_message(
                        client_id,
                        ServerMessage::Error {
                            id: message.id,
                            error,
                        },
                    );
                }
            }
        }

        transport.send_packets(&mut server);
        std::thread::sleep(Duration::from_millis(1));
    }
}
