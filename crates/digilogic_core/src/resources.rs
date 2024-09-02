use crate::components::CircuitID;
use bevy_ecs::prelude::*;
use bevy_reflect::prelude::*;

#[derive(Debug, Resource, Reflect)]
#[reflect(Resource)]
pub struct Project {
    pub name: String,
    pub root_circuit: Option<CircuitID>,
}
