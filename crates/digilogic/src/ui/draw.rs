use bevy_ecs::prelude::*;

#[derive(Default, Resource)]
pub struct Scene(pub vello::Scene);

pub fn draw(mut scene: ResMut<Scene>) {
    let scene = &mut scene.0;
    scene.reset();
    // TODO
}
