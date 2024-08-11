use super::{
    EntityOffset, HoveredEntity, MouseIdle, MouseMoving, MouseState, PointerButtonEvent,
    PointerMovedEvent,
};
use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::spatial_index::SpatialIndex;
use digilogic_core::transform::{BoundingBox, Transform, Vec2};
use digilogic_core::Fixed;

/// Called when a new viewport is added to the world.
pub(crate) fn on_add_viewport_augment_with_fsm(
    trigger: Trigger<OnAdd, Viewport>,
    mut commands: Commands,
) {
    commands
        .entity(trigger.entity())
        .insert(HoveredEntity::default())
        .insert(MouseState::Idle)
        .observe(hover_system)
        .observe(mouse_state_transition_system)
        .observe(mover_system);
}

fn mouse_state_transition_system(
    trigger: Trigger<PointerButtonEvent>,
    hover_query: Query<&HoveredEntity>,
    mut mouse_state_query: Query<&mut MouseState>,
    transform_query: Query<(&Transform, Has<Port>)>,
    mut commands: Commands,
) {
    let viewport = trigger.entity();
    let event = trigger.event();
    let hovered_entity = hover_query.get(viewport).unwrap();
    let mut mouse_state = mouse_state_query.get_mut(viewport).unwrap();
    match (mouse_state.as_ref(), event.pressed) {
        (MouseState::Idle, true) => {
            if let Some(hovered_entity) = hovered_entity.0 {
                let mouse_pos = Vec2 {
                    x: Fixed::try_from_f32(event.pos.x).unwrap(),
                    y: Fixed::try_from_f32(event.pos.y).unwrap(),
                };

                let mut offset_list = Vec::new();
                if let Ok((transform, is_port)) = transform_query.get(hovered_entity) {
                    if is_port {
                        // TODO: enter wire drawing mode
                    } else {
                        offset_list.push(EntityOffset {
                            entity: hovered_entity,
                            offset: transform.translation - mouse_pos,
                        });
                    }
                }

                if !offset_list.is_empty() {
                    *mouse_state = MouseState::Moving(offset_list.clone());
                    commands.entity(viewport).remove::<MouseIdle>();
                    commands.entity(viewport).insert(MouseMoving(offset_list));
                }
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

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u8)]
enum HoveredEntityKind {
    #[default]
    Other,
    Waypoint,
    Endpoint,
    Port,
}

fn hover_system(
    trigger: Trigger<PointerMovedEvent>,
    mut commands: Commands,
    spatial_index: Res<SpatialIndex>,
    entity_kind_query: Query<(Has<Port>, Has<Endpoint>, Has<Waypoint>)>,
    mut current_hovered_entity: Query<&mut HoveredEntity>,
) {
    let position = trigger.event().0;
    let viewport = trigger.entity();
    let bounds = BoundingBox::from_center_half_size(
        Vec2 {
            x: Fixed::try_from_f32(position.x).unwrap(),
            y: Fixed::try_from_f32(position.y).unwrap(),
        },
        Fixed::EPSILON,
        Fixed::EPSILON,
    );

    let mut new_hovered_entity = None;
    let mut new_hovered_entity_kind = HoveredEntityKind::default();
    spatial_index.query(bounds, |&entity| {
        let (is_port, is_endpoint, is_waypoint) = entity_kind_query.get(entity).unwrap_or_default();
        let kind = match (is_port, is_endpoint, is_waypoint) {
            (true, _, _) => HoveredEntityKind::Port,
            (_, true, _) => HoveredEntityKind::Endpoint,
            (_, _, true) => HoveredEntityKind::Waypoint,
            _ => HoveredEntityKind::Other,
        };

        if kind >= new_hovered_entity_kind {
            new_hovered_entity = Some(entity);
            new_hovered_entity_kind = kind;
        }
    });

    let mut current_hovered_entity = current_hovered_entity.get_mut(viewport).unwrap();
    if new_hovered_entity != current_hovered_entity.0 {
        if let Some(current_hovered_entity) = current_hovered_entity.0 {
            commands.entity(current_hovered_entity).remove::<Hovered>();
        }
        if let Some(new_hovered_entity) = new_hovered_entity {
            commands.entity(new_hovered_entity).insert(Hovered);
        }

        current_hovered_entity.0 = new_hovered_entity;
    }
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
