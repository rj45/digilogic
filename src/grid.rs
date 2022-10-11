use bevy::prelude::*;
use bevy_prototype_lyon::prelude::*;

const SPACING: f32 = 50.;
const DIMS: i32 = 1000;
const SIZE: f32 = DIMS as f32 * SPACING;

pub fn setup(mut commands: Commands) {
    let mut path_builder = PathBuilder::new();

    for y in -DIMS..DIMS {
        path_builder.move_to(Vec2 {
            x: -SIZE,
            y: y as f32 * SPACING,
        });
        path_builder.line_to(Vec2 {
            x: SIZE,
            y: y as f32 * SPACING,
        });
    }

    for x in -DIMS..DIMS {
        path_builder.move_to(Vec2 {
            x: x as f32 * SPACING,
            y: -SIZE,
        });
        path_builder.line_to(Vec2 {
            x: x as f32 * SPACING,
            y: SIZE,
        });
    }

    let grid = path_builder.build();

    commands.spawn_bundle(GeometryBuilder::build_as(
        &grid,
        DrawMode::Stroke(StrokeMode::new(Color::DARK_GRAY, 2.0)),
        Transform::default().with_translation(Vec3::Z * -0.01),
    ));
}
