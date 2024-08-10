use super::{PointerButtonEvent, PointerMovedEvent};
use crate::ux::states::*;
use bevy_ecs::prelude::*;
use bevy_log::debug;
use digilogic_core::components::{CircuitID, Hovered, Viewport};
use digilogic_core::spatial_index::SpatialIndex;
use digilogic_core::transform::{BoundingBox, Vec2};
use digilogic_core::{fixed, Fixed};

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

const MOUSE_POS_FUDGE: Fixed = fixed!(2);

fn hover_system(
    trigger: Trigger<PointerMovedEvent>,
    mut commands: Commands,
    spatial_index: Res<SpatialIndex>,
    hover_query: Query<Entity, With<Hovered>>,
    mut found_hovered: Local<Vec<Entity>>,
) {
    let position = trigger.event().0;
    let bounds = BoundingBox::from_center_half_size(
        Vec2 {
            x: Fixed::try_from_f32(position.x).unwrap(),
            y: Fixed::try_from_f32(position.y).unwrap(),
        },
        MOUSE_POS_FUDGE,
        MOUSE_POS_FUDGE,
    );
    found_hovered.clear();
    spatial_index.query(bounds, |entity| {
        found_hovered.push(*entity);
        commands.entity(*entity).insert(Hovered);
    });
    for item in hover_query.iter() {
        if !found_hovered.contains(&item) {
            commands.entity(item).remove::<Hovered>();
        }
    }
}
