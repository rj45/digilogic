use digilogic_netcode::*;
use gsim::*;
use std::num::NonZeroU8;

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

fn update_sim_state(simulator: &Simulator, sim_state: &mut SimState) {
    sim_state.reset();
    // TODO
}

#[derive(Default)]
#[allow(missing_debug_implementations)]
pub struct GsimServer {
    clients: ahash::AHashMap<ClientId, ClientState>,
}

impl GsimServer {
    fn get_glient_state_mut(&mut self, client_id: ClientId) -> &mut ClientState {
        self.clients.get_mut(&client_id).expect("invalid client ID")
    }
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

    fn begin_build(&mut self, client_id: ClientId) -> ServerResult<()> {
        let client_state = self.get_glient_state_mut(client_id);
        *client_state = ClientState::default();
        Ok(())
    }

    fn end_build(&mut self, client_id: ClientId) -> ServerResult<()> {
        let client_state = self.get_glient_state_mut(client_id);
        *client_state = match client_state {
            ClientState::Building { builder } => ClientState::Simulating {
                simulator: std::mem::take(builder).build(),
                sim_state: SimState::default(),
            },
            ClientState::Simulating { .. } => {
                return Err(ServerError::InvalidState);
            }
        };
        Ok(())
    }

    fn add_net(&mut self, width: NonZeroU8) -> ServerResult<NetId> {
        todo!()
    }

    fn add_and_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_or_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_xor_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_nand_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_nor_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_xnor_gate(
        &mut self,
        width: NonZeroU8,
        inputs: &[NetId],
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn add_not_gate(
        &mut self,
        width: NonZeroU8,
        input: NetId,
        output: NetId,
    ) -> ServerResult<CellId> {
        todo!()
    }

    fn sim_state(&mut self, client_id: ClientId) -> ServerResult<Option<&SimState>> {
        let client_state = self.get_glient_state_mut(client_id);
        match client_state {
            ClientState::Building { .. } => Ok(None),
            ClientState::Simulating {
                simulator,
                sim_state,
            } => {
                update_sim_state(simulator, sim_state);
                Ok(Some(sim_state))
            }
        }
    }
}
