use digilogic_netcode::*;
use gsim::*;

enum ClientState {
    Building {
        builder: SimulatorBuilder,
    },
    Simulating {
        simulator: Simulator,
        sim_state: SimState,
    },
}

impl Default for ClientState {
    fn default() -> Self {
        Self::Building {
            builder: SimulatorBuilder::default(),
        }
    }
}

#[derive(Default)]
struct GsimServer {
    clients: ahash::AHashMap<ClientId, ClientState>,
}

impl SimServer for GsimServer {
    fn max_clients(&mut self) -> usize {
        usize::MAX
    }

    fn client_connected(&mut self, client_id: ClientId) {
        self.clients.insert(client_id, ClientState::default());
    }

    fn client_disconnected(&mut self, client_id: ClientId) {
        self.clients.remove(&client_id);
    }

    fn sim_state(&mut self, client_id: ClientId) -> Option<&mut SimState> {
        self.clients
            .get_mut(&client_id)
            .and_then(|state| match state {
                ClientState::Building { .. } => None,
                ClientState::Simulating { sim_state, .. } => Some(sim_state),
            })
    }
}

fn main() {
    run_server(42069, GsimServer::default()).unwrap()
}
