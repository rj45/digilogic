use bevy_ecs::prelude::*;

// TODO: the following types probably shouldn't be using egui types, I am just being lazy.
// Especially if these move to core, they should be using their own types.

/// A mouse button was pressed or released (or a touch started or stopped).
#[derive(Event, Debug)]
pub struct PointerButtonEvent {
    /// Where is the pointer?
    pub pos: egui::Pos2,

    /// What mouse button? For touches, use [`PointerButton::Primary`].
    pub button: egui::PointerButton,

    /// Was it the button/touch pressed this frame, or released?
    pub pressed: bool,

    /// The state of the modifier keys at the time of the event.
    pub modifiers: egui::Modifiers,
}

/// The mouse or touch moved to a new place.
#[derive(Event, Debug)]
pub struct PointerMovedEvent(pub egui::Pos2);
