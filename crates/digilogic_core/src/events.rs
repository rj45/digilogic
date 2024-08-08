use crate::components::CircuitID;
use bevy_ecs::prelude::*;
use std::path::PathBuf;

#[derive(Debug, Event)]
pub struct LoadEvent {
    pub filename: PathBuf,
}

#[derive(Debug, Event)]
pub struct LoadedEvent {
    pub filename: PathBuf,
    pub circuit: CircuitID,
}

#[derive(Debug, Event)]
pub struct UnloadedEvent {
    pub circuit: CircuitID,
}

// TODO: fixme
// #[derive(Event)]
// pub struct ErrorEvent {
//     // TODO: Add context to this for logging
//     // TODO: Add a way to localize this error for display to the user
//     error: Arc<Box<dyn Error>>,
// }
