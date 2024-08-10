mod states;
use states::*;

mod events;
pub use events::*;

mod systems;
pub use systems::*;

#[derive(Clone, Debug, Default)]
pub struct UxPlugin {}

impl bevy_app::Plugin for UxPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.add_event::<PointerMovedEvent>();
        app.add_event::<PointerButtonEvent>();
        app.observe(on_add_viewport_augment_with_fsm);
    }
}
