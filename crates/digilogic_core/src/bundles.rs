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
#[derive(Default, Bundle)]
pub struct Visible {
    // the Position of the entity relative to its Parent
    pub position: Position,
    pub transform: Transform,
    pub shape: Shape,
}

/// A Port is a connection point for an Endpoint. For sub-Circuits,
/// it also connects to an Input or Output Symbol in the child Circuit.
///
/// Ports have a Symbol or SymbolKind as a Parent
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
/// SymbolKinds have Ports as Children
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

    pub size: Size,

    /// The DesignatorPrefix of the SymbolKind (ie. R for resistor, C for capacitor, etc)
    pub designator_prefix: DesignatorPrefix,
}

/// A Symbol is an instance of a SymbolKind. It has Port Children which
/// are its input and output Ports. It represents an all or part of an
/// electronic component.
///
/// Symbols have a Circuit as a Parent, and Ports as Children
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

/// An Endpoint is a connection point for a Net. It connects to a Port
/// in a Symbol. Its Parent is the Net that the Endpoint is part of.
/// It has Waypoint Children.
///
/// Endpoints have a Parent that is a Net and Waypoints as Children
///
/// Endpoints optionally can have some of these additional components:
/// - PortID - the Port that the Endpoint is connected to
#[derive(Bundle)]
pub struct EndpointBundle {
    /// The marker that this is an Endpoint
    pub marker: Endpoint,

    /// Endpoints are Visible
    pub visible: Visible,
}

/// A Net is a set of Endpoints that are connected together.
///
/// Nets have a Circuit as a Parent, and Endpoints as Children
///
/// Nets optionally can have some of these additional components:
/// - ???
#[derive(Bundle)]
pub struct NetBundle {
    /// The name of the Net
    pub name: Name,

    /// The bit width of the Net
    pub bit_width: BitWidth,
}

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
///
/// Circuits have a Children component that contains the Symbols and Nets
///
/// Circuits optionally can have some of these additional components:
/// - ???
#[derive(Component)]
pub struct CircuitBundle {
    /// The marker that this is a Circuit
    pub marker: Circuit,

    /// The SymbolKind that represents the Circuit in parent Circuits
    pub symbol_kind: SymbolKindID,
}
