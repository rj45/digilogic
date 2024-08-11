use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use digilogic_core::transform::Vec2;

#[derive(Component, Deref, DerefMut)]
pub struct HoveredList(pub Vec<Entity>);

#[derive(Component, Copy, Clone)]
pub struct EntityOffset {
    pub entity: Entity,
    pub offset: Vec2,
}

#[derive(Component)]
pub enum MouseState {
    Idle,
    Moving(Vec<EntityOffset>),
}

#[derive(Component)]
pub struct MouseIdle;

#[derive(Component, Deref, DerefMut)]
pub struct MouseMoving(pub Vec<EntityOffset>);
