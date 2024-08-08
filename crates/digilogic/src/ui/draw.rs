use super::{Layer, Scene, Viewport};
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::Read;
use bitflags::bitflags;
use digilogic_core::components::{Child, CircuitID, Shape};
use digilogic_core::transform::{AbsoluteBoundingBox, Direction, GlobalTransform};
use digilogic_core::visibility::ComputedVisibility;
use vello::kurbo::{Affine, BezPath, Circle, Line, Rect, Shape as _, Stroke, Vec2};
use vello::peniko::{Brush, Color, Fill};

include!("bez_path.rs");

bitflags! {
    pub struct PathKind: u8 {
        const FILL = 0x1;
        const STROKE = 0x2;
    }
}

struct PathInfo {
    kind: PathKind,
    path: BezPath,
}

#[derive(Default)]
pub struct SymbolShape {
    paths: Vec<PathInfo>,
}

#[derive(Default, Resource)]
pub struct SymbolShapes(pub Vec<SymbolShape>);

const SYMBOL_STROKE_WIDTH: f64 = 3.0;

type ShapeQuery<'w, 's> = Query<
    'w,
    's,
    (
        (
            Option<Read<Shape>>,
            Option<Read<GlobalTransform>>,
            Option<Read<ComputedVisibility>>,
        ),
        Relations<Child>,
    ),
>;

pub fn draw_symbols(
    symbol_shapes: Res<SymbolShapes>,
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    shapes: ShapeQuery,
) {
    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::Symbol);
        scene.reset();

        shapes
            .traverse::<Child>(std::iter::once(circuit.0))
            .for_each(|(shape, transform, visibility), _| {
                let &mut Some(shape) = shape else {
                    return;
                };

                if !*visibility.copied().unwrap_or_default() {
                    return;
                }

                let transform = transform.copied().unwrap_or_default();
                let transform = Affine::scale(transform.scale.to_f64())
                    .then_rotate(transform.rotation.radians())
                    .then_translate(Vec2::new(
                        transform.translation.x.to_f64(),
                        transform.translation.y.to_f64(),
                    ));

                let symbol_shape = &symbol_shapes.0[*shape as usize];
                for path in symbol_shape.paths.iter() {
                    if path.kind.contains(PathKind::FILL) {
                        scene.fill(
                            Fill::NonZero,
                            transform,
                            &Brush::Solid(Color::GRAY),
                            None,
                            &path.path,
                        );
                    }

                    if path.kind.contains(PathKind::STROKE) {
                        scene.stroke(
                            &Stroke::new(SYMBOL_STROKE_WIDTH),
                            transform,
                            &Brush::Solid(Color::WHITE),
                            None,
                            &path.path,
                        );
                    }
                }
            });
    }
}

pub fn draw_bounding_boxes(
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    boxes: Query<(Option<&AbsoluteBoundingBox>, Relations<Child>)>,
) {
    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::BoundingBox);
        scene.reset();

        boxes
            .traverse::<Child>(std::iter::once(circuit.0))
            .for_each(|bounds, _| {
                let &mut Some(bounds) = bounds else {
                    return;
                };

                scene.stroke(
                    &Stroke::new(1.0),
                    Affine::IDENTITY,
                    &Brush::Solid(Color::RED),
                    None,
                    &Rect::new(
                        bounds.min.x.to_f64(),
                        bounds.min.y.to_f64(),
                        bounds.max.x.to_f64(),
                        bounds.max.y.to_f64(),
                    ),
                );
            });
    }
}

pub fn draw_routing_graph(
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    graphs: Query<Ref<digilogic_routing::Graph>>,
) {
    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::RoutingGraph);
        scene.reset();

        if let Ok(graph) = graphs.get(circuit.0) {
            for node in graph.nodes() {
                let node_pos = (node.position.x.to_f64(), node.position.y.to_f64());

                for dir in [Direction::PosX, Direction::PosY] {
                    if let Some(neighbor_index) = node.get_neighbor(dir) {
                        let neighbor = &graph.nodes()[neighbor_index];
                        let neighbor_pos =
                            (neighbor.position.x.to_f64(), neighbor.position.y.to_f64());

                        scene.stroke(
                            &Stroke::new(1.0),
                            Affine::IDENTITY,
                            &Brush::Solid(Color::LIGHT_SKY_BLUE),
                            None,
                            &Line::new(node_pos, neighbor_pos),
                        );
                    }
                }

                scene.fill(
                    Fill::NonZero,
                    Affine::IDENTITY,
                    &Brush::Solid(Color::DEEP_SKY_BLUE),
                    None,
                    &Circle::new(node_pos, 1.5),
                );
            }
        }
    }
}

