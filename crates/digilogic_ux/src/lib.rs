mod states;
use states::*;

mod events;
pub use events::*;

mod systems;
pub use systems::*;

mod spatial_index;

#[derive(Clone, Debug, Default)]
pub struct UxPlugin {}

impl bevy_app::Plugin for UxPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<HoveredEntity>()
            .register_type::<EntityOffset>()
            .register_type::<MouseState>()
            .register_type::<MouseIdle>()
            .register_type::<MouseMoving>();

        app.add_event::<PointerMovedEvent>();
        app.add_event::<PointerButtonEvent>();
        app.observe(on_add_viewport_augment_with_fsm);

        app.init_resource::<spatial_index::SpatialIndex>();
        app.add_systems(bevy_app::PreUpdate, spatial_index::update_spatial_index);
        app.observe(spatial_index::on_remove_update_spatial_index);
    }
}
