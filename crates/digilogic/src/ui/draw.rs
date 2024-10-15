use super::{Layer, PaletteBrushes, Scene, Viewport};
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::Read;
use bitflags::bitflags;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::visibility::ComputedVisibility;
use digilogic_routing::{VertexKind, Vertices};
use vello::kurbo::{Affine, BezPath, Cap, Circle, Join, Line, Rect, Stroke, Vec2};
use vello::peniko::{Color, Fill, Font};

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

type SymbolQuery<'w, 's> = Query<
    'w,
    's,
    (
        Read<Shape>,
        Read<GlobalTransform>,
        Read<ComputedVisibility>,
        Option<Read<digilogic_netcode::StateOffset>>,
        Option<Read<BitWidth>>,
        Has<Hovered>,
    ),
    With<Symbol>,
>;

#[derive(Resource)]
pub struct VelloFont(pub Font);

pub fn draw_symbols(
    symbol_shapes: Res<SymbolShapes>,
    palette: Res<PaletteBrushes>,
    font: Res<VelloFont>,
    sim_state: Option<Res<digilogic_netcode::SimState>>,
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    children: Query<(Entity, Relations<Child>)>,
    symbols: SymbolQuery,
) {
    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::Symbol);
        scene.reset();

        children
            .traverse::<Child>(std::iter::once(circuit.0))
            .for_each(|&mut entity, _| {
                let Ok((shape, transform, &visibility, state_offset, bit_width, hovered)) =
                    symbols.get(entity)
                else {
                    return;
                };

                if !*visibility {
                    return;
                }

                let transform = Affine::scale(transform.scale.to_f64())
                    .then_rotate(transform.rotation.radians())
                    .then_translate(Vec2::new(
                        transform.translation.x.to_f64(),
                        transform.translation.y.to_f64(),
                    ));

                // TODO: figure out how to layout text, as draw requires a Glyph iterator
                //scene.draw_glyphs(&font.0).hint(true).font_size(12.0).draw();

                let symbol_shape = &symbol_shapes.0[*shape as usize];
                for path in symbol_shape.paths.iter() {
                    let color = palette
                        .get_color_for_state(
                            sim_state.as_deref(),
                            state_offset.copied(),
                            bit_width.copied(),
                        )
                        .unwrap_or(Color::rgb8(3, 3, 3));

                    if path.kind.contains(PathKind::FILL) {
                        scene.fill(Fill::NonZero, transform, color, None, &path.path);
                    }

                    if path.kind.contains(PathKind::STROKE) {
                        let (width, color) = if hovered {
                            (3.5, Color::WHITE)
                        } else {
                            (3.0, Color::rgb8(150, 150, 150))
                        };

                        scene.stroke(
                            &Stroke::new(width)
                                .with_join(Join::Miter)
                                .with_caps(Cap::Butt)
                                .with_miter_limit(2.2),
                            transform,
                            color,
                            None,
                            &path.path,
                        );
                    }
                }
            });
    }
}

type PortQuery<'w, 's> = Query<
    'w,
    's,
    (
        Read<GlobalTransform>,
        Read<ComputedVisibility>,
        Has<Input>,
        Has<Output>,
        Has<Hovered>,
    ),
    With<Port>,
>;

pub fn draw_ports(
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    children: Query<(Entity, Relations<Child>)>,
    ports: PortQuery,
) {
    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::Port);
        scene.reset();

        children
            .traverse::<Child>(std::iter::once(circuit.0))
            .for_each(|&mut entity, _| {
                let Ok(entity) = ports.get(entity) else {
                    return;
                };

                let (transform, &visibility, is_input, is_output, hovered) = entity;

                if !*visibility {
                    return;
                }

                let transform = Affine::scale(transform.scale.to_f64())
                    .then_rotate(transform.rotation.radians())
                    .then_translate(Vec2::new(
                        transform.translation.x.to_f64(),
                        transform.translation.y.to_f64(),
                    ));

                let color = match (is_input, is_output) {
                    (true, true) => Color::rgb8(232, 225, 40),
                    (true, false) => Color::rgb8(40, 110, 228),
                    (false, true) => Color::rgb8(240, 13, 13),
                    (false, false) => Color::rgb8(140, 140, 140),
                };

                let radius = if hovered { 6.0 } else { 4.0 };

                scene.fill(
                    Fill::NonZero,
                    transform,
                    color,
                    None,
                    &Circle::new((0.0, 0.0), radius),
                );
            });
    }
}

