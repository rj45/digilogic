use bevy_reflect::prelude::*;
use bevy_state::prelude::*;

/// The current state of the simulation.  
/// Only the simulation driver should transition this.
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect, States)]
pub enum SimulationState {
    /// The simulation is not connected to a server
    #[default]
    Disconnected,
    /// The simulation is waiting on the server to be ready
    WaitingOnServer,
    /// The simulation graph is being built
    Building,
    /// The simulation is active
    ActiveIdle,
    /// The simulation is active and free-running
    ActiveRunning,
}

impl SimulationState {
    #[inline]
    pub fn is_connected(self) -> bool {
        !matches!(self, SimulationState::Disconnected)
    }

    #[inline]
    pub fn is_active(self) -> bool {
        matches!(
            self,
            SimulationState::ActiveIdle | SimulationState::ActiveRunning,
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect)]
pub struct SimulationConnected;

impl ComputedStates for SimulationConnected {
    type SourceStates = SimulationState;

    fn compute(sources: Self::SourceStates) -> Option<Self> {
        if sources.is_connected() {
            Some(Self)
        } else {
            None
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect)]
pub struct SimulationActive;

impl ComputedStates for SimulationActive {
    type SourceStates = SimulationState;

    fn compute(sources: Self::SourceStates) -> Option<Self> {
        if sources.is_active() {
            Some(Self)
        } else {
            None
        }
    }
}
