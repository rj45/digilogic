pub mod bundles;
pub mod components;
pub mod events;
pub mod symbol;
pub mod transform;
pub mod visibility;

#[macro_use]
extern crate static_assertions;

mod shared_str;
pub use shared_str::SharedStr;

#[derive(Default)]
pub struct CorePlugin;

impl bevy_app::Plugin for CorePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<SharedStr>();
        #[cfg(feature = "inspector")]
        app.register_type_data::<SharedStr, bevy_inspector_egui::inspector_egui_impls::InspectorEguiImpl>();

        app.register_type::<components::PortID>()
            .register_type::<components::SymbolKindIndex>()
            .register_type::<components::SymbolID>()
            .register_type::<components::WaypointID>()
            .register_type::<components::EndpointID>()
            .register_type::<components::WireID>()
            .register_type::<components::SubnetID>()
            .register_type::<components::NetID>()
            .register_type::<components::CircuitID>()
            .register_type::<components::Shape>()
            .register_type::<components::Name>()
            .register_type::<components::DesignatorPrefix>()
            .register_type::<components::DesignatorNumber>()
            .register_type::<components::DesignatorSuffix>()
            .register_type::<components::Number>()
            .register_type::<components::BitWidth>()
            .register_type::<components::Bits>()
            .register_type::<components::Input>()
            .register_type::<components::Output>()
            .register_type::<components::PartOf>()
            .register_type::<components::Selected>()
            .register_type::<components::Hovered>()
            .register_type::<components::Port>()
            .register_type::<components::Symbol>()
            .register_type::<components::Waypoint>()
            .register_type::<components::Endpoint>()
            .register_type::<components::Net>()
            .register_type::<components::Circuit>()
            .register_type::<transform::Vec2i>()
            .register_type::<transform::Rotation>()
            .register_type::<transform::Transform>()
            .register_type::<transform::BoundingBox>()
            .register_type::<transform::GlobalTransform>()
            .register_type::<transform::AbsoluteBoundingBox>()
            .register_type::<visibility::Visibility>()
            .register_type::<visibility::ComputedVisibility>();

        app.init_resource::<symbol::SymbolRegistry>();
        app.add_event::<events::LoadEvent>();
        app.add_event::<events::LoadedEvent>();
        app.add_systems(
            bevy_app::PostUpdate,
            (transform::update_transforms, visibility::update_visibility),
        );
    }
}
