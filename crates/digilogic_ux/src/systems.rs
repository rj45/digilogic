use super::{EntityOffset, HoveredEntity, MouseIdle, MouseMoving, MouseState};
use crate::spatial_index::SpatialIndex;
use crate::{ClickEvent, DragEvent, DragType, HoverEvent, MoveEntity, PointerButton};
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_state::prelude::*;
use digilogic_core::states::SimulationState;
use digilogic_core::transform::{BoundingBox, GlobalTransform, Transform, Vec2};
use digilogic_core::Fixed;
use digilogic_core::{components::*, fixed};

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
        .observe(mouse_click_inputs)
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

fn mouse_click_inputs(
    trigger: Trigger<ClickEvent>,
    hover_query: Query<&HoveredEntity>,
    mut input_query: Query<(&SymbolKind, &mut LogicState), With<Symbol>>,
    simulation: Res<State<SimulationState>>,
    mut eval_event: EventWriter<digilogic_netcode::Eval>,
) {
    let event = trigger.event();
    let viewport = trigger.entity();

    if !simulation.is_active() {
        return;
    }

    if event.button != PointerButton::Primary {
        return;
    }

    let hovered_entity = hover_query.get(viewport).unwrap();
    if let Some(hovered_entity) = hovered_entity.0 {
        if let Ok((&kind, mut state)) = input_query.get_mut(hovered_entity) {
            if kind == SymbolKind::In {
                let state = &mut *state;

                // TODO: support bit widths other than 1
                if let Some((first0, first1)) = state
                    .bit_plane_0
                    .first_mut()
                    .zip(state.bit_plane_1.first_mut())
                {
                    *first0 = !*first0 & 1;
                    *first1 = 1;
                } else {
                    state.bit_plane_0 = [1].as_slice().into();
                    state.bit_plane_1 = [1].as_slice().into();
                }

                bevy_log::info!("Eval event sent");
                eval_event.send(digilogic_netcode::Eval);
            }
        }
    }
}

fn mouse_drag_system(
    trigger: Trigger<DragEvent>,
    mut commands: Commands,
    moving_query: Query<&MouseMoving>,
    hover_query: Query<&HoveredEntity>,
    transform_query: Query<(&Transform, Has<Port>)>,
    mut move_events: EventWriter<MoveEntity>,
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
        // send an event to the entity move system, mainly to keep the argument list of this system shorter
        move_events.send(MoveEntity {
            viewport,
            circuit: event.circuit,
            entity: entity_offset.entity,
            pos: event.pos,
            offset: entity_offset.offset,
        });
    }

    if event.drag_type == DragType::End {
        commands.entity(viewport).remove::<MouseMoving>();
        commands.entity(viewport).insert(MouseIdle);
    }
}

const SNAP_CANDIDATE_DISTANCE: Fixed = fixed!(500);
const SNAP_DISTANCE: Fixed = fixed!(7);

/// Move entities while snapping the entity's ports to nearby ports
pub(crate) fn move_entities_with_snap(
    mut events: EventReader<MoveEntity>,
    spatial_indices: Query<&SpatialIndex, With<Circuit>>,
    children: Query<(Entity, Relations<Child>)>,
    port_transform_query: Query<&GlobalTransform, With<Port>>,
    mut transform_query: Query<&mut Transform, Without<Port>>,
    mut port_positions: Local<Vec<Vec2>>,
    mut excluded_ports: Local<Vec<Entity>>,
) {
    // for each MoveEntity event
    for event in events.read() {
        // find the transform for the entity
        if let Ok(mut transform) = transform_query.get_mut(event.entity) {
            let proposed_pos = event.pos + event.offset;
            let delta = proposed_pos - transform.translation;

            // find all ports for the entity
            port_positions.clear();
            excluded_ports.clear();
            children
                .traverse::<Child>(std::iter::once(event.entity))
                .for_each(|&mut entity, _| {
                    if let Ok(port_transform) = port_transform_query.get(entity) {
                        port_positions.push(port_transform.translation + delta);
                        excluded_ports.push(entity);
                    }
                });

            let mut x_delta = fixed!(0);
            let mut x_dist = Fixed::MAX_INT;

            let mut y_delta = fixed!(0);
            let mut y_dist = Fixed::MAX_INT;

            let mut comparisons = 0;

            // check all entities within SNAP_CANDIDATE_DISTANCE for ports to snap to
            let snap_vec = Vec2 {
                x: SNAP_CANDIDATE_DISTANCE,
                y: SNAP_CANDIDATE_DISTANCE,
            };
            let bbox = BoundingBox::from_points(proposed_pos - snap_vec, proposed_pos + snap_vec);

            // scan the spatial index for ports within bbox
            let spatial_index = spatial_indices
                .get(event.circuit.0)
                .expect("CircuitID is invalid on MoveEntity event");
            spatial_index.query(bbox, |entity| {
                if let Ok(candidate_transform) = port_transform_query.get(*entity) {
                    if excluded_ports.contains(entity) {
                        // do not snap to our own ports
                        return;
                    }

                    // for each of the moved entity's ports
                    for port_pos in port_positions.iter() {
                        // check if the x coordinate of the port is closer, if so, record it
                        let dx = (port_pos.x - candidate_transform.translation.x).abs();
                        if dx < x_dist {
                            x_dist = dx;
                            x_delta = candidate_transform.translation.x - port_pos.x;
                        }

                        // check if the y coordinate of the port is closer, if so, record it
                        let dy = (port_pos.y - candidate_transform.translation.y).abs();
                        if dy < y_dist {
                            y_dist = dy;
                            y_delta = candidate_transform.translation.y - port_pos.y;
                        }

                        comparisons += 1;
                    }
                }
            });

            // if the closest coordinates are too far away, don't snap to them
            if x_dist > SNAP_DISTANCE {
                x_delta = fixed!(0);
            }
            if y_dist > SNAP_DISTANCE {
                y_delta = fixed!(0);
            }

            // update the position with any snap delta added
            transform.translation = proposed_pos
                + Vec2 {
                    x: x_delta,
                    y: y_delta,
                };
        }
    }
}
