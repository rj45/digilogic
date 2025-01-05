use digilogic_netcode::*;
use gsim::*;
use std::num::NonZeroU8;

enum ClientState {
    Building(SimulatorBuilder),
    Simulating(Simulator),
}

impl Default for ClientState {
    fn default() -> Self {
        Self::Building(SimulatorBuilder::default())
    }
}

#[derive(Default)]
#[allow(missing_debug_implementations)]
pub struct GsimServer {
    clients: ahash::AHashMap<ClientId, ClientState>,
    bit_plane_0: [u8; 32],
    bit_plane_1: [u8; 32],
}

impl GsimServer {
    fn get_client_state(&self, client_id: ClientId) -> &ClientState {
        self.clients.get(&client_id).expect("invalid client ID")
    }

    fn get_client_state_mut(&mut self, client_id: ClientId) -> &mut ClientState {
        self.clients.get_mut(&client_id).expect("invalid client ID")
    }

    fn get_builder_mut(&mut self, client_id: ClientId) -> ServerResult<&mut SimulatorBuilder> {
        match self.get_client_state_mut(client_id) {
            ClientState::Building(builder) => Ok(builder),
            ClientState::Simulating(_) => Err(ServerError::InvalidState),
        }
    }

    fn get_simulator(&self, client_id: ClientId) -> ServerResult<&Simulator> {
        match self.get_client_state(client_id) {
            ClientState::Building(_) => Err(ServerError::InvalidState),
            ClientState::Simulating(simulator) => Ok(simulator),
        }
    }

    fn get_simulator_mut(&mut self, client_id: ClientId) -> ServerResult<&mut Simulator> {
        match self.get_client_state_mut(client_id) {
            ClientState::Building(_) => Err(ServerError::InvalidState),
            ClientState::Simulating(simulator) => Ok(simulator),
        }
    }
}

fn component_error_to_server_error(error: AddComponentError) -> ServerError {
    match error {
        AddComponentError::TooManyComponents => ServerError::OutOfResources,
        AddComponentError::InvalidWireId => ServerError::InvalidNetId,
        AddComponentError::WireWidthMismatch => ServerError::WidthMismatch,
        AddComponentError::WireWidthIncompatible => ServerError::WidthIncompatible,
        AddComponentError::OffsetOutOfRange => ServerError::OutOfRange,
        AddComponentError::TooFewInputs => ServerError::InvalidInputCount,
        AddComponentError::InvalidInputCount => ServerError::InvalidInputCount,
        _ => ServerError::Other,
    }
}

fn simulation_result_to_server_result(result: SimulationRunResult) -> ServerResult<()> {
    match result {
        SimulationRunResult::Ok => Ok(()),
        SimulationRunResult::MaxStepsReached => Err(ServerError::MaxStepsReached),
        SimulationRunResult::Err(_) => Err(ServerError::DriverConflict),
    }
}

macro_rules! gate_impl {
    ($name:ident) => {
        fn $name(
            &mut self,
            client_id: ClientId,
            width: NonZeroU8,
            inputs: &[Self::NetId],
            output: Self::NetId,
        ) -> ServerResult<Self::CellId> {
            let builder = self.get_builder_mut(client_id)?;

            let output_width = builder
                .get_wire_width(output)
                .map_err(|_| ServerError::InvalidNetId)?;
            if width != output_width {
                return Err(ServerError::WidthMismatch);
            }

            builder
                .$name(inputs, output)
                .map_err(component_error_to_server_error)
        }
    };
}

impl SimServer for GsimServer {
    type NetId = WireId;
    type CellId = ComponentId;

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
        let client_state = self.get_client_state_mut(client_id);
        *client_state = ClientState::default();
        Ok(())
    }

    fn end_build(&mut self, client_id: ClientId) -> ServerResult<()> {
        let client_state = self.get_client_state_mut(client_id);
        match client_state {
            ClientState::Building(builder) => {
                let simulator = std::mem::take(builder).build();
                *client_state = ClientState::Simulating(simulator);
                Ok(())
            }
            ClientState::Simulating { .. } => Err(ServerError::InvalidState),
        }
    }

    fn add_net(&mut self, client_id: ClientId, width: NonZeroU8) -> ServerResult<Self::NetId> {
        self.get_builder_mut(client_id)?
            .add_wire(width)
            .ok_or(ServerError::OutOfResources)
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
        input: Self::NetId,
        output: Self::NetId,
    ) -> ServerResult<Self::CellId> {
        let builder = self.get_builder_mut(client_id)?;

        let output_width = builder
            .get_wire_width(output)
            .map_err(|_| ServerError::InvalidNetId)?;
        if width != output_width {
            return Err(ServerError::WidthMismatch);
        }

        builder
            .add_not_gate(input, output)
            .map_err(component_error_to_server_error)
    }

    fn add_mux(
        &mut self,
        client_id: ClientId,
        width: NonZeroU8,
        inputs: &[Self::NetId],
        output: Self::NetId,
    ) -> ServerResult<Self::CellId> {
        let builder = self.get_builder_mut(client_id)?;

        let output_width = builder
            .get_wire_width(output)
            .map_err(|_| ServerError::InvalidNetId)?;
        if width != output_width {
            return Err(ServerError::WidthMismatch);
        }

        // TODO: probably better to not pack select inside the inputs, I was being lazy
        let select = inputs[0];
        let inputs = &inputs[1..];

        builder
            .add_multiplexer(inputs, select, output)
            .map_err(component_error_to_server_error)
    }

    fn set_net_drive(
        &mut self,
        client_id: ClientId,
        net: Self::NetId,
        bit_plane_0: &[u8],
        bit_plane_1: &[u8],
    ) -> ServerResult<()> {
        let simulator = self.get_simulator_mut(client_id)?;

        let mut words0 = [0u32; 8];
        let mut words1 = [0u32; 8];
        bytemuck::cast_slice_mut(&mut words0)[..bit_plane_0.len()].copy_from_slice(bit_plane_0);
        bytemuck::cast_slice_mut(&mut words1)[..bit_plane_1.len()].copy_from_slice(bit_plane_1);

        let word_count = bit_plane_0.len().min(bit_plane_1.len()).div_ceil(4);
        let new_drive = LogicState::from_bit_planes(&words0[..word_count], &words1[..word_count]);

        simulator
            .set_wire_drive(net, &new_drive)
            .map_err(|_| ServerError::InvalidNetId)
    }

    fn eval(&mut self, client_id: ClientId, max_steps: u64) -> ServerResult<()> {
        let simulator = self.get_simulator_mut(client_id)?;
        simulation_result_to_server_result(simulator.run_sim(max_steps))
    }

    fn get_net_state(
        &mut self,
        client_id: ClientId,
        net: Self::NetId,
    ) -> ServerResult<(NonZeroU8, &[u8], &[u8])> {
        let simulator = self.get_simulator(client_id)?;

        let bit_width = simulator.get_wire_width(net).unwrap();
        let state = simulator.get_wire_state(net).unwrap();
        let (bit_plane_0, bit_plane_1) = state.to_bit_planes(bit_width);

        let bit_plane_0: &[u8] = bytemuck::cast_slice(&bit_plane_0);
        self.bit_plane_0[..bit_plane_0.len()].copy_from_slice(bit_plane_0);
        let bit_plane_1: &[u8] = bytemuck::cast_slice(&bit_plane_1);
        self.bit_plane_1[..bit_plane_1.len()].copy_from_slice(bit_plane_1);

        Ok((bit_width, &self.bit_plane_0, &self.bit_plane_1))
    }
}
