use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_hierarchy::Parent;
use bevy_reflect::Reflect;
use std::ops::{Add, AddAssign, Mul, MulAssign, Sub, SubAssign};

macro_rules! const_min {
    ($a:expr, $b:expr) => {
        if $a < $b {
            $a
        } else {
            $b
        }
    };
}

macro_rules! const_max {
    ($a:expr, $b:expr) => {
        if $a > $b {
            $a
        } else {
            $b
        }
    };
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect)]
#[repr(C)]
pub struct Vec2i {
    pub x: i32,
    pub y: i32,
}

impl Vec2i {
    pub const ZERO: Self = Self { x: 0, y: 0 };

    #[inline]
    pub const fn min(self, rhs: Self) -> Self {
        Self {
            x: const_min!(self.x, rhs.y),
            y: const_min!(self.x, rhs.y),
        }
    }

    #[inline]
    pub const fn max(self, rhs: Self) -> Self {
        Self {
            x: const_max!(self.x, rhs.y),
            y: const_max!(self.x, rhs.y),
        }
    }

    #[inline]
    pub const fn manhatten_distance_to(self, other: Self) -> u32 {
        self.x.abs_diff(other.x) + self.y.abs_diff(other.y)
    }
}

impl Default for Vec2i {
    #[inline]
    fn default() -> Self {
        Self::ZERO
    }
}

impl Add for Vec2i {
    type Output = Self;

    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

impl AddAssign for Vec2i {
    #[inline]
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl Sub for Vec2i {
    type Output = Self;

    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
        }
    }
}

impl SubAssign for Vec2i {
    #[inline]
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect)]
#[repr(u8)]
pub enum Rotation {
    #[default]
    Rot0 = 0,
    Rot90 = 1,
    Rot180 = 2,
    Rot270 = 3,
}

impl Rotation {
    #[inline]
    pub fn radians(self) -> f64 {
        ((self as u8) as f64) * std::f64::consts::FRAC_PI_2
    }
}

impl Mul for Rotation {
    type Output = Self;

    #[inline]
    fn mul(self, rhs: Self) -> Self::Output {
        match ((self as u8) + (rhs as u8)) % 4 {
            0 => Self::Rot0,
            1 => Self::Rot90,
            2 => Self::Rot180,
            3 => Self::Rot270,
            _ => unreachable!(),
        }
    }
}

