use crate::bundles::{PortBundle, SymbolBundle};
use crate::components::*;
use crate::transform::*;
use crate::visibility::VisibilityBundle;
use crate::SharedStr;
use bevy_ecs::prelude::*;
use bevy_hierarchy::BuildChildren;
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
            name: SharedStr::new_static("Y"),
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
            name: SharedStr::new_static("A"),
            position: Vec2i { x: 0, y: 0 },
            input: true,
            output: false,
        }],
    },
];

pub struct PortInfo {
    pub name: SharedStr,
    pub id: Entity,
}

pub struct SymbolBuilder<'a> {
    registry: &'a SymbolRegistry,
    kind_index: usize,
    name: Option<SharedStr>,
    designator_number: Option<u32>,
    position: Option<Vec2i>,
    bit_width: Option<BitWidth>,
    ports: SmallVec<[PortInfo; 7]>,
}

#[derive(Resource)]
pub struct SymbolRegistry {
    kinds: Vec<SymbolKind>,
}

impl SymbolRegistry {
    pub fn get(&self, name: &SharedStr) -> Option<SymbolBuilder> {
        let kind = self.kinds.iter().position(|kind| kind.name == *name);
        kind.map(|kind| SymbolBuilder {
            registry: self,
            kind_index: kind,
            name: None,
            designator_number: None,
            position: None,
            bit_width: None,
            ports: SmallVec::new(),
        })
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

impl SymbolBuilder<'_> {
    pub fn name(&mut self, name: SharedStr) -> &mut Self {
        self.name = Some(name);
        self
    }

    pub fn designator_number(&mut self, number: u32) -> &mut Self {
        self.designator_number = Some(number);
        self
    }

    pub fn position(&mut self, position: Vec2i) -> &mut Self {
        self.position = Some(position);
        self
    }

    pub fn bit_width(&mut self, bit_width: BitWidth) -> &mut Self {
        self.bit_width = Some(bit_width);
        self
    }

    pub fn ports(&self) -> &[PortInfo] {
        &self.ports
    }

    pub fn build(&mut self, commands: &mut Commands, circuit_id: Entity) -> Entity {
        let kind = self.registry.kinds.get(self.kind_index as usize).unwrap();

        let symbol_id = commands
            .spawn(SymbolBundle {
                name: Name(self.name.as_ref().unwrap_or(&kind.name).clone()),
                designator_prefix: DesignatorPrefix(kind.designator_prefix.clone()),
                designator_number: DesignatorNumber(self.designator_number.unwrap_or_default()),
                symbol_kind: SymbolKindIndex(kind.index.0),
                shape: kind.shape,
                transform: TransformBundle {
                    transform: Transform {
                        translation: self.position.unwrap_or_default(),
                        rotation: Rotation::Rot0,
                    },
                    ..Default::default()
                },
                symbol: Symbol::default(),
                visibility: VisibilityBundle::default(),
            })
            .set_parent(circuit_id)
            .id();

        self.ports = kind
            .ports
            .iter()
            .map(|port| {
                let id = port.build(
                    commands,
                    symbol_id,
                    self.bit_width.as_ref().unwrap_or(&BitWidth(1)),
                );
                PortInfo {
                    name: port.name.clone(),
                    id,
                }
            })
            .collect();

        symbol_id
    }
}

impl PortDef {
    fn build(&self, commands: &mut Commands, symbol_id: Entity, bit_width: &BitWidth) -> Entity {
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
                visibility: VisibilityBundle::default(),
            })
            .set_parent(symbol_id)
            .id()
    }
}
