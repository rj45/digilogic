use crate::*;
use std::net::Ipv4Addr;
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
            inputs: &[NetId],
            output: NetId,
        ) -> ServerResult<CellId> {
            let _ = (client_id, width, inputs, output);
            Err(ServerError::Unsupported)
        }
    };
}

pub trait SimServer {
    fn max_clients(&mut self) -> usize {
        1
    }

    fn client_connected(&mut self, client_id: ClientId);
    fn client_disconnected(&mut self, client_id: ClientId);

    fn begin_build(&mut self, client_id: ClientId) -> ServerResult<()>;
    fn end_build(&mut self, client_id: ClientId) -> ServerResult<()>;

    fn add_net(&mut self, client_id: ClientId, width: NonZeroU8) -> ServerResult<NetId>;
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
        input: NetId,
        output: NetId,
    ) -> ServerResult<CellId> {
        let _ = (client_id, width, input, output);
        Err(ServerError::Unsupported)
    }

    fn sim_state(&mut self, client_id: ClientId) -> ServerResult<Option<&SimState>>;
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

fn process_message(
    server: &mut RenetServer,
    sim_server: &mut impl SimServer,
    client_id: ClientId,
    response_id: ServerMessageId,
    message_kind: ClientMessageKind,
    net_offset: &mut u64,
) -> ServerResult<()> {
    macro_rules! add_gate {
        ($add_fn:ident, $width:ident, $inputs:ident, $output:ident) => {{
            let id = sim_server.$add_fn(client_id, $width, &$inputs, $output)?;
            server.send_command_message(
                client_id,
                ServerMessage {
                    id: response_id,
                    kind: ServerMessageKind::CellAdded { id },
                },
            );
        }};
    }

    match message_kind {
        ClientMessageKind::BeginBuild => {
            sim_server.begin_build(client_id)?;
            *net_offset = 0;
        }
        ClientMessageKind::EndBuild => {
            sim_server.end_build(client_id)?;
            server.send_command_message(
                client_id,
                ServerMessage {
                    id: response_id,
                    kind: ServerMessageKind::BuildingFinished,
                },
            );
        }

        ClientMessageKind::AddNet { width } => {
            let id = sim_server.add_net(client_id, width)?;
            server.send_command_message(
                client_id,
                ServerMessage {
                    id: response_id,
                    kind: ServerMessageKind::NetAdded {
                        id,
                        offset: *net_offset,
                    },
                },
            );
            *net_offset += width.get() as u64;
        }
        ClientMessageKind::AddAndGate {
            width,
            inputs,
            output,
        } => add_gate!(add_and_gate, width, inputs, output),
        ClientMessageKind::AddOrGate {
            width,
            inputs,
            output,
        } => add_gate!(add_or_gate, width, inputs, output),
        ClientMessageKind::AddXorGate {
            width,
            inputs,
            output,
        } => add_gate!(add_xor_gate, width, inputs, output),
        ClientMessageKind::AddNandGate {
            width,
            inputs,
            output,
        } => add_gate!(add_nand_gate, width, inputs, output),
        ClientMessageKind::AddNorGate {
            width,
            inputs,
            output,
        } => add_gate!(add_nor_gate, width, inputs, output),
        ClientMessageKind::AddXnorGate {
            width,
            inputs,
            output,
        } => add_gate!(add_xnor_gate, width, inputs, output),
        ClientMessageKind::AddNotGate {
            width,
            input,
            output,
        } => {
            let id = sim_server.add_not_gate(client_id, width, input, output)?;
            server.send_command_message(
                client_id,
                ServerMessage {
                    id: response_id,
                    kind: ServerMessageKind::CellAdded { id },
                },
            );
        }

        ClientMessageKind::QuerySimState => {
            if let Some(sim_state) = sim_server.sim_state(client_id)? {
                server.send_sim_state(client_id, sim_state);
            }
        }
    }

    Ok(())
}

pub fn run_server(
    port: Option<u16>,
    mut sim_server: impl SimServer,
) -> Result<(), NetcodeTransportError> {
    let mut server = RenetServer::new(common_config());

    let server_addr: SocketAddr = (Ipv4Addr::UNSPECIFIED, port.unwrap_or(DEFAULT_PORT)).into();
    let socket = UdpSocket::bind(server_addr)?;
    let config = server_config(sim_server.max_clients().min(1024), server_addr);
    let mut transport = NetcodeServerTransport::new(config, socket)?;

    let mut prev_time = Instant::now();
    let mut client_ids = Vec::new();
    let mut net_offset = 0;
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
                    sim_server.client_connected(client_id);

                    server.send_command_message(
                        client_id,
                        ServerMessage {
                            id: ServerMessageId::Status,
                            kind: ServerMessageKind::Ready,
                        },
                    );
                }
                ServerEvent::ClientDisconnected { client_id, .. } => {
                    println!("client {client_id} disconnected");
                    sim_server.client_disconnected(client_id);
                }
            }
        }

        client_ids.clear();
        client_ids.extend(server.clients_id_iter());
        for &client_id in &client_ids {
            while let Some(message) = server.receive_message(client_id, COMMAND_CHANNEL_ID) {
                let message: ClientMessage =
                    rmp_serde::from_slice(&message).expect("invalid client message");

                let response_id = ServerMessageId::Response(message.id);
                if let Err(err) = process_message(
                    &mut server,
                    &mut sim_server,
                    client_id,
                    response_id,
                    message.kind,
                    &mut net_offset,
                ) {
                    server.send_command_message(
                        client_id,
                        ServerMessage {
                            id: response_id,
                            kind: ServerMessageKind::Error(err),
                        },
                    );
                }
            }
        }

        transport.send_packets(&mut server);
        std::thread::sleep(Duration::from_millis(1));
    }
}
