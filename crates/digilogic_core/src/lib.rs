pub mod bundles;
pub mod components;
pub mod events;
pub mod resources;
pub mod states;
pub mod symbol;
pub mod transform;
pub mod visibility;

#[macro_use]
extern crate static_assertions;

mod shared_str;
pub use shared_str::SharedStr;

mod fixed;
pub use fixed::FRACT_BITS as FIXED_FRACT_BITS;
pub use fixed::{Fixed, FromFixedError, ToFixedError};

pub type HashSet<T> = ahash::AHashSet<T>;
pub type HashMap<K, V> = ahash::AHashMap<K, V>;

use bevy_ecs::prelude::*;
use bevy_ecs::system::SystemParam;
use bevy_state::prelude::*;
use bevy_state::state::FreelyMutableState;
use std::ops::Deref;

#[derive(Debug, SystemParam)]
pub struct StateMut<'w, S: FreelyMutableState> {
    state: Res<'w, State<S>>,
    next_state: ResMut<'w, NextState<S>>,
}

impl<S: FreelyMutableState> StateMut<'_, S> {
    #[inline]
    pub fn get(&self) -> &S {
        self.state.get()
    }

    #[inline]
    pub fn queue_next(&mut self, next: S) {
        self.next_state.set(next);
    }
}

impl<S: FreelyMutableState> Deref for StateMut<'_, S> {
    type Target = S;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

#[derive(Default, Debug)]
pub struct CorePlugin;

impl bevy_app::Plugin for CorePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        use aery::prelude::*;

        app.register_type::<SharedStr>().register_type::<Fixed>();

        #[cfg(feature = "inspector")]
        {
            use bevy_inspector_egui::inspector_egui_impls::InspectorEguiImpl;

            app.register_type_data::<SharedStr, InspectorEguiImpl>()
                .register_type_data::<Fixed, InspectorEguiImpl>();
        }

        app.register_relation::<components::Child>();

        app.register_type::<components::PortID>()
            .register_type::<components::SymbolKind>()
            .register_type::<components::SymbolID>()
            .register_type::<components::WaypointID>()
            .register_type::<components::EndpointID>()
            .register_type::<components::NetID>()
            .register_type::<components::CircuitID>()
            .register_type::<components::Shape>()
            .register_type::<components::Name>()
            .register_type::<components::DesignatorPrefix>()
            .register_type::<components::DesignatorNumber>()
            .register_type::<components::DesignatorSuffix>()
            .register_type::<components::Number>()
            .register_type::<components::BitWidth>()
            .register_type::<components::LogicState>()
            .register_type::<components::Bits>()
            .register_type::<components::Input>()
            .register_type::<components::Output>()
            .register_type::<components::Selected>()
            .register_type::<components::Hovered>()
            .register_type::<components::Port>()
            .register_type::<components::Symbol>()
            .register_type::<components::Endpoint>()
            .register_type::<components::Net>()
            .register_type::<components::Circuit>()
            .register_type::<resources::Project>()
            .register_type::<states::SimulationState>()
            .register_type::<states::SimulationConnected>()
            .register_type::<states::SimulationActive>();

        app.init_state::<states::SimulationState>()
            .add_computed_state::<states::SimulationConnected>()
            .add_computed_state::<states::SimulationActive>();

        app.init_resource::<symbol::SymbolRegistry>();

        app.add_event::<events::ProjectLoadEvent>()
            .add_event::<events::ProjectLoadedEvent>()
            .add_event::<events::CircuitLoadEvent>()
            .add_event::<events::CircuitLoadedEvent>();

        app.add_plugins((transform::TransformPlugin, visibility::VisibilityPlugin));
    }
}
