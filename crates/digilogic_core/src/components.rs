use crate::SharedStr;
use bevy_ecs::prelude::*;
use smallvec::SmallVec;

/////
// Entity ID components
/////

#[derive(Component)]
pub struct PortID(pub Entity);

#[derive(Component)]
pub struct SymbolKindID(pub Entity);

#[derive(Component)]
pub struct SymbolID(pub Entity);

#[derive(Component)]
pub struct WaypointID(pub Entity);

#[derive(Component)]
pub struct EndpointID(pub Entity);

#[derive(Component)]
pub struct WireID(pub Entity);

#[derive(Component)]
pub struct SubnetID(pub Entity);

#[derive(Component)]
pub struct NetID(pub Entity);

#[derive(Component)]
pub struct CircuitID(pub Entity);

/////
// Entity part components
/////

/// The Position of the Entity in its parent's coordinate system
#[derive(Default, Component)]
pub struct Position {
    pub x: f32,
    pub y: f32,
}

/// The Transform of the Entity in the world coordinate system,
/// which should always be kept up-to-date with the Position / Rotation
/// of the Entity, as well as its parent. This is a 3x2 matrix.
#[derive(Default, Component)]
pub struct Transform {
    pub m_00: f32,
    pub m_01: f32,
    pub m_10: f32,
    pub m_11: f32,
    pub m_20: f32,
    pub m_21: f32,
}

/// The Size of the Entity
#[derive(Default, Component)]
pub struct Size {
    pub width: f32,
    pub height: f32,
}

/// The Shape of the Entity as an index into the Shapes Vello can draw
#[derive(Default, Component)]
pub struct Shape(pub u32);

/// A Name for the entity.
#[derive(Component)]
pub struct Name(pub SharedStr);

/// The Reference Designator prefix (like U for ICs, R for resistors, etc.)
#[derive(Component)]
pub struct DesignatorPrefix(pub SharedStr);

/// The Reference Designator number (like 1, 2, 3, etc.)
#[derive(Component)]
pub struct DesignatorNumber(pub u32);

/// The Reference Designator suffix (like A, B, C, etc.) if it has one
#[derive(Component)]
pub struct DesignatorSuffix(pub SharedStr);

/// The Number of the entity (pin number, etc.)
#[derive(Component)]
pub struct Number(pub i32);

/// The rotation of the entity in 90 degree increments
#[derive(Default, Component)]
pub enum Rotation {
    #[default]
    Rot0,
    Rot90,
    Rot180,
    Rot270,
}

// The bitwidth of a Port / Symbol / Net.
// Can be up to 255 bits wide.
#[derive(Component)]
pub struct BitWidth(pub u8);

/// The list of bits that the entity uses in a Net. The order of the
/// bits becomes the order they are presented to the Ports the Subnet's
/// Endpoints are connected to. So, for example, if a Net is 4 bits wide,
/// and a Subnet uses bits 1, 3, and 0, then the Ports the Subnet's
/// Endpoints are connected to will be presented with 3 bits, bit 0 being
/// the Net's bit 1, bit 1 being the Net's bit 3, and bit 2 being the
/// Net's bit 0.
#[derive(Component)]
pub struct Bits(pub SmallVec<[u8; 8]>);

/// The entity is an input
#[derive(Component)]
pub struct Input;

/// The entity is an output
#[derive(Component)]
pub struct Output;

/// The entity is part of a set of entities. For example, one gate in a chip.
#[derive(Component)]
pub struct PartOf {
    pub first: Entity,
    pub index: u32,
}

/// Whether to hide the entity when drawing
// TODO: should be sparse?
#[derive(Component)]
pub struct Hidden;

/// Whether the entity is selected
// TODO: should be sparse?
#[derive(Component)]
pub struct Selected;

/// Whether the entity is hovered
// TODO: should be sparse?
#[derive(Component)]
pub struct Hovered;

// Entity type tags

/// A Port is a connection point for an Endpoint. For sub-Circuits,
/// it also connects to an Input or Output Symbol in the child Circuit.
#[derive(Component)]
pub struct Port;

/// A SymbolKind is a template for a Symbol. It has Port Children which
/// are cloned into the Symbol as its Port Children when the Symbol is
/// instantiated.
#[derive(Component)]
pub struct SymbolKind;

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
#[derive(Component)]
pub struct Symbol;

/// A Waypoint is a point in a Net that a wire needs to route through.
/// Which of the Net's wires depends on the Endpoint the Waypoint is attached to.
#[derive(Component)]
pub struct Waypoint;

/// An Endpoint is a connection point for a Wire. It connects to a Port
/// in a Symbol. Its Parent is the Subnet that the Endpoint is part of.
/// It has Waypoint Children.
#[derive(Component)]
pub struct Endpoint;

/// A Wire is a connection between two or more Endpoints. It has Endpoint Children.
#[derive(Component)]
pub struct Wire;

/// A Subnet is a subset of the Net's Wires that uses a subset of the
/// Net's bits.
#[derive(Component)]
pub struct Subnet;

/// A Net is a set of Subnets that are connected together. It has
/// Subnet Children, and a Netlist Parent. Often a Net will have
/// only one Subnet, unless there's a bus split.
#[derive(Component)]
pub struct Net;

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
#[derive(Component)]
pub struct Circuit;
