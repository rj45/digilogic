use crate::ux::events::InputEvent;
use crate::ux::states::*;
use bevy_ecs::prelude::*;

use digilogic_core::components::Viewport;

#[derive(Component, Debug, Clone, Copy, PartialEq, Default)]
pub(crate) struct MouseFSM {
    pub state: RootMouseState,
}

pub(crate) fn on_add_viewport_augment_with_fsm(
    trigger: Trigger<OnAdd, Viewport>,
    mut commands: Commands,
) {
    commands
        .entity(trigger.entity())
        .insert(MouseFSM::default());
}

pub(crate) fn root_fsm_system(
    mut mouse_fsm: Query<(Entity, &mut MouseFSM)>,
    mut input_events: EventReader<InputEvent>,
) {
    for input_event in input_events.read() {
        let viewport = input_event.viewport;

        for (fsm_viewport, mut mouse_fsm) in mouse_fsm.iter_mut() {
            if viewport != fsm_viewport {
                continue;
            }
        }
    }
}
