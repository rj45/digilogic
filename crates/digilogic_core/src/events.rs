use crate::components::CircuitID;
use bevy_ecs::prelude::*;
use std::path::PathBuf;

#[derive(Debug, Event)]
pub struct ProjectLoadEvent {
    pub filename: PathBuf,
}

#[derive(Debug, Event)]
pub struct ProjectLoadedEvent;

#[derive(Debug, Event)]
pub struct CircuitLoadEvent {
    pub filename: PathBuf,
}

#[derive(Debug, Event)]
pub struct CircuitLoadedEvent {
    pub circuit: CircuitID,
}

// TODO: fixme
// #[derive(Event)]
// pub struct ErrorEvent {
//     // TODO: Add context to this for logging
//     // TODO: Add a way to localize this error for display to the user
//     error: Arc<Box<dyn Error>>,
// }
