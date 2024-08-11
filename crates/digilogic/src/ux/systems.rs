use super::{PointerButtonEvent, PointerMovedEvent};
use bevy_ecs::prelude::*;
use digilogic_core::components::{Endpoint, Hovered, Port, Symbol, Viewport, Waypoint};
use digilogic_core::spatial_index::SpatialIndex;
use digilogic_core::transform::{BoundingBox, Vec2};
use digilogic_core::{fixed, Fixed};
use std::ops::Sub;

/// Called when a new viewport is added to the world.
pub(crate) fn on_add_viewport_augment_with_fsm(
    trigger: Trigger<OnAdd, Viewport>,
    mut commands: Commands,
) {
    commands
        .entity(trigger.entity())
        .observe(hover_system)
        .observe(mouse_fsm_system);
}

#[derive(Debug, Clone)]
pub enum Subject {
    Symbol(Entity),
    Port(Entity),
    Endpoint(Entity),
    Waypoint(Entity),
}

pub enum Verb {
    LeftPress,
    LeftRelease,
    RightClick,
}

type HoverQuery<'w, 's, 'a> =
    Query<'w, 's, (Entity, Has<Symbol>, Has<Port>, Has<Endpoint>, Has<Waypoint>), With<Hovered>>;

fn mouse_fsm_system(trigger: Trigger<PointerButtonEvent>, hover_query: HoverQuery) {
    let viewport = trigger.entity();

    let mut subject = None;
    for (entity, symbol, port, endpoint, waypoint) in hover_query.iter() {
        subject = if symbol {
            Some(Subject::Symbol(entity))
        } else if port {
            Some(Subject::Port(entity))
        } else if endpoint {
            Some(Subject::Endpoint(entity))
        } else if waypoint {
            Some(Subject::Waypoint(entity))
        } else {
            continue;
        };
        break;
    }

    if let None = subject {
        return;
    }
    let subject = subject.unwrap();
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
