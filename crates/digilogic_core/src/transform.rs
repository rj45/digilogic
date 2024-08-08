use crate::{fixed, Fixed};
use aery::prelude::*;
use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_reflect::Reflect;
use bitflags::bitflags;
use serde::{Deserialize, Serialize};
use std::ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Sub, SubAssign};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Reflect)]
#[repr(C)]
pub struct Vec2 {
    pub x: Fixed,
    pub y: Fixed,
}

impl Vec2 {
    pub const ZERO: Self = Self {
        x: fixed!(0.0),
        y: fixed!(0.0),
    };

    #[inline]
    pub const fn splat(value: Fixed) -> Self {
        Self { x: value, y: value }
    }

    #[inline]
    pub const fn min(self, rhs: Self) -> Self {
        Self {
            x: self.x.min(rhs.x),
            y: self.y.min(rhs.y),
        }
    }

    #[inline]
    pub const fn max(self, rhs: Self) -> Self {
        Self {
            x: self.x.max(rhs.x),
            y: self.y.max(rhs.y),
        }
    }

    #[inline]
    pub const fn manhatten_distance_to(self, other: Self) -> Fixed {
        let diff_x = self.x.const_sub(other.x).abs();
        let diff_y = self.y.const_sub(other.y).abs();
        diff_x.const_add(diff_y)
    }
}

impl Default for Vec2 {
    #[inline]
    fn default() -> Self {
        Self::ZERO
    }
}

impl Add for Vec2 {
    type Output = Self;

    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

impl AddAssign for Vec2 {
    #[inline]
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl Sub for Vec2 {
    type Output = Self;

    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
        }
    }
}

impl SubAssign for Vec2 {
    #[inline]
    fn sub_assign(&mut self, rhs: Self) {
        *self = *self - rhs;
    }
}

impl Mul<Fixed> for Vec2 {
    type Output = Self;

    #[inline]
    fn mul(self, rhs: Fixed) -> Self::Output {
        Self {
            x: self.x * rhs,
            y: self.y * rhs,
        }
    }
}

impl MulAssign<Fixed> for Vec2 {
    #[inline]
    fn mul_assign(&mut self, rhs: Fixed) {
        *self = *self * rhs;
    }
}

impl Div<Fixed> for Vec2 {
    type Output = Self;

    #[inline]
    fn div(self, rhs: Fixed) -> Self::Output {
        Self {
            x: self.x / rhs,
            y: self.y / rhs,
        }
    }
}

impl DivAssign<Fixed> for Vec2 {
    #[inline]
    fn div_assign(&mut self, rhs: Fixed) {
        *self = *self / rhs;
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Reflect)]
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

impl Vec2 {
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

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Component, Reflect)]
pub struct Transform {
    pub translation: Vec2,
    pub rotation: Rotation,
    pub scale: Fixed,
}

impl Transform {
    pub const IDENTITY: Self = Self {
        translation: Vec2::ZERO,
        rotation: Rotation::Rot0,
        scale: fixed!(1),
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
            translation: self.translation + (rhs.translation * self.scale).rotate(self.rotation),
            rotation: self.rotation * rhs.rotation,
            scale: self.scale * rhs.scale,
        }
    }
}

impl MulAssign for Transform {
    #[inline]
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl Vec2 {
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
#[derive(
    Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Component, Reflect,
)]
#[repr(C)]
pub struct BoundingBox {
    pub min: Vec2,
    pub max: Vec2,
}

impl BoundingBox {
    #[inline]
    pub const fn from_points(a: Vec2, b: Vec2) -> Self {
        Self {
            min: a.min(b),
            max: a.max(b),
        }
    }

    pub const fn from_center_half_size(
        center: Vec2,
        half_width: Fixed,
        half_height: Fixed,
    ) -> Self {
        assert!(half_width.to_bits() >= 0);
        assert!(half_height.to_bits() >= 0);

        Self {
            min: Vec2 {
                x: center.x.const_sub(half_width),
                y: center.y.const_sub(half_height),
            },
            max: Vec2 {
                x: center.x.const_sub(half_width),
                y: center.y.const_sub(half_height),
            },
        }
    }

