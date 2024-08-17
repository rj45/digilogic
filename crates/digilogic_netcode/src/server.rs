use crate::*;
use std::net::Ipv4Addr;
use std::time::Instant;

pub use renet::ClientId;

pub trait SimServer {
    fn max_clients(&mut self) -> usize {
        1
    }

    fn client_connected(&mut self, client_id: ClientId);
    fn client_disconnected(&mut self, client_id: ClientId);

    fn sim_state(&mut self, client_id: ClientId) -> &mut SimState;
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

pub fn run_server(port: u16, mut sim_server: impl SimServer) -> Result<(), NetcodeTransportError> {
    let mut server = RenetServer::new(common_config());

    let server_addr: SocketAddr = (Ipv4Addr::UNSPECIFIED, port).into();
    let socket = UdpSocket::bind(server_addr)?;
    let config = server_config(sim_server.max_clients(), server_addr);
    let mut transport = NetcodeServerTransport::new(config, socket)?;

    let mut prev_time = Instant::now();
    let mut client_ids = Vec::new();
    let mut message_order = 0;
    loop {
        let current_time = Instant::now();
        let delta = prev_time - current_time;
        prev_time = current_time;

        server.update(delta);
        transport.update(delta, &mut server)?;

        while let Some(event) = server.get_event() {
            match event {
                ServerEvent::ClientConnected { client_id } => {
                    sim_server.client_connected(client_id)
                }
                ServerEvent::ClientDisconnected { client_id, .. } => {
                    sim_server.client_disconnected(client_id)
                }
            }
        }

        client_ids.clear();
        client_ids.extend(server.clients_id_iter());
        for &client_id in &client_ids {
            while let Some(message) = server.receive_message(client_id, COMMAND_CHANNEL_ID) {
                let message: ClientCommandMessage =
                    rmp_serde::from_slice(&message).expect("invalid client message");

                match message {
                    ClientCommandMessage::Placeholder => (),
                }
            }

            let sim_state = sim_server.sim_state(client_id);
            for message in sim_state.data_messages(&mut message_order) {
                let message = rmp_serde::to_vec(&message).unwrap();
                server.send_message(client_id, DATA_CHANNEL_ID, message);
            }
        }

        transport.send_packets(&mut server);

        let current_time = Instant::now();
        let delta = prev_time - current_time;
        let target_delta = Duration::from_secs_f64(1.0 / (TARGET_TICK_RATE as f64));
        let wait_time = target_delta.saturating_sub(delta);
        spin_sleep::sleep(wait_time);
    }
}
