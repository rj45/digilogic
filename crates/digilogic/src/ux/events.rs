use bevy_ecs::prelude::*;

#[derive(Event, Debug)]
pub struct InputEvent {
    pub viewport: Entity,
    pub event: egui::Event,
}
