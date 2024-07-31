use bevy_ecs::prelude::*;
use digilogic_core::{components::Shape, transform::GlobalTransform};
use vello::{
    kurbo::{Affine, BezPath, Stroke, Vec2},
    peniko::{Brush, Color, Fill},
};

include!("bez_path.rs");

#[derive(Default, Resource)]
pub struct Scene(pub vello::Scene);

pub enum PathKind {
    Fill,
    Stroke,
    FillStroke,
}

struct PathInfo {
    kind: PathKind,
    path: BezPath,
}

#[derive(Default)]
pub struct SymbolShape {
    paths: Vec<PathInfo>,
    scale: f32,
}

#[derive(Default, Resource)]
pub struct SymbolShapes(pub Vec<SymbolShape>);

pub fn draw(
    mut scene: ResMut<Scene>,
    symbol_svgs: Res<SymbolShapes>,
    shapes: Query<(&Shape, &GlobalTransform)>,
) {
    let scene = &mut scene.0;
    scene.reset();

    for (&shape, transform) in shapes.iter() {
        let symbolShape = &symbol_svgs.0[shape as usize];
        let transform = Affine::scale(symbolShape.scale as f64)
            .then_rotate(transform.rotation.radians())
            .then_translate(Vec2::new(
                transform.translation.x as f64,
                transform.translation.y as f64,
            ));

        for path in symbolShape.paths.iter() {
            match path.kind {
                PathKind::Fill => scene.fill(
                    Fill::NonZero,
                    transform,
                    &Brush::Solid(Color::GRAY),
                    None,
                    &path.path,
                ),
                PathKind::Stroke => scene.stroke(
                    &Stroke::new(0.5),
                    transform,
                    &Brush::Solid(Color::WHITE),
                    None,
                    &path.path,
                ),
                PathKind::FillStroke => {
                    scene.fill(
                        Fill::NonZero,
                        transform,
                        &Brush::Solid(Color::GRAY),
                        None,
                        &path.path,
                    );
                    scene.stroke(
                        &Stroke::new(0.5),
                        transform,
                        &Brush::Solid(Color::WHITE),
                        None,
                        &path.path,
                    );
                }
            }
        }
    }
}

pub fn init_symbol_shapes(mut symbol_svgs: ResMut<SymbolShapes>) {
    symbol_svgs.0 = vec![
        // Chip
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::Fill,
                path: bez_path!(),
            }],
            scale: 10.0,
        },
        // Port
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::Fill,
                path: bez_path!(),
            }],
            scale: 10.0,
        },
        // And -- from schemalib-and2-l.svg
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FillStroke,
                path: bez_path!(M 5.9,7 H 3 V 1 L 5.9,1 C 7.7,1 9,2.2 9,4 9,5.8 7.4,7 5.9,7 Z),
            }],
            scale: 10.0,
        },
        // Or -- from schemalib-or2-l.svg
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FillStroke,
                path: bez_path!(
                    M 3,7 H 4.4 C 6.7,7 7.7,6.9 9,4 7.7,1.1 6.7,1 4.4,1 H 3 C 4.4,3.1 4.4,4.9 3,7 Z
                ),
            }],
            scale: 10.0,
        },
        // Xor -- from schemalib-xor2-l.svg
        SymbolShape {
            paths: vec![
                PathInfo {
                    kind: PathKind::FillStroke,
                    path: bez_path!(
                        M 3,7 H 4.4 C 6.7,7 7.7,6.9 9,4 7.7,1.1 6.7,1 4.4,1 H 3 C 4.4,3.1 4.4,4.9 3,7 Z
                    ),
                },
                PathInfo {
                    kind: PathKind::Stroke,
                    path: bez_path!(
                        M 2.2,1 C 3.6,3.1 3.6,4.9 2.2,7
                    ),
                },
            ],
            scale: 10.0,
        },
        // Not -- from schemalib-inv-l.svg
        SymbolShape {
            paths: vec![
                PathInfo {
                    kind: PathKind::FillStroke,
                    path: bez_path!(
                        M 7,3.7 C 6.6,3.7 6.3,3.4 6.3,3 6.3,2.6 6.6,2.3 7,2.3 7.4,2.3 7.7,2.6 7.7,3 7.7,3.4 7.4,3.7 7,3.7 Z
                    ),
                },
                PathInfo {
                    kind: PathKind::FillStroke,
                    path: bez_path!(
                        M 6.3,3 3.3,1.5 V 4.5 L 6.3,3 Z
                    ),
                },
            ],
            scale: 10.0,
        },
        // Input
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FillStroke,
                path: bez_path!(M 14,1 H 1 V 13 H 14 L 18,7 Z),
            }],
            scale: 4.0,
        },
        // Output
        SymbolShape {
            paths: vec![PathInfo {
                kind: PathKind::FillStroke,
                path: bez_path!(M 10,1 H 23 V 13 H 10 L 6,7 Z),
            }],
            scale: 4.0,
        },
    ];
}