    pub const fn from_half_size(half_width: Fixed, half_height: Fixed) -> Self {
        assert!(half_width.to_bits() >= 0);
        assert!(half_height.to_bits() >= 0);

        Self {
            min: Vec2 {
                x: half_width.const_neg(),
                y: half_height.const_neg(),
            },
            max: Vec2 {
                x: half_width,
                y: half_height,
            },
        }
    }

    pub const fn from_top_left_size(top_left: Vec2, width: Fixed, height: Fixed) -> Self {
        assert!(width.to_bits() >= 0);
        assert!(height.to_bits() >= 0);

        Self {
            min: top_left,
            max: Vec2 {
                x: top_left.x.const_add(width),
                y: top_left.y.const_add(height),
            },
        }
    }

    #[inline]
    pub const fn min(self) -> Vec2 {
        self.min
    }

    #[inline]
    pub const fn max(self) -> Vec2 {
        self.max
    }

    #[inline]
    pub const fn width(self) -> Fixed {
        self.max.x.const_sub(self.min.x).abs()
    }

    #[inline]
    pub const fn height(self) -> Fixed {
        self.max.y.const_sub(self.min.y).abs()
    }

    #[inline]
    pub const fn center(self) -> Vec2 {
        Vec2 {
            x: self.min.x.const_add(self.max.x).const_div(fixed!(2)),
            y: self.min.y.const_add(self.max.y).const_div(fixed!(2)),
        }
    }

    #[inline]
    pub const fn corners(self) -> [Vec2; 4] {
        [
            self.min,
            Vec2 {
                x: self.min.x,
                y: self.max.y,
            },
            self.max,
            Vec2 {
                x: self.max.x,
                y: self.min.y,
            },
        ]
    }

    #[inline]
    pub fn extrude(self, offset: Vec2) -> Self {
        Self::from_points(self.min - offset, self.max + offset)
    }

    #[inline]
    pub fn contains(self, point: Vec2) -> bool {
        (self.min().x <= point.x)
            && (self.max().x >= point.x)
            && (self.min().y <= point.y)
            && (self.max().y >= point.y)
    }

    #[inline]
    pub fn intersects(self, other: Self) -> bool {
        // TODO: this would be a lot more efficient if we used center + half size representation
        let a = self.center();
        let b = other.center();

        // this way of detecting intersections minimizes the number of branches
        ((a.x - b.x).abs() * fixed!(2) < (self.width() + other.width()))
            && ((a.y - b.y).abs() * fixed!(2) < (self.height() + other.height()))
    }

