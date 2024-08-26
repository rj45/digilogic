use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use bevy_reflect::Reflect;
use digilogic_core::transform::Vec2;

#[derive(Debug, Default, Component, Deref, DerefMut, Reflect)]
pub struct HoveredEntity(pub Option<Entity>);

#[derive(Debug, Component, Copy, Clone, Reflect)]
pub struct EntityOffset {
    pub entity: Entity,
    pub offset: Vec2,
}

#[derive(Debug, Component, Reflect)]
pub enum MouseState {
    Idle,
    Moving(Vec<EntityOffset>),
}

#[derive(Debug, Component, Reflect)]
pub struct MouseIdle;

#[derive(Debug, Component, Deref, DerefMut, Reflect)]
pub struct MouseMoving(pub Vec<EntityOffset>);
