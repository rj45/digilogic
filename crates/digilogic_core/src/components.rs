use bevy_ecs::prelude::*;
use smallvec::SmallVec;

/////
// Entity ID components
/////

#[derive(Component)]
pub struct PortID(Entity);

#[derive(Component)]
pub struct SymbolKindID(Entity);

#[derive(Component)]
pub struct SymbolID(Entity);

#[derive(Component)]
pub struct WaypointID(Entity);

#[derive(Component)]
pub struct EndpointID(Entity);

#[derive(Component)]
pub struct WireID(Entity);

#[derive(Component)]
pub struct SubnetID(Entity);

#[derive(Component)]
pub struct NetID(Entity);

#[derive(Component)]
pub struct CircuitID(Entity);

/////
// Entity part components
/////

/// The Position of the Entity in its parent's coordinate system
#[derive(Component)]
pub struct Position {
    x: f32,
    y: f32,
}

/// The Transform of the Entity in the world coordinate system,
/// which should always be kept up-to-date with the Position / Rotation
/// of the Entity, as well as its parent. This is a 3x2 matrix.
#[derive(Component)]
pub struct Transform {
    m_00: f32,
    m_01: f32,
    m_10: f32,
    m_11: f32,
    m_20: f32,
    m_21: f32,
}

/// The Size of the Entity
#[derive(Component)]
pub struct Size {
    width: f32,
    height: f32,
}

/// The Shape of the Entity as an index into the Shapes Vello can draw
#[derive(Component)]
pub struct Shape(u32);

/// A Name for the entity.
#[derive(Component)]
pub struct Name(String);

/// The Reference Designator prefix (like U for ICs, R for resistors, etc.)
#[derive(Component)]
pub struct DesignatorPrefix(String);

/// The Reference Designator number (like 1, 2, 3, etc.)
#[derive(Component)]
pub struct DesignatorNumber(u32);

/// The Reference Designator suffix (like A, B, C, etc.) if it has one
#[derive(Component)]
pub struct DesignatorSuffix(String);

/// The Number of the entity (pin number, etc.)
#[derive(Component)]
pub struct Number(i32);

/// The rotation of the entity in 90 degree increments
#[derive(Component)]
pub enum Rotation {
    Rot0,
    Rot90,
    Rot180,
    Rot270,
}

// The bitwidth of a Port / Symbol / Net.
// Can be up to 255 bits wide.
#[derive(Component)]
pub struct BitWidth(u8);

/// The list of bits that the entity uses in a Net. The order of the
/// bits becomes the order they are presented to the Ports the Subnet's
/// Endpoints are connected to. So, for example, if a Net is 4 bits wide,
/// and a Subnet uses bits 1, 3, and 0, then the Ports the Subnet's
/// Endpoints are connected to will be presented with 3 bits, bit 0 being
/// the Net's bit 1, bit 1 being the Net's bit 3, and bit 2 being the
/// Net's bit 0.
#[derive(Component)]
pub struct Bits(SmallVec<[u8; 8]>);

/// The entity is an input
#[derive(Component)]
pub struct Input;

/// The entity is an output
#[derive(Component)]
pub struct Output;

