use crate::ux::states::*;
use bevy_ecs::prelude::*;

#[derive(Component, Debug, Clone, Copy, PartialEq, Default)]
pub struct MouseFSM {
    pub state: RootMouseState,
}
