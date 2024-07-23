use std::{error::Error, path::PathBuf, sync::Arc};

use super::components::CircuitID;
use bevy_ecs::prelude::*;

#[derive(Event)]
pub struct LoadEvent {
    pub filename: PathBuf,
}

#[derive(Event)]
pub struct LoadedEvent {
    pub filename: PathBuf,
    pub circuit: CircuitID,
}

// TODO: fixme
// #[derive(Event)]
// pub struct ErrorEvent {
//     // TODO: Add context to this for logging
//     // TODO: Add a way to localize this error for display to the user
//     error: Arc<Box<dyn Error>>,
// }
