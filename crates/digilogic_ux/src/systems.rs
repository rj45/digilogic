use super::{EntityOffset, HoveredEntity, MouseIdle, MouseMoving, MouseState};
use crate::spatial_index::SpatialIndex;
use crate::{DragEvent, DragType, HoverEvent, PointerButton};
use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::transform::{BoundingBox, Transform};
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
        .observe(mouse_drag_system);
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u8)]
enum HoveredEntityKind {
    #[default]
    Other,
    Net,
    Endpoint,
    Port,
}

type EntityKindQuery<'w, 's> = Query<'w, 's, (Has<Port>, Has<Endpoint>, Has<Net>)>;

fn hover_system(
    trigger: Trigger<HoverEvent>,
    mut commands: Commands,
    circuits: Query<&SpatialIndex, With<Circuit>>,
    entity_kind_query: EntityKindQuery,
    mut current_hovered_entity: Query<&mut HoveredEntity>,
) {
    let spatial_index = circuits
        .get(trigger.event().circuit.0)
        .expect("invalid circuit ID");

    let position = trigger.event().pos;
    let viewport = trigger.entity();
    let bounds = BoundingBox::from_center_half_size(position, Fixed::EPSILON, Fixed::EPSILON);

    let mut new_hovered_entity = None;
    let mut new_hovered_entity_kind = HoveredEntityKind::default();
    spatial_index.query(bounds, |&entity| {
        let (is_port, is_endpoint, is_net) = entity_kind_query.get(entity).unwrap_or_default();
        let kind = match (is_port, is_endpoint, is_net) {
            (true, _, _) => HoveredEntityKind::Port,
            (_, true, _) => HoveredEntityKind::Endpoint,
            (_, _, true) => HoveredEntityKind::Net,
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

fn mouse_drag_system(
    trigger: Trigger<DragEvent>,
    mut commands: Commands,
    moving_query: Query<&MouseMoving>,
    hover_query: Query<&HoveredEntity>,
    mut transform_query: Query<(&mut Transform, Has<Port>)>,
) {
    let event = trigger.event();
    let viewport = trigger.entity();

    if event.button != PointerButton::Primary {
        return;
    }

    let moving = if let Ok(moving) = moving_query.get(viewport) {
        moving
    } else {
        // TODO: offset_list should be populated with all selected entities, for now just use the hovered entity
        let mut offset_list = Vec::new();
        let hovered_entity = hover_query.get(viewport).unwrap();
        if let Some(hovered_entity) = hovered_entity.0 {
            if let Ok((transform, is_port)) = transform_query.get(hovered_entity) {
                if is_port {
                    // TODO: enter wire drawing mode
                } else {
                    offset_list.push(EntityOffset {
                        entity: hovered_entity,
                        offset: transform.translation - event.pos,
                    });
                }
            }
        }

        if !offset_list.is_empty() {
            commands.entity(viewport).remove::<MouseIdle>();
            commands
                .entity(viewport)
                .insert(MouseMoving(offset_list.clone()));
        }
        &MouseMoving(offset_list)
    };

    for entity_offset in moving.0.iter() {
        if let Ok((mut transform, _)) = transform_query.get_mut(entity_offset.entity) {
            transform.translation = event.pos + entity_offset.offset;
        }
    }

    if event.drag_type == DragType::End {
        commands.entity(viewport).remove::<MouseMoving>();
        commands.entity(viewport).insert(MouseIdle);
    }
}
