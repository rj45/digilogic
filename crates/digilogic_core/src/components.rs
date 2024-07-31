use crate::SharedStr;
use bevy_ecs::prelude::*;
use smallvec::SmallVec;

#[derive(Component)]
pub struct Parent(pub Entity);

/////
// Entity ID components
/////

#[derive(Component)]
pub struct PortID(pub Entity);

#[derive(Component, Clone, Copy)]
pub struct SymbolKindIndex(pub usize);

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

/// The Shape of the Entity as an index into the Shapes Vello can draw
#[derive(Default, Component, Clone, Copy)]
pub enum Shape {
    #[default]
    Chip,
    Port,
    And,
    Or,
    Xor,
    Not,
    Input,
    Output,
}

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

// The bitwidth of a Port / Symbol / Net.
// Can be up to 255 bits wide.
#[derive(Component)]
pub struct BitWidth(pub u8);

/// The list of bits that the entity uses in a Net. The order of the bits becomes
/// the order they are presented to the input of the entity. So, for example, if
/// a Net is 4 bits wide, and an entity uses bits 1, 3, and 0, then the entity
/// will be presented with 3 bits, bit 0 being the Net's bit 1, bit 1 being the
/// Net's bit 3, and bit 2 being the Net's bit 0.
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

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
#[derive(Component)]
pub struct Symbol {
    pub ports: SmallVec<[Entity; 7]>,
}

/// A Waypoint is a point in a Net that a wire needs to route through.
/// Which of the Net's wires depends on the Endpoint the Waypoint is attached to.
#[derive(Component, Default)]
pub struct Waypoint;

/// An Endpoint is a connection point for a Wire. It connects to a Port
/// in a Symbol. Its Parent is the Subnet that the Endpoint is part of.
/// It has Waypoint Children.
#[derive(Default, Component)]
pub struct Endpoint {
    pub waypoints: SmallVec<[Entity; 2]>,
}

/// A Net is a set of Subnets that are connected together. It has
/// Subnet Children, and a Netlist Parent. Often a Net will have
/// only one Subnet, unless there's a bus split.
#[derive(Default, Component)]
pub struct Net {
    pub endpoints: SmallVec<[Entity; 2]>,
}

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
#[derive(Default, Component)]
pub struct Circuit {
    pub symbols: Vec<Entity>,
    pub nets: Vec<Entity>,
}
