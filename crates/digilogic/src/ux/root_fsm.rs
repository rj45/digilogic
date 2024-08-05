use crate::ux::events::InputEvent;
use crate::ux::states::*;
use bevy_ecs::prelude::*;
use log::debug;

use digilogic_core::components::Viewport;

#[derive(Component, Debug, Clone, Copy, PartialEq, Default)]
pub(crate) struct MouseFSM {
    pub state: RootMouseState,
}

pub(crate) fn on_add_viewport_augment_with_fsm(
    trigger: Trigger<OnAdd, Viewport>,
    mut commands: Commands,
) {
    debug!("on_add_viewport_augment_with_fsm: {:?}", trigger.entity());
    commands
        .entity(trigger.entity())
        .insert(MouseFSM::default())
        .observe(root_fsm_system);
}

pub(crate) fn root_fsm_system(
    trigger: Trigger<InputEvent>,
    mut mouse_fsm: Query<(Entity, &mut MouseFSM)>,
) {
    let viewport = trigger.entity();
    let mut mouse_fsm = mouse_fsm.get_mut(viewport).unwrap();

    debug!(
        "MouseFSM: {:?} InputEvent: {:?}",
        mouse_fsm.1.state,
        trigger.event()
    );
}