    #[inline]
    pub fn translate(mut self, translation: Vec2) -> Self {
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

impl bvh_arena::BoundingVolume for BoundingBox {
    fn merge(self, other: Self) -> Self {
        let min = self.min.min(other.min);
        let max = self.max.max(other.max);
        Self::from_points(min, max)
    }

    #[inline]
    fn area(&self) -> f32 {
        self.width().to_f32() * self.height().to_f32()
    }

    #[inline]
    fn overlaps(&self, other: &Self) -> bool {
        self.intersects(*other)
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

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Component, Reflect)]
#[repr(u8)]
pub enum Direction {
    PosX = 0,
    NegY = 1,
    NegX = 2,
    PosY = 3,
}

impl Direction {
    pub const ALL: [Self; 4] = [Self::PosX, Self::NegY, Self::NegX, Self::PosY];

    /// Gets the opposite direction of this one.
    #[inline]
    pub const fn opposite(self) -> Self {
        match self {
            Self::PosX => Self::NegX,
            Self::NegX => Self::PosX,
            Self::PosY => Self::NegY,
            Self::NegY => Self::PosY,
        }
    }

    #[inline]
    pub fn rotate(self, rotation: Rotation) -> Self {
        match ((self as u8) + (rotation as u8)) % 4 {
            0 => Self::PosX,
            1 => Self::NegY,
            2 => Self::NegX,
            3 => Self::PosY,
            _ => unreachable!(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct AbsoluteDirection(Direction);

impl Default for AbsoluteDirection {
    #[inline]
    fn default() -> Self {
        Self(Direction::PosX)
    }
}

#[derive(Bundle)]
pub struct DirectionBundle {
    pub direction: Direction,
    pub absolute_direction: AbsoluteDirection,
}

bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Component, Reflect)]
    #[repr(transparent)]
    #[reflect_value]
    pub struct Directions: u8 {
        const POS_X = 0x1;
        const NEG_Y = 0x2;
        const NEG_X = 0x4;
        const POS_Y = 0x8;

        const X = Self::POS_X.bits() | Self::NEG_X.bits();
        const Y = Self::POS_Y.bits() | Self::NEG_Y.bits();

        const NONE = 0;
        const ALL = Self::X.bits() | Self::Y.bits();
    }
}

impl From<Direction> for Directions {
    #[inline]
    fn from(value: Direction) -> Self {
        Self::from_bits(1 << (value as u8)).expect("invalid direction")
    }
}

impl Default for Directions {
    #[inline]
    fn default() -> Self {
        Self::ALL
    }
}

impl Directions {
    #[inline]
    pub fn rotate(self, rotation: Rotation) -> Self {
        let shifted = self.bits() << (rotation as u8);
        let rotated = (shifted & 0xF) | (shifted >> 4);
        Self::from_bits(rotated).expect("invalid rotation")
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct AbsoluteDirections(Directions);

#[derive(Default, Bundle)]
pub struct DirectionsBundle {
    pub directions: Directions,
    pub absolute_directions: AbsoluteDirections,
}

#[derive(Relation)]
pub struct InheritTransform;

fn update_root_transform(
    mut roots: Query<(&Transform, &mut GlobalTransform), Root<InheritTransform>>,
) {
    for (&transform, mut global_transform) in roots.iter_mut() {
        if global_transform.0 != transform {
            global_transform.0 = transform;
        }
    }
}

fn update_transform(
    mut tree: Query<(
        (&Transform, &mut GlobalTransform),
        Relations<InheritTransform>,
    )>,
    roots: Query<Entity, Root<InheritTransform>>,
) {
    tree.traverse_mut::<InheritTransform>(roots.iter())
        .track_self()
        .for_each(
            |(_, parent_global_transform), _, (child_transform, child_global_transform), _| {
                let new_transform = parent_global_transform.0 * **child_transform;
                if child_global_transform.0 != new_transform {
                    child_global_transform.0 = new_transform;
                }
            },
        );
}

fn update_bounding_box(
    mut query: Query<
        (&BoundingBox, &mut AbsoluteBoundingBox, &GlobalTransform),
        Changed<GlobalTransform>,
    >,
) {
    for (bb, mut abs_bb, transform) in query.iter_mut() {
        abs_bb.0 = bb.transform(**transform);
    }
}

fn update_direction(
    mut query: Query<
        (&Direction, &mut AbsoluteDirection, &GlobalTransform),
        Changed<GlobalTransform>,
    >,
) {
    for (dir, mut abs_dir, transform) in query.iter_mut() {
        abs_dir.0 = dir.rotate(transform.rotation);
    }
}

fn update_directions(
    mut query: Query<
        (&Directions, &mut AbsoluteDirections, &GlobalTransform),
        Changed<GlobalTransform>,
    >,
) {
    for (dirs, mut abs_dirs, transform) in query.iter_mut() {
        abs_dirs.0 = dirs.rotate(transform.rotation);
    }
}

pub(crate) struct TransformPlugin;

impl bevy_app::Plugin for TransformPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Vec2>()
            .register_type::<Rotation>()
            .register_type::<Transform>()
            .register_type::<BoundingBox>()
            .register_type::<GlobalTransform>()
            .register_type::<AbsoluteBoundingBox>();

        app.register_relation::<InheritTransform>();
        app.add_systems(
            bevy_app::PostUpdate,
            (update_root_transform, update_transform).chain(),
        );
        app.add_systems(
            bevy_app::PostUpdate,
            (update_bounding_box, update_direction, update_directions).after(update_transform),
        );
    }
}
