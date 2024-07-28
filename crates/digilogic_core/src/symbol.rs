use crate::components::*;
use crate::SharedStr;
use bevy_ecs::prelude::*;

#[derive(Clone)]
struct PortDef {
    name: SharedStr,
    bit_width: u8,
    position: Position,
    origin: Origin,
    shape: Shape,
    input: bool,
    output: bool,
}

#[derive(Clone)]
struct SymbolKind {
    name: SharedStr,
    designator_prefix: SharedStr,
    ports: &'static [PortDef],
    size: Size,
    origin: Origin,
    shape: Shape,
}

const PORT_HALF_WIDTH: f32 = 1.5;

const PORT_ORIGIN: Origin = Origin {
    x: PORT_HALF_WIDTH,
    y: PORT_HALF_WIDTH,
};
const PORT_SHAPE: Shape = Shape::Port; // todo: fixme

const GATE_PORTS_2_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Position { x: 0.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("B"),
        position: Position { x: 0.0, y: 40.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Position { x: 80.0, y: 20.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: false,
        output: true,
    },
];

const GATE_PORTS_1_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Position { x: 0.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Position { x: 40.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: false,
        output: true,
    },
];

const KINDS: &[SymbolKind] = &[
    SymbolKind {
        name: SharedStr::new_static("AND"),
        designator_prefix: SharedStr::new_static("U"),
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape::And,                  // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("OR"),
        designator_prefix: SharedStr::new_static("U"),
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape::Or,                   // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("XOR"),
        designator_prefix: SharedStr::new_static("U"),
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape::Xor,                  // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("NOT"),
        designator_prefix: SharedStr::new_static("U"),
        size: Size {
            width: 60.0,
            height: 40.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape::Not,                  // TODO: fixme
        ports: GATE_PORTS_1_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("IN"),
        designator_prefix: SharedStr::new_static("J"),
        size: Size {
            width: 40.0,
            height: 20.0,
        },
        origin: Origin { x: 40.0, y: 10.0 }, // position of the first port
        shape: Shape::Input,                 // TODO: fixme
        ports: &[PortDef {
            name: SharedStr::new_static("I"),
            position: Position { x: 0.0, y: 0.0 },
            origin: PORT_ORIGIN,
            bit_width: 1,
            shape: PORT_SHAPE,
            input: false,
            output: true,
        }],
    },
    SymbolKind {
        name: SharedStr::new_static("OUT"),
        designator_prefix: SharedStr::new_static("J"),
        size: Size {
            width: 40.0,
            height: 20.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape::Output,               // TODO: fixme
        ports: &[PortDef {
            name: SharedStr::new_static("O"),
            position: Position { x: 0.0, y: 0.0 },
            origin: PORT_ORIGIN,
            bit_width: 1,
            shape: PORT_SHAPE,
            input: true,
            output: false,
        }],
    },
];

#[derive(Resource)]
pub struct SymbolRegistry {
    kinds: Vec<SymbolKind>,
}

impl Default for SymbolRegistry {
    fn default() -> Self {
        Self { kinds: KINDS.to_vec() }
    }
}
