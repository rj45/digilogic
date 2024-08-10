use bevy_ecs::prelude::*;
use digilogic_core::{
    components::{EndpointID, SymbolKind},
    transform::Vec2,
};

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum FSMEvent {
    ClickSymbol(Entity),
    ClickPort(Entity),
    ClickEndpoint(Entity),
    ClickWire(Entity, Vec2),
    ClickBackground(Vec2),
    Drag(Vec2),
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum RootMouseState {
    #[default]
    None,
    Selecting(SelectMouseState),
    Moving(MoveMouseState),
    Adding(AddMouseState),
    Wiring(WireMouseState),
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum SelectMouseState {
    #[default]
    None,
    Selecting,
    Selected(Entity),
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum MoveMouseState {
    #[default]
    None,
    Moving(Entity),
    Moved(Entity),
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum AddMouseState {
    #[default]
    None,
    Adding(SymbolKind),
    Added(Entity),
    Cancelled,
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub enum WireMouseState {
    #[default]
    None,
    NewEndpoint,
    Wiring(EndpointID),
    Wired(EndpointID),
    Cancelled,
}
