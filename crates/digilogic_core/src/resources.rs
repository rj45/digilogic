use crate::components::CircuitID;
use crate::SharedStr;
use bevy_ecs::prelude::*;
use bevy_reflect::prelude::*;
use std::path::PathBuf;

#[derive(Debug, Resource, Reflect)]
#[reflect(Resource)]
pub struct Project {
    pub name: SharedStr,
    pub file_path: Option<PathBuf>,
    pub root_circuit: Option<CircuitID>,
}
