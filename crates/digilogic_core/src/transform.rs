use bevy_ecs::prelude::*;

/// The position of the entity
#[derive(Default, Component, Clone, Copy)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}

/// The origin of the entity in its own coordinate system
#[derive(Default, Component, Clone, Copy)]
pub struct Origin {
    pub x: i32,
    pub y: i32,
}

/// The size of the entity
#[derive(Default, Component, Clone, Copy)]
pub struct Size {
    pub width: i32,
    pub height: i32,
}

/// The rotation of the entity in 90 degree counter-clockwise increments
#[derive(Default, Component, Clone, Copy)]
pub enum Rotation {
    #[default]
    Rot0,
    Rot90,
    Rot180,
    Rot270,
}

/// The computed absolute bounding box of the entity
#[derive(Default, Component)]
pub struct BoundingBox {
    min_x: i32,
    min_y: i32,
    max_x: i32,
    max_y: i32,
}

impl BoundingBox {
    #[inline]
    pub fn min_x(&self) -> i32 {
        self.min_x
    }

    #[inline]
    pub fn min_y(&self) -> i32 {
        self.min_y
    }

    #[inline]
    pub fn max_x(&self) -> i32 {
        self.max_x
    }

    #[inline]
    pub fn max_y(&self) -> i32 {
        self.max_y
    }
}

pub(crate) fn update_bounding_boxes(
    mut query: Query<
        (
            &mut BoundingBox,
            &Position,
            &Size,
            Option<&Origin>,
            Option<&Rotation>,
        ),
        Or<(
            Changed<Position>,
            Changed<Size>,
            Changed<Origin>,
            Changed<Rotation>,
        )>,
    >,
) {
    for (mut bb, pos, size, orig, rot) in query.iter_mut() {
        let orig = orig.copied().unwrap_or_default();
        let rot = rot.copied().unwrap_or_default();

        match rot {
            Rotation::Rot0 => {
                bb.min_x = orig.x;
                bb.min_y = orig.y;
                bb.max_x = bb.min_x + size.width;
                bb.max_y = bb.min_y + size.height;
            }
            Rotation::Rot90 => {
                bb.min_x = -orig.y;
                bb.min_y = orig.x;
                bb.max_x = -(bb.min_y + size.height);
                bb.max_y = bb.min_x + size.width;
            }
            Rotation::Rot180 => {
                bb.min_x = -orig.x;
                bb.min_y = -orig.y;
                bb.max_x = -(bb.min_x + size.width);
                bb.max_y = -(bb.min_y + size.height);
            }
            Rotation::Rot270 => {
                bb.min_x = orig.y;
                bb.min_y = -orig.x;
                bb.max_x = bb.min_y + size.height;
                bb.max_y = -(bb.min_x + size.width);
            }
        }

        bb.min_x += pos.x;
        bb.min_y += pos.y;
        bb.max_x += pos.x;
        bb.max_y += pos.y;
    }
}
