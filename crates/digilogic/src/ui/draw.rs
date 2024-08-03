use super::{PanZoom, Scene, Viewport};
use bevy_ecs::prelude::*;
use bevy_hierarchy::prelude::*;
use bitflags::bitflags;
use digilogic_core::components::CircuitID;
use digilogic_core::transform::GlobalTransform;
use digilogic_core::visibility::ComputedVisibility;
use vello::kurbo::{Affine, BezPath, Shape, Stroke, Vec2};
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

pub fn draw(
    symbol_shapes: Res<SymbolShapes>,
    mut viewports: Query<(&PanZoom, &mut Scene, &CircuitID), With<Viewport>>,
    children: Query<&Children>,
    shapes: Query<(
        &digilogic_core::components::Shape,
        Option<&GlobalTransform>,
        Option<&ComputedVisibility>,
    )>,
) {
    for (pan_zoom, mut scene, circuit) in viewports.iter_mut() {
        scene.reset();

        draw_recurse(
            &children,
            circuit.0,
            &shapes,
            pan_zoom,
            &symbol_shapes,
            &mut scene,
        );
    }
}

fn draw_recurse(
    children: &Query<&Children>,
    parent: Entity,
    shapes: &Query<(
        &digilogic_core::components::Shape,
        Option<&GlobalTransform>,
        Option<&ComputedVisibility>,
    )>,
    pan_zoom: &PanZoom,
    symbol_shapes: &Res<SymbolShapes>,
    scene: &mut Mut<Scene>,
) {
    for child in children.iter_descendants(parent) {
        if let Ok((&shape, transform, vis)) = shapes.get(child) {
            if !*vis.copied().unwrap_or_default() {
                continue;
            }

            let transform = transform.copied().unwrap_or_default();
            let transform = Affine::rotate(transform.rotation.radians())
                .then_translate(Vec2::new(
                    transform.translation.x as f64,
                    transform.translation.y as f64,
                ))
                .then_translate(Vec2::new(pan_zoom.pan.x as f64, pan_zoom.pan.y as f64))
                .then_scale(pan_zoom.zoom as f64);

            let symbol_shape = &symbol_shapes.0[shape as usize];
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

            draw_recurse(children, child, shapes, pan_zoom, symbol_shapes, scene);
        }
    }
}

fn scale_path(mut path: BezPath, scale: f64, translate: (f64, f64)) -> BezPath {
    path.apply_affine(Affine::scale(scale).then_translate(Vec2::new(translate.0, translate.1)));
    path
}

const GATE_SCALE: f64 = 12.5;
const GATE_TRANSLATE: (f64, f64) = (-32.5, -28.5);

const NOT_SCALE: f64 = 8.0;
const NOT_TRANSLATE: (f64, f64) = (-21.5, -22.0);

const INOUT_SCALE: f64 = 3.0;
const INPUT_TRANSLATE: (f64, f64) = (-55.0, -19.5);
const OUTPUT_TRANSLATE: (f64, f64) = (-15.0, -19.5);

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
                path: vello::kurbo::Circle::new((1.5, 1.5), 1.5)
                    .path_elements(0.01)
                    .collect(),
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
