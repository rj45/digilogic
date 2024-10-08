use crate::components::*;
use crate::transform::*;
use crate::visibility::*;
use bevy_ecs::prelude::*;

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
#[derive(Debug, Bundle)]
pub struct PortBundle {
    // The marker that this is a Port
    pub port: Port,

    // The name of the Port
    pub name: Name,

    /// The bit width of the Port. This must match the bit width of any
    /// connected subnet.
    pub bit_width: BitWidth,

    pub transform: TransformBundle,
    pub visibility: VisibilityBundle,
    pub bounds: BoundingBoxBundle,
    pub directions: DirectionsBundle,
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
#[derive(Debug, Bundle)]
pub struct SymbolBundle {
    /// The marker that this is a Symbol
    pub symbol: Symbol,

    /// The name of the Symbol
    pub name: Name,

    /// The designator prefix of the Symbol, cloned from SymbolKind
    pub designator_prefix: DesignatorPrefix,

    /// The designator number of the Symbol
    pub designator_number: DesignatorNumber,

    /// The SymbolKind that the Symbol is an instance of
    pub symbol_kind: SymbolKind,

    pub shape: Shape,
    pub transform: TransformBundle,
    pub visibility: VisibilityBundle,
    pub bounds: BoundingBoxBundle,
}

/// An Endpoint is a connection point for a Net. It connects to a Port
/// in a Symbol. Its Parent is the Net that the Endpoint is part of.
/// It has Waypoint Children.
///
/// Endpoints have a Parent that is a Net and Waypoints as Children
///
/// Endpoints optionally can have some of these additional components:
/// - PortID - the Port that the Endpoint is connected to
#[derive(Debug, Bundle, Default)]
pub struct EndpointBundle {
    /// The marker that this is an Endpoint
    pub endpoint: Endpoint,

    pub transform: TransformBundle,
    pub visibility: VisibilityBundle,
    pub bounds: BoundingBoxBundle,
}

/// A Net is a set of Endpoints that are connected together.
///
/// Nets have a Circuit as a Parent, and Endpoints as Children
///
/// Nets optionally can have some of these additional components:
/// - ???
#[derive(Debug, Bundle)]
pub struct NetBundle {
    /// The marker that this is a Net
    pub net: Net,

    /// The name of the Net
    pub name: Name,

    /// The bit width of the Net
    pub bit_width: BitWidth,

    pub visibility: VisibilityBundle,
}

/// A Circuit is a set of Symbols and Nets forming an Electronic Circuit.
/// It has Symbol and Net Children, and a SymbolKind
///
/// Circuits have a Children component that contains the Symbols and Nets
///
/// Circuits optionally can have some of these additional components:
/// - ???
#[derive(Debug, Bundle, Default)]
pub struct CircuitBundle {
    /// The marker that this is a Circuit
    pub circuit: Circuit,
    /// The name of the circuit
    pub name: Name,
}
