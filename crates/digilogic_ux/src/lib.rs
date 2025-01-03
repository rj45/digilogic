mod states;
use states::*;

mod events;
pub use events::*;

mod systems;
use systems::*;

mod spatial_index;

#[derive(Clone, Debug, Default)]
pub struct UxPlugin;

impl bevy_app::Plugin for UxPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        use bevy_ecs::prelude::*;

        app.register_type::<HoveredEntity>()
            .register_type::<EntityOffset>()
            .register_type::<MouseState>()
            .register_type::<MouseIdle>()
            .register_type::<MouseMoving>();

        app.add_event::<DragEvent>();
        app.add_event::<ClickEvent>();
        app.add_event::<HoverEvent>();
        app.add_event::<MoveEntity>();
        app.add_observer(on_add_viewport_augment_with_fsm);

        app.add_observer(spatial_index::inject_spatial_index);
        app.add_systems(bevy_app::PreUpdate, spatial_index::update_spatial_index);
        app.add_systems(
            bevy_app::PreUpdate,
            spatial_index::update_spatial_index_on_routing.after(digilogic_routing::RoutingSet),
        );
        app.add_observer(spatial_index::on_remove_bounding_box_update_spatial_index);
        app.add_observer(spatial_index::on_remove_net_update_spatial_index);
        app.add_systems(bevy_app::PostUpdate, move_entities_with_snap);
    }
}
