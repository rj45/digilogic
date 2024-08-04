use bevy_ecs::prelude::*;

#[derive(Event)]
pub struct InputEvent {
    pub viewport: Entity,
    pub event: egui::Event,
}
