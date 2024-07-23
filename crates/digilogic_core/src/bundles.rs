use super::components::*;
use bevy_ecs::prelude::*;
use bevy_hierarchy::{Children, Parent};

/// A Visible is a bundle of the components needed to render an entity.
///
/// Can optionally have these additional components:
/// - Parent - the parent entity, if it has one
/// - Size - the shape will be scaled to fit the Size
/// - Rotation - the rotation of the shape and its children
/// - Selected - if the entity is selected
/// - Hovered - if the entity is hovered
/// - Hidden - if the entity is hidden
#[derive(Bundle)]
pub struct Visible {
    // the Position of the entity relative to its Parent
    pub position: Position,
    pub transform: Transform,
    pub shape: Shape,
}

/// A Port is a connection point for an Endpoint. For sub-Circuits,
/// it also connects to an Input or Output Symbol in the child Circuit.
///
/// Ports optionally can have some of these additional components:
/// - EndpointID - the Endpoint that the Port is connected to (Symbol Ports only)
/// - Name - the name of the Port
/// - Number - the pin number of the Port
///
#[derive(Bundle)]
pub struct PortBundle {
    // The marker that this is a Port
    pub marker: Port,

    /// Ports are Visible
    pub visible: Visible,

    /// The Symbol or SymbolKind that the Port belongs to
    pub symbol: Parent,

    /// The bit width of the Port. This must match the bit width of any
    /// connected subnet.
    pub bit_width: BitWidth,
}

/// A SymbolKind is a template for a Symbol. It has Port Children which
/// are cloned into the Symbol as its Port Children when the Symbol is
/// instantiated.
///
/// SymbolKinds optionally can have some of these additional components:
/// - CircuitID - the Circuit if the SymbolKind represents a sub-Circuit
#[derive(Bundle)]
pub struct SymbolKindBundle {
    // The marker that this is a SymbolKind
    pub marker: SymbolKind,

    /// SymbolKinds are Visible
    pub visible: Visible,

    /// The name of the SymbolKind
    pub name: Name,

    /// The DesignatorPrefix of the SymbolKind (ie. R for resistor, C for capacitor, etc)
    pub designator_prefix: DesignatorPrefix,

    /// The Ports that the SymbolKind has
    pub ports: Children,
}

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
///
/// Symbols optionally can have some of these additional components:
/// - DesignatorSuffix - the suffix for the Reference Designator
/// - PartOf - if the Symbol is part of a set of Symbols (ie one gate of a chip with many)
#[derive(Bundle)]
pub struct SymbolBundle {
    /// The marker that this is a Symbol
    pub marker: Symbol,

    /// Symbols are Visible
    pub visible: Visible,

    /// The name of the Symbol
    pub name: Name,

    /// The designator prefix of the Symbol, cloned from SymbolKind
    pub designator_prefix: DesignatorPrefix,

    /// The designator number of the Symbol
    pub designator_number: DesignatorNumber,

    /// The rotation of the Symbol
    pub rotation: Rotation,

    /// The size of the Symbol, cloned from SymbolKind
    pub size: Size,

    /// The SymbolKind that the Symbol is an instance of
    pub symbol_kind: SymbolKindID,

    /// The Circuit that the Symbol is part of
    pub circuit: Parent,

    /// The Ports that the Symbol has
    pub ports: Children,
}

/// A Waypoint is a point in a Net that a wire needs to route through.
/// Which of the Net's wires depends on the Endpoint the Waypoint is attached to.
#[derive(Bundle)]
pub struct WaypointBundle {
    /// The marker that this is a Waypoint
    pub marker: Waypoint,

    /// Waypoints are Visible
    pub visible: Visible,

    /// The Endpoint that the Waypoint is attached to
    pub endpoint: Parent,
}

/// An Endpoint is a connection point for a Wire. It connects to a Port
/// in a Symbol. Its Parent is the Wire that the Endpoint is part of.
/// It has Waypoint Children.
///
/// Endpoints optionally can have some of these additional components:
/// - PortID - the Port that the Endpoint is connected to
#[derive(Bundle)]
pub struct EndpointBundle {
    /// The marker that this is an Endpoint
    pub marker: Endpoint,

    /// Endpoints are Visible
    pub visible: Visible,

    /// The Wire that the Endpoint belongs to
    pub wire: Parent,

    /// The Waypoints that the Endpoint has
    pub waypoints: Children,
}

/// A Wire is a connection between two or more Endpoints. It has Endpoint Children.
/// Note: Wires are drawn a different way than other Shapes, and so are not Visible.
///
/// Wires optionally can have some of these additional components:
/// - Selected - if the Wire is selected
/// - Hovered - if the Wire is hovered
/// - Hidden - if the Wire is hidden
#[derive(Bundle)]
pub struct WireBundle {
    /// The marker that this is a Wire
    pub marker: Wire,

    /// The Endpoints that the Wire has
    pub endpoints: Children,
    // TODO: add vertices?
}

/// A Subnet is a subset of the Net's Endpoints that uses a subset of the
/// Net's bits.
///
/// Subnets optionally can have some of these additional components:
/// - ???
#[derive(Bundle)]
pub struct SubnetBundle {
    /// The marker that this is a Subnet
    pub marker: Subnet,

    /// The Net that the Subnet is part of
    pub net: Parent,

    /// The Wires that the Subnet has
    pub wires: Children,

    /// The bit width of the Subnet
    pub bit_width: BitWidth,

    /// The bits that the Subnet uses
    pub bits: Bits,
}

/// A Net is a set of Subnets that are connected together. It has
/// Subnet Children, and a Circuit Parent. Often a Net will have
/// only one Subnet, unless there's a bus split.
///
/// Nets optionally can have some of these additional components:
/// - ???
#[derive(Bundle)]
pub struct NetBundle {
    /// The Circuit the Net is part of
    pub circuit: Parent,

    /// The Subnets that the Net has
    pub subnets: Children,

    /// The name of the Net
    pub name: Name,

    /// The bit width of the Net
    pub bit_width: BitWidth,
}

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
///
/// Circuits optionally can have some of these additional components:
/// - ???
#[derive(Component)]
pub struct Circuit {
    /// The SymbolKind that represents the Circuit in parent Circuits
    pub symbol_kind: SymbolKindID,

    /// The Symbols and Nets that the Circuit has
    pub children: Vec<Entity>,
}
