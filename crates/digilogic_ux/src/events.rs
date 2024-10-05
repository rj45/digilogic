use bevy_ecs::prelude::*;
use digilogic_core::{components::CircuitID, transform::Vec2};

#[derive(Event, Debug, Copy, Clone, PartialEq, Eq)]
pub enum PointerButton {
    Primary,
    Secondary,
    Middle,
}

#[derive(Event, Debug, Copy, Clone)]
pub struct Modifiers {
    pub alt: bool,
    pub ctrl: bool,
    pub shift: bool,
    pub mac_cmd: bool,
    pub command: bool,
}

#[derive(Event, Debug)]
pub struct ClickEvent {
    /// Which viewport does this event target?
    pub viewport: Entity,

    /// Which circuit does this event target?
    pub circuit: CircuitID,

    pub pos: Vec2,
    pub button: PointerButton,
    pub modifiers: Modifiers,
}

#[derive(Event, Debug)]
pub struct HoverEvent {
    /// Which viewport does this event target?
    pub viewport: Entity,

    /// Which circuit does this event target?
    pub circuit: CircuitID,

    pub pos: Vec2,
    pub modifiers: Modifiers,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DragType {
    Start,
    Dragging,
    End,
}

#[derive(Event, Debug)]
pub struct DragEvent {
    pub drag_type: DragType,

    /// Which viewport does this event target?
    pub viewport: Entity,

    /// Which circuit does this event target?
    pub circuit: CircuitID,

    pub pos: Vec2,
    pub delta: Vec2,
    pub button: PointerButton,
    pub modifiers: Modifiers,
}

#[derive(Event, Debug)]
pub struct MoveEntity {
    pub viewport: Entity,
    pub circuit: CircuitID,
    pub entity: Entity,
    pub pos: Vec2,
    pub offset: Vec2,
}
