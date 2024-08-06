mod root_fsm;
use root_fsm::*;

mod states;
use states::*;

mod events;
pub use events::*;

#[derive(Clone, Debug, Default)]
pub struct UxPlugin {}

impl bevy_app::Plugin for UxPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.add_event::<crate::ux::PointerMovedEvent>();
        app.add_event::<crate::ux::PointerButtonEvent>();
        app.observe(on_add_viewport_augment_with_fsm);
    }
}
