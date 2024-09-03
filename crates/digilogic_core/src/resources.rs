use std::path::PathBuf;

use crate::components::CircuitID;
use bevy_ecs::prelude::*;
use bevy_reflect::prelude::*;

#[derive(Debug, Resource, Reflect)]
#[reflect(Resource)]
pub struct Project {
    pub name: String,
    pub file_path: Option<PathBuf>,
    pub root_circuit: Option<CircuitID>,
}
