pub mod bundles;
pub mod components;

use bevy_ecs::schedule::Schedule;
use bevy_ecs::world::World;

pub trait Plugin {
    fn build(self, world: &mut World, schedule: &mut Schedule);
}
