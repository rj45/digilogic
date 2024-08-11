use super::{
    EntityOffset, HoveredList, MouseIdle, MouseMoving, MouseState, PointerButtonEvent,
    PointerMovedEvent,
};
use bevy_ecs::prelude::*;
use digilogic_core::components::{Hovered, Viewport};
use digilogic_core::spatial_index::SpatialIndex;
use digilogic_core::transform::{BoundingBox, Transform, Vec2};
use digilogic_core::{fixed, Fixed};

/// Called when a new viewport is added to the world.
pub(crate) fn on_add_viewport_augment_with_fsm(
    trigger: Trigger<OnAdd, Viewport>,
    mut commands: Commands,
) {
    commands
        .entity(trigger.entity())
        .insert(HoveredList(Vec::new()))
        .insert(MouseState::Idle)
        .observe(hover_system)
        .observe(mouse_state_transition_system)
        .observe(mover_system);
}

fn mouse_state_transition_system(
    trigger: Trigger<PointerButtonEvent>,
    hover_query: Query<&HoveredList>,
    mut mouse_state_query: Query<&mut MouseState>,
    transform_query: Query<&Transform>,
    mut commands: Commands,
) {
    let viewport = trigger.entity();
    let event = trigger.event();
    let hover_list = hover_query.get(viewport).unwrap();
    let mut mouse_state = mouse_state_query.get_mut(viewport).unwrap();
    match (mouse_state.as_ref(), event.pressed) {
        (MouseState::Idle, true) => {
            if hover_list.len() > 0 {
                let mouse_pos = Vec2 {
                    x: Fixed::try_from_f32(event.pos.x).unwrap(),
                    y: Fixed::try_from_f32(event.pos.y).unwrap(),
                };
                let mut offset_list = Vec::new();
                for entity in hover_list.0.iter() {
                    if let Ok(transform) = transform_query.get(*entity) {
                        offset_list.push(EntityOffset {
                            entity: *entity,
                            offset: transform.translation - mouse_pos,
                        });
                    }
                }

                *mouse_state = MouseState::Moving(offset_list.clone());
                commands.entity(viewport).remove::<MouseIdle>();
                commands
                    .entity(viewport)
                    .insert(MouseMoving(offset_list.clone()));
            }
        }
        (MouseState::Moving(..), false) => {
            *mouse_state = MouseState::Idle;
            commands.entity(viewport).remove::<MouseMoving>();
            commands.entity(viewport).insert(MouseIdle);
        }
        _ => {}
    }
}

const MOUSE_POS_FUDGE: Fixed = fixed!(2);

fn hover_system(
    trigger: Trigger<PointerMovedEvent>,
    mut commands: Commands,
    spatial_index: Res<SpatialIndex>,
    hover_query: Query<Entity, With<Hovered>>,
    mut found_hovered: Local<Vec<Entity>>,
    mut hovered_list: Query<&mut HoveredList>,
) {
    let position = trigger.event().0;
    let viewport = trigger.entity();
    let bounds = BoundingBox::from_center_half_size(
        Vec2 {
            x: Fixed::try_from_f32(position.x).unwrap(),
            y: Fixed::try_from_f32(position.y).unwrap(),
        },
        MOUSE_POS_FUDGE,
        MOUSE_POS_FUDGE,
    );
    found_hovered.clear();
    let mut list = hovered_list.get_mut(viewport).unwrap();
    spatial_index.query(bounds, |entity| {
        found_hovered.push(*entity);
        commands.entity(*entity).insert(Hovered);

        if !list.iter().any(|subject| *subject == *entity) {
            list.push(*entity);
        }
    });
    for item in hover_query.iter() {
        if !found_hovered.contains(&item) {
            commands.entity(item).remove::<Hovered>();
        }
    }
    list.retain(|item| found_hovered.contains(item));
}

fn mover_system(
    trigger: Trigger<PointerMovedEvent>,
    moving_query: Query<&MouseMoving>,
    mut transform_query: Query<&mut Transform>,
) {
    let position = trigger.event().0;
    let viewport = trigger.entity();
    if let Ok(moving) = moving_query.get(viewport) {
        for entity_offset in moving.0.iter() {
            if let Ok(mut transform) = transform_query.get_mut(entity_offset.entity) {
                transform.translation = Vec2 {
                    x: Fixed::try_from_f32(position.x).unwrap() + entity_offset.offset.x,
                    y: Fixed::try_from_f32(position.y).unwrap() + entity_offset.offset.y,
                };
            }
        }
    }
}