type VertexQuery<'w, 's> = Query<
    'w,
    's,
    (
        (
            Option<Read<Vertices>>,
            Option<Read<ComputedVisibility>>,
            Option<Read<digilogic_netcode::StateOffset>>,
            Option<Read<BitWidth>>,
            Has<Hovered>,
        ),
        Relations<Child>,
    ),
>;

pub fn draw_wires(
    app_state: Res<crate::AppSettings>,
    palette: Res<PaletteBrushes>,
    sim_state: Option<Res<digilogic_netcode::SimState>>,
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    vertices: VertexQuery,
) {
    let brush_transform = palette.get_brush_transform();

    for (scene, circuit) in viewports.iter() {
        let mut scene = scene.for_layer(Layer::Wire);
        scene.reset();

        vertices
            .traverse::<Child>(std::iter::once(circuit.0))
            .for_each(
                |&mut (vertices, visibility, state_offset, bit_width, hovered), _| {
                    let Some(vertices) = vertices else {
                        return;
                    };

                    if !*visibility.copied().unwrap_or_default() {
                        return;
                    }

                    let brush = palette.get_brush_for_state(
                        sim_state.as_deref(),
                        state_offset.copied(),
                        bit_width.copied(),
                    );

                    let brush_transform = brush.is_some().then_some(brush_transform);

                    let (width, radius) = if hovered && brush.is_none() {
                        (3.0, 4.5)
                    } else {
                        (2.5, 4.0)
                    };

                    let mut path = BezPath::new();
                    let mut is_root_path = false;

                    for vertex in vertices.iter() {
                        let pos = (vertex.position.x.to_f64(), vertex.position.y.to_f64());

                        match vertex.kind {
                            VertexKind::Normal | VertexKind::Dummy => path.line_to(pos),
                            VertexKind::WireStart { is_root } => {
                                path = BezPath::new();
                                path.move_to(pos);
                                is_root_path = is_root;
                            }
                            VertexKind::WireEnd { junction_kind } => {
                                let brush = brush.unwrap_or_else(|| {
                                    let is_root = is_root_path && app_state.show_root_wires;

                                    match (is_root, hovered) {
                                        (true, true) => Color::rgb8(245, 220, 116).into(),
                                        (true, false) => Color::rgb8(208, 166, 2).into(),
                                        (false, true) => Color::rgb8(125, 240, 147).into(),
                                        (false, false) => Color::rgb8(8, 190, 42).into(),
                                    }
                                });

                                path.line_to(pos);

                                scene.stroke(
                                    &Stroke::new(width),
                                    Affine::IDENTITY,
                                    brush,
                                    brush_transform,
                                    &path,
                                );

                                if junction_kind.is_some() {
                                    scene.fill(
                                        Fill::NonZero,
                                        Affine::IDENTITY,
                                        brush,
                                        brush_transform,
                                        &Circle::new(pos, radius),
                                    );
                                }
                            }
                        }
                    }
                },
            );
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
            .for_each(|&mut bounds, _| {
                let Some(bounds) = bounds else {
                    return;
                };

                scene.stroke(
                    &Stroke::new(1.0),
                    Affine::IDENTITY,
                    Color::RED,
                    None,
                    &Rect::new(
                        bounds.min().x.to_f64(),
                        bounds.min().y.to_f64(),
                        bounds.max().x.to_f64(),
                        bounds.max().y.to_f64(),
                    ),
                );
            });
    }
}

pub fn draw_routing_graph(
    viewports: Query<(&Scene, &CircuitID), With<Viewport>>,
    graphs: Query<Ref<digilogic_routing::graph::Graph>>,
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
                            Color::LIGHT_SKY_BLUE,
                            None,
                            &Line::new(node_pos, neighbor_pos),
                        );
                    }
                }
            }

            for node in graph.nodes() {
                let node_pos = (node.position.x.to_f64(), node.position.y.to_f64());

                let node_color = if node.is_explicit {
                    Color::HOT_PINK
                } else {
                    Color::DEEP_SKY_BLUE
                };

                scene.fill(
                    Fill::NonZero,
                    Affine::IDENTITY,
                    node_color,
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