fn scale_path(mut path: BezPath, scale: f64, translate: (f64, f64)) -> BezPath {
    path.apply_affine(Affine::scale(scale).then_translate(Vec2::new(translate.0, translate.1)));
    path
}

const GATE_SCALE: f64 = 12.5;
const GATE_TRANSLATE: (f64, f64) = (-34.5, -29.5);

const NOT_SCALE: f64 = 7.75;
const NOT_TRANSLATE: (f64, f64) = (-22.0, -22.75);

const INOUT_SCALE: f64 = 2.5;
const INPUT_TRANSLATE: (f64, f64) = (-46.5, -17.75);
const OUTPUT_TRANSLATE: (f64, f64) = (-12.0, -17.75);

pub fn init_symbol_shapes(mut symbol_svgs: ResMut<SymbolShapes>) {
    symbol_svgs.0 = vec![
        // Chip
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FILL,
                path: bez_path!(),
            }],
        },
        // Port
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::STROKE,
                path: scale_path(
                    Circle::new((1.5, 1.5), 1.5).path_elements(0.01).collect(),
                    1.0,
                    (-1.5, -1.5),
                ),
            }],
        },
        // And -- from schemalib-and2-l.svg
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FILL | PathKind::STROKE,
                path: scale_path(
                    bez_path!(M 5.9,7 H 3 V 1 L 5.9,1 C 7.7,1 9,2.2 9,4 9,5.8 7.4,7 5.9,7 Z),
                    GATE_SCALE,
                    GATE_TRANSLATE,
                ),
            }],
        },
        // Or -- from schemalib-or2-l.svg
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FILL | PathKind::STROKE,
                path: scale_path(
                    bez_path!(
                        M 3,7 H 4.4 C 6.7,7 7.7,6.9 9,4 7.7,1.1 6.7,1 4.4,1 H 3 C 4.4,3.1 4.4,4.9 3,7 Z
                    ),
                    GATE_SCALE,
                    GATE_TRANSLATE,
                ),
            }],
        },
        // Xor -- from schemalib-xor2-l.svg
        SymbolShape {
            paths: vec![
                PathInfo {
                    kind: PathKind::FILL | PathKind::STROKE,
                    path: scale_path(
                        bez_path!(
                            M 3,7 H 4.4 C 6.7,7 7.7,6.9 9,4 7.7,1.1 6.7,1 4.4,1 H 3 C 4.4,3.1 4.4,4.9 3,7 Z
                        ),
                        GATE_SCALE,
                        GATE_TRANSLATE,
                    ),
                },
                PathInfo {
                    kind: PathKind::STROKE,
                    path: scale_path(
                        bez_path!(
                            M 2.2,1 C 3.6,3.1 3.6,4.9 2.2,7
                        ),
                        GATE_SCALE,
                        GATE_TRANSLATE,
                    ),
                },
            ],
        },
        // Not -- from schemalib-inv-l.svg
        SymbolShape {
            paths: vec![
                PathInfo {
                    kind: PathKind::FILL | PathKind::STROKE,
                    path: scale_path(
                        bez_path!(
                            M 7,3.7 C 6.6,3.7 6.3,3.4 6.3,3 6.3,2.6 6.6,2.3 7,2.3 7.4,2.3 7.7,2.6 7.7,3 7.7,3.4 7.4,3.7 7,3.7 Z
                        ),
                        NOT_SCALE,
                        NOT_TRANSLATE,
                    ),
                },
                PathInfo {
                    kind: PathKind::FILL | PathKind::STROKE,
                    path: scale_path(
                        bez_path!(
                            M 6.3,3 3.3,1.5 V 4.5 L 6.3,3 Z
                        ),
                        NOT_SCALE,
                        NOT_TRANSLATE,
                    ),
                },
            ],
        },
        // Input
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FILL | PathKind::STROKE,
                path: scale_path(
                    bez_path!(M 14,1 H 1 V 13 H 14 L 18,7 Z),
                    INOUT_SCALE,
                    INPUT_TRANSLATE,
                ),
            }],
        },
        // Output
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FILL | PathKind::STROKE,
                path: scale_path(
                    bez_path!(M 10,1 H 23 V 13 H 10 L 6,7 Z),
                    INOUT_SCALE,
                    OUTPUT_TRANSLATE,
                ),
            }],
        },
    ];
}