impl MulAssign for Rotation {
    #[inline]
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl Vec2i {
    pub fn rotate(self, rotation: Rotation) -> Self {
        match rotation {
            Rotation::Rot0 => self,
            Rotation::Rot90 => Self {
                x: -self.y,
                y: self.x,
            },
            Rotation::Rot180 => Self {
                x: -self.x,
                y: -self.y,
            },
            Rotation::Rot270 => Self {
                x: self.y,
                y: -self.x,
            },
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Component, Reflect)]
pub struct Transform {
    pub translation: Vec2i,
    pub rotation: Rotation,
}

impl Transform {
    pub const IDENTITY: Self = Self {
        translation: Vec2i::ZERO,
        rotation: Rotation::Rot0,
    };
}

impl Default for Transform {
    #[inline]
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Mul for Transform {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self::Output {
        Self {
            translation: self.translation + rhs.translation.rotate(self.rotation),
            rotation: self.rotation * rhs.rotation,
        }
    }
}

impl MulAssign for Transform {
    #[inline]
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl Vec2i {
    #[inline]
    pub fn transform(self, transform: Transform) -> Self {
        self.rotate(transform.rotation) + transform.translation
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct GlobalTransform(Transform);

#[derive(Default, Bundle)]
pub struct TransformBundle {
    pub transform: Transform,
    pub global_transform: GlobalTransform,
}

/// The bounding box of the entity relative to its center
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Component, Reflect)]
#[repr(C)]
pub struct BoundingBox {
    min: Vec2i,
    max: Vec2i,
}

impl BoundingBox {
    #[inline]
    pub const fn from_points(a: Vec2i, b: Vec2i) -> Self {
        Self {
            min: a.min(b),
            max: a.max(b),
        }
    }

    pub const fn from_center_half_size(center: Vec2i, half_width: u32, half_height: u32) -> Self {
        Self {
            min: Vec2i {
                x: center.x - (half_width as i32),
                y: center.y - (half_height as i32),
            },
            max: Vec2i {
                x: center.x + (half_width as i32),
                y: center.y + (half_height as i32),
            },
        }
    }

    pub const fn from_half_size(half_width: u32, half_height: u32) -> Self {
        Self {
            min: Vec2i {
                x: -(half_width as i32),
                y: -(half_height as i32),
            },
            max: Vec2i {
                x: half_width as i32,
                y: half_height as i32,
            },
        }
    }

    #[inline]
    pub const fn min(self) -> Vec2i {
        self.min
    }

    #[inline]
    pub const fn max(self) -> Vec2i {
        self.max
    }

    #[inline]
    pub const fn width(self) -> u32 {
        self.max.x.abs_diff(self.min.x)
    }

    #[inline]
    pub const fn height(self) -> u32 {
        self.max.y.abs_diff(self.min.y)
    }

    #[inline]
    pub const fn contains(self, point: Vec2i) -> bool {
        (self.min().x <= point.x)
            && (self.max().x >= point.x)
            && (self.min().y <= point.y)
            && (self.max().y >= point.y)
    }

    //#[inline]
    //pub const fn intersects_with(self, min: Vec2i, max: Vec2i) -> bool {
    //    assert!(min.x <= max.x);
    //    assert!(min.y <= max.y);

    //    let intersects_x = ((self.min_x() >= min.x) && (self.max_x() <= max.x))
    //        || ((self.min_x() <= min.x) && (self.max_x() >= min.x))
    //        || ((self.min_x() <= max.x) && (self.max_x() >= max.x));
    //    let intersects_y = ((self.min_y() >= min.y) && (self.max_y() <= max.y))
    //        || ((self.min_y() <= min.y) && (self.max_y() >= min.y))
    //        || ((self.min_y() <= max.y) && (self.max_y() >= max.y));
    //    intersects_x && intersects_y
    //}

    #[inline]
    pub fn translate(mut self, translation: Vec2i) -> Self {
        self.min += translation;
        self.max += translation;
        self
    }

    pub fn rotate(self, rotation: Rotation) -> Self {
        let a = self.min.rotate(rotation);
        let b = self.max.rotate(rotation);
        Self::from_points(a, b)
    }

    pub fn transform(self, transform: Transform) -> Self {
        let a = self.min.transform(transform);
        let b = self.max.transform(transform);
        Self::from_points(a, b)
    }
}

/// The computed absolute bounding box of the entity
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct AbsoluteBoundingBox(BoundingBox);

#[derive(Default, Bundle)]
pub struct BoundingBoxBundle {
    pub bounding_box: BoundingBox,
    pub absolute_bounding_box: AbsoluteBoundingBox,
}

pub(crate) fn update_transforms(
    mut absolutes: Query<(
        &Transform,
        &mut GlobalTransform,
        Option<&BoundingBox>,
        Option<&mut AbsoluteBoundingBox>,
        Option<&Parent>,
    )>,
    ancestors: Query<(Option<&Transform>, Option<&Parent>), Or<(With<Transform>, With<Parent>)>>,
) {
    absolutes
        .par_iter_mut()
        .for_each(|(transform, mut global_transform, bb, abs_bb, parent)| {
            let mut transform = *transform;
            let mut next_parent = parent;
            while let Some(parent) = next_parent {
                let Ok((parent_transform, parent)) = ancestors.get(parent.get()) else {
                    break;
                };

                transform = parent_transform.copied().unwrap_or_default() * transform;
                next_parent = parent;
            }

            if global_transform.0 != transform {
                global_transform.0 = transform;
            }
            if let Some(mut abs_bb) = abs_bb {
                let bb = bb.copied().unwrap_or_default();
                let new_abs_bb = bb.transform(transform);
                if abs_bb.0 != new_abs_bb {
                    abs_bb.0 = new_abs_bb;
                }
            }
        });
}
