use crate::bundles::*;
use crate::components::*;
use crate::SharedStr;
use bevy_ecs::prelude::*;
use bevy_hierarchy::BuildChildren;

#[derive(Clone)]
struct PortDef {
    name: &'static str,
    bit_width: u8,
    position: Position,
    origin: Origin,
    shape: Shape,
    input: bool,
    output: bool,
}

struct SymbolKindDef {
    name: &'static str,
    designator_prefix: &'static str,
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

const PORT_SHAPE: Shape = Shape(0); // todo: fixme

const GATE_PORTS_2_INPUT: &[PortDef] = &[
    PortDef {
        name: "A",
        position: Position { x: 0.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: "B",
        position: Position { x: 0.0, y: 40.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: "Y",
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
        name: "A",
        position: Position { x: 0.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: true,
        output: false,
    },
    PortDef {
        name: "Y",
        position: Position { x: 40.0, y: 0.0 },
        origin: PORT_ORIGIN,
        bit_width: 1,
        shape: PORT_SHAPE,
        input: false,
        output: true,
    },
];

const KINDS: &[SymbolKindDef] = &[
    SymbolKindDef {
        name: "AND",
        designator_prefix: "U",
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape(1),                    // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKindDef {
        name: "OR",
        designator_prefix: "U",
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape(2),                    // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKindDef {
        name: "XOR",
        designator_prefix: "U",
        size: Size {
            width: 80.0,
            height: 60.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape(3),                    // TODO: fixme
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKindDef {
        name: "NOT",
        designator_prefix: "U",
        size: Size {
            width: 60.0,
            height: 40.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape(4),                    // TODO: fixme
        ports: GATE_PORTS_1_INPUT,
    },
    SymbolKindDef {
        name: "IN",
        designator_prefix: "J",
        size: Size {
            width: 40.0,
            height: 20.0,
        },
        origin: Origin { x: 40.0, y: 10.0 }, // position of the first port
        shape: Shape(5),                     // TODO: fixme
        ports: &[PortDef {
            name: "I",
            position: Position { x: 0.0, y: 0.0 },
            origin: PORT_ORIGIN,
            bit_width: 1,
            shape: PORT_SHAPE,
            input: false,
            output: true,
        }],
    },
    SymbolKindDef {
        name: "OUT",
        designator_prefix: "J",
        size: Size {
            width: 40.0,
            height: 20.0,
        },
        origin: Origin { x: 0.0, y: 10.0 }, // position of the first port
        shape: Shape(6),                    // TODO: fixme
        ports: &[PortDef {
            name: "O",
            position: Position { x: 0.0, y: 0.0 },
            origin: PORT_ORIGIN,
            bit_width: 1,
            shape: PORT_SHAPE,
            input: true,
            output: false,
        }],
    },
];

pub fn init_builtin_symbol_kinds(mut commands: Commands) {
    for kind in KINDS.iter() {
        let kind_id = commands
            .spawn(SymbolKindBundle {
                marker: SymbolKind,
                visible: Visible {
                    shape: kind.shape,
                    origin: kind.origin,
                    ..Default::default()
                },
                name: Name(SharedStr::new_static(kind.name)),
                size: kind.size,
                designator_prefix: DesignatorPrefix(kind.designator_prefix.into()),
            })
            .id();

        for port in kind.ports.iter() {
            let mut port_cmd = commands.spawn(PortBundle {
                marker: Port,
                name: Name(SharedStr::new_static(port.name)),
                visible: Visible {
                    shape: port.shape,
                    origin: port.origin,
                    position: port.position,
                    ..Default::default()
                },
                bit_width: BitWidth(port.bit_width),
            });
            port_cmd.set_parent(kind_id);
            if port.input {
                port_cmd.insert(Input);
            }
            if port.output {
                port_cmd.insert(Output);
            }
        }
    }
}
