use crate::bundles::{PortBundle, SymbolBundle};
use crate::components::*;
use crate::transform::*;
use crate::SharedStr;
use bevy_ecs::prelude::*;
use smallvec::SmallVec;

#[derive(Clone)]
struct PortDef {
    name: SharedStr,
    position: Vec2i,
    input: bool,
    output: bool,
}

#[derive(Clone)]
pub struct SymbolKind {
    index: SymbolKindIndex,
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
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("AND"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::And,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("OR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::Or,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("XOR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(40, 30),
        shape: Shape::Xor,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolKind {
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("NOT"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_half_size(30, 20),
        shape: Shape::Not,
        ports: GATE_PORTS_1_INPUT,
    },
    SymbolKind {
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("IN"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_half_size(20, 10),
        shape: Shape::Input,
        ports: &[PortDef {
            name: SharedStr::new_static("I"),
            position: Vec2i { x: 0, y: 0 },
            input: false,
            output: true,
        }],
    },
    SymbolKind {
        index: SymbolKindIndex(0),
        name: SharedStr::new_static("OUT"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_half_size(20, 10),
        shape: Shape::Output,
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

impl SymbolRegistry {
    pub fn get(&self, name: &SharedStr) -> Option<&SymbolKind> {
        self.kinds.iter().find(|kind| kind.name == *name)
    }

    pub fn get_by_index(&self, index: usize) -> Option<&SymbolKind> {
        self.kinds.get(index)
    }
}

impl Default for SymbolRegistry {
    fn default() -> Self {
        let mut list = Self {
            kinds: KINDS.to_vec(),
        };

        for (i, kind) in list.kinds.iter_mut().enumerate() {
            kind.index = SymbolKindIndex(i);
        }

        list
    }
}

impl SymbolKind {
    pub fn build(
        &self,
        commands: &mut Commands,
        circuit_id: Entity,
        designator_number: u32,
        bit_width: BitWidth,
    ) -> Entity {
        let ports: SmallVec<[Entity; 7]> = self
            .ports
            .iter()
            .map(|port| port.build(commands, &bit_width))
            .collect();
        let symbol_id = commands
            .spawn(SymbolBundle {
                symbol: Symbol { ports: ports },
                name: Name(self.name.clone()),
                designator_prefix: DesignatorPrefix(self.designator_prefix.clone()),
                designator_number: DesignatorNumber(designator_number),
                symbol_kind: SymbolKindIndex(self.index.0),
                shape: self.shape,
                transform: TransformBundle {
                    transform: Transform {
                        translation: Vec2i::default(),
                        rotation: Rotation::Rot0,
                    },
                    global_transform: GlobalTransform::default(),
                },
            })
            .insert(CircuitID(circuit_id))
            .id();

        commands.add(move |world: &mut World| {
            let mut circuit = world.get_mut::<Circuit>(circuit_id).unwrap();
            circuit.symbols.push(symbol_id);
        });

        symbol_id
    }
}

impl PortDef {
    fn build(&self, commands: &mut Commands, bit_width: &BitWidth) -> Entity {
        commands
            .spawn(PortBundle {
                port: Port,
                name: Name(self.name.clone()),
                shape: PORT_SHAPE,
                transform: TransformBundle {
                    transform: Transform {
                        translation: self.position,
                        rotation: Rotation::Rot0,
                    },
                    global_transform: GlobalTransform::default(),
                },
                bit_width: BitWidth(bit_width.0),
            })
            .id()
    }
}
