use crate::bundles::{PortBundle, SymbolBundle};
use crate::components::*;
use crate::transform::*;
use crate::visibility::VisibilityBundle;
use crate::{fixed, Fixed, SharedStr};
use bevy_ecs::prelude::*;
use bevy_hierarchy::BuildChildren;
use smallvec::SmallVec;

#[derive(Clone)]
struct PortDef {
    name: SharedStr,
    position: Vec2,
    input: bool,
    output: bool,
    directions: Directions,
}

#[derive(Clone)]
pub struct SymbolDef {
    kind: SymbolKind,
    name: SharedStr,
    designator_prefix: SharedStr,
    ports: &'static [PortDef],
    bounding_box: BoundingBox,
    shape: Shape,
}

const PORT_HALF_WIDTH: Fixed = fixed!(2);
const PORT_SHAPE: Shape = Shape::Port; // todo: fixme

const GATE_PORTS_2_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Vec2 {
            x: fixed!(0),
            y: fixed!(0),
        },
        input: true,
        output: false,
        directions: Directions::NEG_X,
    },
    PortDef {
        name: SharedStr::new_static("B"),
        position: Vec2 {
            x: fixed!(0),
            y: fixed!(40),
        },
        input: true,
        output: false,
        directions: Directions::NEG_X,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Vec2 {
            x: fixed!(80),
            y: fixed!(20),
        },
        input: false,
        output: true,
        directions: Directions::POS_X,
    },
];

const GATE_PORTS_1_INPUT: &[PortDef] = &[
    PortDef {
        name: SharedStr::new_static("A"),
        position: Vec2 {
            x: fixed!(0),
            y: fixed!(0),
        },
        input: true,
        output: false,
        directions: Directions::NEG_X,
    },
    PortDef {
        name: SharedStr::new_static("Y"),
        position: Vec2 {
            x: fixed!(40),
            y: fixed!(0),
        },
        input: false,
        output: true,
        directions: Directions::POS_X,
    },
];

const KINDS: &[SymbolDef] = &[
    SymbolDef {
        kind: SymbolKind::And,
        name: SharedStr::new_static("AND"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(0),
                y: fixed!(-10),
            },
            fixed!(80),
            fixed!(60),
        ),
        shape: Shape::And,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolDef {
        kind: SymbolKind::Or,
        name: SharedStr::new_static("OR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(0),
                y: fixed!(-10),
            },
            fixed!(80),
            fixed!(60),
        ),
        shape: Shape::Or,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolDef {
        kind: SymbolKind::Xor,
        name: SharedStr::new_static("XOR"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(0),
                y: fixed!(-10),
            },
            fixed!(80),
            fixed!(60),
        ),
        shape: Shape::Xor,
        ports: GATE_PORTS_2_INPUT,
    },
    SymbolDef {
        kind: SymbolKind::Not,
        name: SharedStr::new_static("NOT"),
        designator_prefix: SharedStr::new_static("U"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(0),
                y: fixed!(-10),
            },
            fixed!(40),
            fixed!(20),
        ),
        shape: Shape::Not,
        ports: GATE_PORTS_1_INPUT,
    },
    SymbolDef {
        kind: SymbolKind::In,
        name: SharedStr::new_static("IN"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(-40),
                y: fixed!(-20),
            },
            fixed!(40),
            fixed!(40),
        ),
        shape: Shape::Input,
        ports: &[PortDef {
            name: SharedStr::new_static("Y"),
            position: Vec2 {
                x: fixed!(0),
                y: fixed!(0),
            },
            input: false,
            output: true,
            directions: Directions::POS_X,
        }],
    },
    SymbolDef {
        kind: SymbolKind::Out,
        name: SharedStr::new_static("OUT"),
        designator_prefix: SharedStr::new_static("J"),
        bounding_box: BoundingBox::from_top_left_size(
            Vec2 {
                x: fixed!(0),
                y: fixed!(-20),
            },
            fixed!(40),
            fixed!(40),
        ),
        shape: Shape::Output,
        ports: &[PortDef {
            name: SharedStr::new_static("A"),
            position: Vec2 {
                x: fixed!(0),
                y: fixed!(0),
            },
            input: true,
            output: false,
            directions: Directions::NEG_X,
        }],
    },
];

pub struct PortInfo {
    pub name: SharedStr,
    pub id: Entity,
}

pub struct SymbolBuilder<'a> {
    registry: &'a SymbolRegistry,
    kind: SymbolKind,
    name: Option<SharedStr>,
    designator_number: Option<u32>,
    position: Option<Vec2>,
    bit_width: Option<BitWidth>,
    ports: SmallVec<[PortInfo; 7]>,
}

#[derive(Resource)]
pub struct SymbolRegistry {
    kinds: Vec<SymbolDef>,
}

impl SymbolRegistry {
    pub fn get(&self, kind: SymbolKind) -> SymbolBuilder {
        SymbolBuilder {
            registry: self,
            kind,
            name: None,
            designator_number: None,
            position: None,
            bit_width: None,
            ports: SmallVec::new(),
        }
    }

    pub fn get_by_name(&self, name: &SharedStr) -> Option<SymbolBuilder> {
        let def = self.kinds.iter().find(|kind| kind.name == *name);

        def.map(|kind| self.get(kind.kind))
    }

    pub fn get_by_index(&self, index: usize) -> Option<&SymbolDef> {
        self.kinds.get(index)
    }
}

impl Default for SymbolRegistry {
    fn default() -> Self {
        let list = Self {
            kinds: KINDS.to_vec(),
        };

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

    pub fn position(&mut self, position: Vec2) -> &mut Self {
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
        let kind = self.registry.kinds.get(self.kind as usize).unwrap();

        let symbol_id = commands
            .spawn(SymbolBundle {
                name: Name(self.name.as_ref().unwrap_or(&kind.name).clone()),
                designator_prefix: DesignatorPrefix(kind.designator_prefix.clone()),
                designator_number: DesignatorNumber(self.designator_number.unwrap_or_default()),
                symbol_kind: kind.kind,
                shape: kind.shape,
                transform: TransformBundle {
                    transform: Transform {
                        translation: self.position.unwrap_or_default(),
                        ..Default::default()
                    },
                    ..Default::default()
                },
                symbol: Symbol,
                visibility: VisibilityBundle::default(),
                bounds: BoundingBoxBundle {
                    bounding_box: kind.bounding_box,
                    ..Default::default()
                },
            })
            .set_parent(circuit_id)
            .id();

        self.ports = kind
            .ports
            .iter()
            .map(|port| {
                let id = port.build(commands, symbol_id, self.bit_width.unwrap_or(BitWidth(1)));
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
    fn build(&self, commands: &mut Commands, symbol_id: Entity, bit_width: BitWidth) -> Entity {
        commands
            .spawn(PortBundle {
                port: Port,
                name: Name(self.name.clone()),
                shape: PORT_SHAPE,
                transform: TransformBundle {
                    transform: Transform {
                        translation: self.position,
                        ..Default::default()
                    },
                    ..Default::default()
                },
                bit_width,
                visibility: VisibilityBundle::default(),
                bounds: BoundingBoxBundle {
                    bounding_box: BoundingBox::from_half_size(PORT_HALF_WIDTH, PORT_HALF_WIDTH),
                    ..Default::default()
                },
                directions: DirectionsBundle {
                    directions: self.directions,
                    ..Default::default()
                },
            })
            .set_parent(symbol_id)
            .id()
    }
}
