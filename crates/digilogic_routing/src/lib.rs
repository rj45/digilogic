use bevy_ecs::prelude::*;
use digilogic_core::components::*;

// fn route(
//     circuits: Query<Entity, (With<Circuit>, Without<Symbol>, Without<Net>)>,
//     symbols: Query<(&Position, &Rotation), (Without<Circuit>, With<Symbol>, Without<Net>)>,
//     nets: Query<Entity, (Without<Circuit>, Without<Symbol>, With<Net>)>,
// ) {
// }

#[derive(Default)]
pub struct RoutingPlugin;

impl bevy_app::Plugin for RoutingPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        // app.register_system(route);
    }
}
