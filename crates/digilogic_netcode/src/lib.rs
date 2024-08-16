use digilogic_core::components::Circuit;
use digilogic_core::HashMap;
use renet::*;

#[derive(Default)]
#[cfg_attr(feature = "client", derive(bevy_ecs::prelude::Component))]
#[allow(missing_debug_implementations)]
pub struct SimState {
    // TODO: these are just placeholders
    nets: Vec<u32>,
    components: HashMap<u32, u32>,
}

#[cfg(feature = "client")]
mod client {
    use super::*;
    use bevy_app::prelude::*;
    use bevy_ecs::prelude::*;
    use bevy_time::prelude::*;

    fn inject_sim_state(trigger: Trigger<OnAdd, Circuit>, mut commands: Commands) {
        commands
            .get_entity(trigger.entity())
            .unwrap()
            .insert(SimState::default());
    }

    fn update_client(mut client: ResMut<RenetClient>, time: Res<Time>) {
        client.update(time.delta())
    }

    #[derive(Default, Debug)]
    pub struct ClientPlugin;

    impl Plugin for ClientPlugin {
        fn build(&self, app: &mut App) {
            app.observe(inject_sim_state);
            app.add_systems(
                PreUpdate,
                update_client.run_if(resource_exists::<RenetClient>),
            );
        }
    }
}

#[cfg(feature = "client")]
pub use client::*;