/// The entity is part of a set of entities. For example, one gate in a chip.
#[derive(Component)]
pub struct PartOf {
    first: Entity,
    index: u32,
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
///
/// Ports generally have these components:
/// - Parent: the Symbol or SymbolKind that the Port belongs to
/// - EndpointID: the Endpoint that the Port is connected to (Symbol Ports only)
/// - Name: the name of the Port
/// - Shape: the shape to draw for the Port
/// - Number: the pin number of the Port
/// - BitWidth: the bitwidth of the Port
/// - Input: if the Port is an input or inout Port
/// - Output: if the Port is an output or inout Port
/// - Selected: if the Port is selected
/// - Hovered: if the Port is hovered
/// - Hidden: if the Port is hidden
#[derive(Component)]
pub struct Port;

/// A SymbolKind is a template for a Symbol. It has Port Children which
/// are cloned into the Symbol as its Port Children when the Symbol is
/// instantiated.
///
/// SymbolKinds generally have these components:
/// - Name: the name of the SymbolKind
/// - Shape: the Shape of the SymbolKind
/// - CircuitID: the Circuit if the SymbolKind represents a sub-Circuit
/// - DesignatorPrefix: the prefix for the Reference Designator
/// - Children: the Ports that the SymbolKind has
/// - Selected: if the SymbolKind is selected for adding to the circuit
/// - Hidden: if the SymbolKind is hidden from displaying in UI lists
#[derive(Component)]
pub struct SymbolKind;

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
///
/// Symbols generally have these components:
/// - Name: the name of the Symbol (cloned from SymbolKind)
/// - Shape: the Shape of the Symbol when drawn (cloned from SymbolKind)
/// - DesignatorPrefix: the prefix for the Reference Designator (cloned from SymbolKind)
/// - DesignatorNumber: the number for the Reference Designator
/// - DesignatorSuffix: the suffix for the Reference Designator
/// - Position: the Position of the Symbol
/// - Rotation: the Rotation of the Symbol
/// - Size: the Size of the Symbol (cloned from SymbolKind)
/// - SymbolKindID: the SymbolKind that the Symbol is an instance of
/// - Parent: the Circuit that the Symbol is part of
/// - Children: the Ports that the Symbol has (which are cloned from SymbolKind)
/// - PartOf: if the Symbol is part of a set of Symbols
///   (ie. a gate in a chip with many gates)
/// - Selected: if the Symbol is selected
/// - Hovered: if the Symbol is hovered
/// - Hidden: if the Symbol is hidden
#[derive(Component)]
pub struct Symbol;

/// A Waypoint is a point in a Net that a wire needs to route through.
/// Which of the Net's wires depends on the Endpoint the Waypoint is attached to.
///
/// Waypoints generally have these components:
/// - Position: the Position of the Waypoint
/// - Parent: the Endpoint that the Waypoint is attached to
/// - Selected: if the Waypoint is selected
/// - Hovered: if the Waypoint is hovered
/// - Hidden: if the Waypoint is hidden
#[derive(Component)]
pub struct Waypoint;

/// An Endpoint is a connection point for a Wire. It connects to a Port
/// in a Symbol. Its Parent is the Subnet that the Endpoint is part of.
/// It has Waypoint Children.
///
/// Endpoints generally have these components:
/// - Parent: the Subnet that the Endpoint is connected to
/// - PortID: the Port that the Endpoint is connected to (optional)
/// - Position: the Position of the Endpoint
/// - Shape: the Shape of the Endpoint (optional)
#[derive(Component)]
pub struct Endpoint;

/// A Subnet is a subset of the Net's Endpoints that uses a subset of the
/// Net's bits.
///
/// Subnets generally have these components:
/// - Parent: the Net that the Subnet is part of
/// - Children: the Endpoints that the Subnet has
/// - BitWidth: the bitwidth of the Subnet, must match the Bits length
/// - Bits: the list of bits that the Subnet uses
/// - Name: the name of the Subnet (optional)
/// - Selected: if the Subnet is selected
/// - Hovered: if the Subnet is hovered
/// - Hidden: if the Subnet is hidden
#[derive(Component)]
pub struct Subnet;

/// A Net is a set of Subnets that are connected together. It has
/// Subnet Children, and a Netlist Parent. Often a Net will have
/// only one Subnet, unless there's a bus split.
///
/// Nets generally have these components:
/// - Parent: Circuit the Net is part of
/// - Children: the Subnets that the Net has
/// - Name: the name of the Net (optional)
/// - BitWidth: the bitwidth of the Net
/// - Selected: if the Net is selected
/// - Hovered: if the Net is hovered
/// - Hidden: if the Net is hidden
#[derive(Component)]
pub struct Net;

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
///
/// Circuits generally have these components:
/// - SymbolKindID: the SymbolKind that represents the Circuit in parent
///   Circuits
/// - Children: the Symbols and Nets that the Circuit has
#[derive(Component)]
pub struct Circuit;
