use crate::ux::states::*;
use bevy_ecs::prelude::*;
use log::debug;

use digilogic_core::{
    components::Viewport,
    spatial_index::SpatialIndex,
    transform::{BoundingBox, Vec2i},
};

use super::{PointerButtonEvent, PointerMovedEvent};

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
        .observe(root_fsm_system)
        .observe(hover_system);
}

fn root_fsm_system(
    trigger: Trigger<PointerButtonEvent>,
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

const MOUSE_POS_FUDGE: u32 = 2;

fn hover_system(
    trigger: Trigger<PointerMovedEvent>,
    commands: Commands,
    spatial_index: Res<SpatialIndex>,
) {
    let viewport = trigger.entity();
    let position = trigger.event().0;
    let bounds = BoundingBox::from_center_half_size(
        Vec2i {
            x: position.x as i32,
            y: position.y as i32,
        },
        MOUSE_POS_FUDGE,
        MOUSE_POS_FUDGE,
    );
    spatial_index.query(bounds, |entity| {
        debug!("Hovering over entity: {} at {:?}", entity, bounds);
    });
}
