use crate::SharedStr;
use aery::prelude::*;
use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use bevy_reflect::Reflect;
use smallvec::{smallvec, SmallVec};
use std::num::NonZeroU8;
use std::path::PathBuf;

/////
// Entity relations
/////

#[derive(Debug, Relation)]
#[aery(Recursive)]
pub struct Child;

/////
// Entity ID components
/////

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct PortID(pub Entity);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub enum SymbolKind {
    And,
    Or,
    Xor,
    Not,
    In,
    Out,
    Mux,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct SymbolID(pub Entity);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct WaypointID(pub Entity);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct EndpointID(pub Entity);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct NetID(pub Entity);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Component, Reflect)]
pub struct CircuitID(pub Entity);

/////
// Entity part components
/////

/// The Shape of the Entity as an index into the Shapes Vello can draw
#[derive(Default, Debug, Component, Clone, Copy, Reflect)]
pub enum Shape {
    #[default]
    Chip,
    And,
    Or,
    Xor,
    Not,
    Input,
    Output,
}

/// A Name for the entity.
#[derive(Default, Debug, Clone, Deref, Component, Reflect)]
pub struct Name(pub SharedStr);

// The file path of the entity.
#[derive(Default, Debug, Clone, Deref, Component, Reflect)]
pub struct FilePath(pub PathBuf);

/// The Reference Designator prefix (like U for ICs, R for resistors, etc.)
#[derive(Default, Debug, Clone, Deref, Component, Reflect)]
pub struct DesignatorPrefix(pub SharedStr);

/// The Reference Designator number (like 1, 2, 3, etc.)
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Component, Reflect)]
pub struct DesignatorNumber(pub u32);

/// The Reference Designator suffix (like A, B, C, etc.) if it has one
#[derive(Default, Debug, Clone, Deref, Component, Reflect)]
pub struct DesignatorSuffix(pub SharedStr);

/// The Number of the entity (pin number, etc.)
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Component, Reflect)]
pub struct Number(pub i32);

// The bitwidth of a Port / Symbol / Net.
// Can be up to 255 bits wide.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Component, Reflect)]
pub struct BitWidth(pub NonZeroU8);

/// The logic state of the entity
#[derive(Default, Debug, Component, Reflect)]
pub struct LogicState {
    pub bit_plane_0: SmallVec<[u8; 16]>,
    pub bit_plane_1: SmallVec<[u8; 16]>,
}

impl LogicState {
    pub fn from_bool(value: bool) -> Self {
        Self {
            bit_plane_0: smallvec![value as u8],
            bit_plane_1: smallvec![1],
        }
    }
}

/// The list of bits that the entity uses in a Net. The order of the bits becomes
/// the order they are presented to the input of the entity. So, for example, if
/// a Net is 4 bits wide, and an entity uses bits 1, 3, and 0, then the entity
/// will be presented with 3 bits, bit 0 being the Net's bit 1, bit 1 being the
/// Net's bit 3, and bit 2 being the Net's bit 0.
#[derive(Debug, Component, Reflect)]
pub struct Bits(pub SmallVec<[u8; 8]>);

/// The entity is an input
#[derive(Default, Debug, Component, Reflect)]
pub struct Input;

/// The entity is an output
#[derive(Default, Debug, Component, Reflect)]
pub struct Output;

/// Whether the entity is selected
#[derive(Default, Debug, Component, Reflect)]
pub struct Selected;

/// Whether the entity is hovered
#[derive(Default, Debug, Component, Reflect)]
#[component(storage = "SparseSet")]
pub struct Hovered;

// Entity type tags

/// A Port is a connection point for an Endpoint. For sub-Circuits,
/// it also connects to an Input or Output Symbol in the child Circuit.
#[derive(Default, Debug, Component, Reflect)]
pub struct Port;

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
#[derive(Default, Debug, Component, Reflect)]
pub struct Symbol;

/// An Endpoint is a connection point for a Wire. It connects to a Port
/// in a Symbol. Its Parent is the Subnet that the Endpoint is part of.
/// It has Waypoint Children.
#[derive(Default, Debug, Component, Reflect)]
pub struct Endpoint;

/// A Net is a set of Subnets that are connected together. It has
/// Subnet Children, and a Netlist Parent. Often a Net will have
/// only one Subnet, unless there's a bus split.
#[derive(Default, Debug, Component, Reflect)]
pub struct Net;

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
#[derive(Default, Debug, Component, Reflect)]
pub struct Circuit;

/// A Viewport is a view into the Circuit. Mostly handled by the UI layer
/// but defined here for other systems to use.
#[derive(Default, Debug, Component, Reflect)]
pub struct Viewport;
