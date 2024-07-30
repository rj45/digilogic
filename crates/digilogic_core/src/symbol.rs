use crate::components::*;
use crate::transform::*;
use crate::SharedStr;
use bevy_ecs::prelude::*;

#[derive(Clone)]
struct PortDef {
    name: SharedStr,
    position: Vec2i,
    input: bool,
    output: bool,
}

#[derive(Clone)]
struct SymbolKind {
    name: SharedStr,
    designator_prefix: SharedStr,
    ports: &'static [PortDef],
    bounding_box: BoundingBox,
    shape: Shape,
}

const PORT_HALF_WIDTH: i32 = 2;
const PORT_SHAPE: Shape = Shape::Port; // todo: fixme

const GATE_PORTS_2_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Vec2i { x: 0, y: 0 },
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("B"),
        position: Vec2i { x: 0, y: 40 },
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Vec2i { x: 80, y: 20 },
        input: false,
        output: true,
    },
];

const GATE_PORTS_1_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Vec2i { x: 0, y: 0 },
        input: true,
        output: false,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Vec2i { x: 40, y: 0 },
        input: false,
        output: true,
    },
];

const KINDS: &[SymbolKind] = &[
    SymbolKind {
        name: SharedStr::new_static("AND"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::And, // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("OR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::Or, // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("XOR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::Xor, // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("NOT"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(30, 20),
        shape: Shape::Not, // TODO: fixme
        ports: GATE_PORTS_1_INPUT,
    },
    SymbolKind {
        name: SharedStr::new_static("IN"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_half_size(20, 10),
        shape: Shape::Input, // TODO: fixme
        ports: &[PortDef {
            name: SharedStr::new_static("I"),
            position: Vec2i { x: 0, y: 0 },
            input: false,
            output: true,
        }],
    },
    SymbolKind {
        name: SharedStr::new_static("OUT"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_half_size(20, 10),
        shape: Shape::Output, // TODO: fixme
        ports: &[PortDef {
            name: SharedStr::new_static("O"),
            position: Vec2i { x: 0, y: 0 },
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
        Self {
            kinds: KINDS.to_vec(),
        }
    }
}
