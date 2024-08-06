use bevy_reflect::Reflect;
use std::fmt;
use std::ops::*;

pub const FRACT_BITS: usize = 8;
const FRACT_MASK: i32 = (1 << FRACT_BITS) - 1;

#[derive(Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Reflect)]
#[repr(transparent)]
#[reflect_value]
pub struct Fixed(i32);

impl Fixed {
    pub const MIN: Self = Self(i32::MIN);
    pub const MAX: Self = Self(i32::MAX);
    pub const MIN_INT: Self = Self(i32::MIN & !FRACT_MASK);
    pub const MAX_INT: Self = Self(i32::MAX & !FRACT_MASK);
    pub const EPSILON: Self = Self(1);

    #[inline]
    pub const fn from_bits(bits: i32) -> Self {
        Self(bits)
    }

    #[inline]
    pub const fn to_bits(self) -> i32 {
        self.0
    }
}

macro_rules! from_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub const fn $name(value: $t) -> Self {
                Self((value as i32) << FRACT_BITS)
            }
        }

        impl From<$t> for Fixed {
            #[inline]
            fn from(value: $t) -> Self {
                Self::$name(value)
            }
        }
    };
}

macro_rules! try_from_u_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub const fn $name(value: $t) -> Option<Self> {
                if value > ((Self::MAX.0 >> FRACT_BITS) as $t) {
                    None
                } else {
                    Some(Self((value << FRACT_BITS) as i32))
                }
            }
        }

        impl TryFrom<$t> for Fixed {
            type Error = ToFixedError;

            #[inline]
            fn try_from(value: $t) -> Result<Self, Self::Error> {
                Self::$name(value).ok_or(ToFixedError)
            }
        }
    };
}

macro_rules! try_from_s_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub const fn $name(value: $t) -> Option<Self> {
                if value > ((Self::MAX.0 >> FRACT_BITS) as $t) {
                    None
                } else if value < ((Self::MIN.0 >> FRACT_BITS) as $t) {
                    None
                } else {
                    Some(Self((value << FRACT_BITS) as i32))
                }
            }
        }

        impl TryFrom<$t> for Fixed {
            type Error = ToFixedError;

            #[inline]
            fn try_from(value: $t) -> Result<Self, Self::Error> {
                Self::$name(value).ok_or(ToFixedError)
            }
        }
    };
}

macro_rules! try_from_float_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(value: $t) -> Option<Self> {
                let scaled = value * ((1 << FRACT_BITS) as $t);
                if (scaled < (i32::MIN as $t)) || (scaled > (i32::MAX as $t)) {
                    None
                } else {
                    Some(Self(scaled as i32))
                }
            }
        }

        impl TryFrom<$t> for Fixed {
            type Error = ToFixedError;

            #[inline]
            fn try_from(value: $t) -> Result<Self, Self::Error> {
                Self::$name(value).ok_or(ToFixedError)
            }
        }
    };
}

macro_rules! try_to_u_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(self) -> Option<$t> {
                if self.0 < 0 {
                    None
                } else {
                    let v = self.0 >> FRACT_BITS;
                    if v > (<$t>::MAX as i32) {
                        None
                    } else {
                        Some(v as $t)
                    }
                }
            }
        }

        impl TryFrom<Fixed> for $t {
            type Error = FromFixedError;

            #[inline]
            fn try_from(value: Fixed) -> Result<Self, Self::Error> {
                value.$name().ok_or(FromFixedError)
            }
        }
    };
}

macro_rules! try_to_s_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(self) -> Option<$t> {
                let v = self.0 >> FRACT_BITS;
                if v > (<$t>::MAX as i32) {
                    None
                } else {
                    Some(v as $t)
                }
            }
        }

        impl TryFrom<Fixed> for $t {
            type Error = FromFixedError;

            #[inline]
            fn try_from(value: Fixed) -> Result<Self, Self::Error> {
                value.$name().ok_or(FromFixedError)
            }
        }
    };
}

macro_rules! to_u_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(self) -> Option<$t> {
                if self.0 < 0 {
                    None
                } else {
                    Some((self.0 >> FRACT_BITS) as $t)
                }
            }
        }

        impl TryFrom<Fixed> for $t {
            type Error = FromFixedError;

            #[inline]
            fn try_from(value: Fixed) -> Result<Self, Self::Error> {
                value.$name().ok_or(FromFixedError)
            }
        }
    };
}

macro_rules! to_s_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(self) -> $t {
                (self.0 >> FRACT_BITS) as $t
            }
        }

        impl From<Fixed> for $t {
            #[inline]
            fn from(value: Fixed) -> Self {
                value.$name()
            }
        }
    };
}

macro_rules! to_float_fn {
    ($name:ident : $t:ty) => {
        impl Fixed {
            #[inline]
            pub fn $name(self) -> $t {
                (self.0 as $t) / ((1 << FRACT_BITS) as $t)
            }
        }

        impl From<Fixed> for $t {
            #[inline]
            fn from(value: Fixed) -> Self {
                value.$name()
            }
        }
    };
}

#[derive(Debug, Clone)]
pub struct ToFixedError;

from_fn!(from_u8 : u8);
from_fn!(from_i8 : i8);
from_fn!(from_u16 : u16);
from_fn!(from_i16 : i16);
try_from_u_fn!(try_from_u32 : u32);
try_from_s_fn!(try_from_i32 : i32);
try_from_u_fn!(try_from_u64 : u64);
try_from_s_fn!(try_from_i64 : i64);
try_from_u_fn!(try_from_u128 : u128);
try_from_s_fn!(try_from_i128 : i128);
try_from_u_fn!(try_from_usize : usize);
try_from_s_fn!(try_from_isize : isize);
try_from_float_fn!(try_from_f32 : f32);
try_from_float_fn!(try_from_f64 : f64);

#[derive(Debug, Clone)]
pub struct FromFixedError;

try_to_u_fn!(try_to_u8 : u8);
try_to_s_fn!(try_to_i8 : i8);
try_to_u_fn!(try_to_u16 : u16);
try_to_s_fn!(try_to_i16 : i16);
to_u_fn!(try_to_u32 : u32);
to_s_fn!(to_i32 : i32);
to_u_fn!(try_to_u64 : u64);
to_s_fn!(to_i64 : i64);
to_u_fn!(try_to_u128 : u128);
to_s_fn!(to_i128 : i128);
to_u_fn!(try_to_usize : usize);
to_s_fn!(to_isize : isize);
to_float_fn!(to_f32 : f32);
to_float_fn!(to_f64 : f64);

#[macro_export]
macro_rules! fixed {
    ($v:literal) => {{
        const SCALED: f64 = ($v as f64) * ((1 << $crate::FIXED_FRACT_BITS) as f64);
        const LOWER_BOUND: bool = SCALED >= (i32::MIN as f64);
        const UPPER_BOUNT: bool = SCALED <= (i32::MAX as f64);

        if LOWER_BOUND && UPPER_BOUNT {
            $crate::Fixed::from_bits(SCALED as i32)
        } else {
            panic!("Value out of range");
        }
    }};
}

impl fmt::Debug for Fixed {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.to_f64(), f)
    }
}

impl fmt::Display for Fixed {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&self.to_f64(), f)
    }
}

impl Fixed {
    pub const fn strict_add(self, rhs: Self) -> Self {
        match self.0.checked_add(rhs.0) {
            Some(v) => Self(v),
            None => panic!("attempt to add with overflow"),
        }
    }

    pub const fn strict_sub(self, rhs: Self) -> Self {
        match self.0.checked_sub(rhs.0) {
            Some(v) => Self(v),
            None => panic!("attempt to subtract with overflow"),
        }
    }

    pub const fn strict_neg(self) -> Self {
        match self.0.checked_neg() {
            Some(v) => Self(v),
            None => panic!("attempt to negate with overflow"),
        }
    }

    pub const fn strict_mul(self, rhs: Self) -> Self {
        let lhs = self.0 as i64;
        let rhs = rhs.0 as i64;
        let result64 = lhs.wrapping_mul(rhs) >> FRACT_BITS;
        let result32 = result64 as i32;
        if (result32 as i64) != result64 {
            panic!("attempt to multiply with overflow");
        }
        Self(result32)
    }

    pub const fn strict_div(self, rhs: Self) -> Self {
        let lhs = self.0 as i64;
        let rhs = rhs.0 as i64;
        let result64 = (lhs << FRACT_BITS).wrapping_div(rhs);
        let result32 = result64 as i32;
        if (result32 as i64) != result64 {
            panic!("attempt to divide with overflow");
        }
        Self(result32)
    }

    pub const fn strict_rem(self, rhs: Self) -> Self {
        match self.0.checked_rem(rhs.0) {
            Some(v) => Self(v),
            None => panic!("attempt to calculate the remainder with overflow"),
        }
    }

    #[inline]
    pub const fn strict_abs(self) -> Self {
        if self.0 < 0 {
            self.strict_neg()
        } else {
            self
        }
    }

    #[inline]
    pub const fn wrapping_add(self, rhs: Self) -> Self {
        Self(self.0.wrapping_add(rhs.0))
    }

    #[inline]
    pub const fn wrapping_sub(self, rhs: Self) -> Self {
        Self(self.0.wrapping_sub(rhs.0))
    }

    #[inline]
    pub const fn wrapping_neg(self) -> Self {
        Self(self.0.wrapping_neg())
    }

    #[inline]
    pub const fn wrapping_mul(self, rhs: Self) -> Self {
        let lhs = self.0 as i64;
        let rhs = rhs.0 as i64;
        let result = lhs.wrapping_mul(rhs) >> FRACT_BITS;
        Self(result as i32)
    }

    #[inline]
    pub const fn wrapping_div(self, rhs: Self) -> Self {
        let lhs = self.0 as i64;
        let rhs = rhs.0 as i64;
        let result = (lhs << FRACT_BITS).wrapping_div(rhs);
        Self(result as i32)
    }

    #[inline]
    pub const fn wrapping_rem(self, rhs: Self) -> Self {
        Self(self.0.wrapping_rem(rhs.0))
    }

    #[inline]
    pub const fn wrapping_abs(self) -> Self {
        if self.0 < 0 {
            self.wrapping_neg()
        } else {
            self
        }
    }

    #[inline]
    pub const fn min(self, rhs: Self) -> Self {
        if self.0 <= rhs.0 {
            Self(self.0)
        } else {
            Self(rhs.0)
        }
    }

    #[inline]
    pub const fn max(self, rhs: Self) -> Self {
        if self.0 >= rhs.0 {
            Self(self.0)
        } else {
            Self(rhs.0)
        }
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn abs(self) -> Self {
        self.strict_abs()
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn abs(self) -> Self {
        self.wrapping_abs()
    }
}

// FIXME: move all of this into the corresponding traits once const trait impls become stable
impl Fixed {
    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_add(self, rhs: Self) -> Self {
        self.strict_add(rhs)
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_add(self, rhs: Self) -> Self {
        self.wrapping_add(rhs)
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_sub(self, rhs: Self) -> Self {
        self.strict_sub(rhs)
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_sub(self, rhs: Self) -> Self {
        self.wrapping_sub(rhs)
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_neg(self) -> Self {
        self.strict_neg()
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_neg(self) -> Self {
        self.wrapping_neg()
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_mul(self, rhs: Self) -> Self {
        self.strict_mul(rhs)
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_mul(self, rhs: Self) -> Self {
        self.wrapping_mul(rhs)
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_div(self, rhs: Self) -> Self {
        self.strict_div(rhs)
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_div(self, rhs: Self) -> Self {
        self.wrapping_div(rhs)
    }

    #[cfg(debug_assertions)]
    #[inline]
    pub const fn const_rem(self, rhs: Self) -> Self {
        self.strict_rem(rhs)
    }

    #[cfg(not(debug_assertions))]
    #[inline]
    pub const fn const_rem(self, rhs: Self) -> Self {
        self.wrapping_rem(rhs)
    }
}

impl Add for Fixed {
    type Output = Self;

    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        self.const_add(rhs)
    }
}

impl AddAssign for Fixed {
    #[inline]
    fn add_assign(&mut self, rhs: Self) {
        *self = self.wrapping_add(rhs);
    }
}

impl Sub for Fixed {
    type Output = Self;

    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        self.const_sub(rhs)
    }
}

impl SubAssign for Fixed {
    #[inline]
    fn sub_assign(&mut self, rhs: Self) {
        *self = self.wrapping_sub(rhs);
    }
}

impl Neg for Fixed {
    type Output = Self;

    #[inline]
    fn neg(self) -> Self::Output {
        self.const_neg()
    }
}

impl Mul for Fixed {
    type Output = Self;

    #[inline]
    fn mul(self, rhs: Self) -> Self::Output {
        self.const_mul(rhs)
    }
}

impl MulAssign for Fixed {
    #[inline]
    fn mul_assign(&mut self, rhs: Self) {
        *self = self.wrapping_mul(rhs);
    }
}

impl Div for Fixed {
    type Output = Self;

    #[inline]
    fn div(self, rhs: Self) -> Self::Output {
        self.const_div(rhs)
    }
}

impl DivAssign for Fixed {
    #[inline]
    fn div_assign(&mut self, rhs: Self) {
        *self = self.wrapping_div(rhs);
    }
}

impl Rem for Fixed {
    type Output = Self;

    #[inline]
    fn rem(self, rhs: Self) -> Self::Output {
        self.const_rem(rhs)
    }
}

impl RemAssign for Fixed {
    #[inline]
    fn rem_assign(&mut self, rhs: Self) {
        *self = self.wrapping_rem(rhs);
    }
}

impl serde::Serialize for Fixed {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_f64(self.to_f64())
    }
}

struct FixedVisitor;

macro_rules! visit_fn {
    ($visit:ident : $t:ty => $parse:ident) => {
        fn $visit<E>(self, v: $t) -> Result<Self::Value, E>
        where
            E: serde::de::Error,
        {
            Ok(Fixed::$parse(v))
        }
    };
}

macro_rules! try_visit_fn {
    ($visit:ident : $t:ty => $parse:ident) => {
        fn $visit<E>(self, v: $t) -> Result<Self::Value, E>
        where
            E: serde::de::Error,
        {
            Fixed::$parse(v).ok_or_else(|| E::custom("value out of range"))
        }
    };
}

impl<'de> serde::de::Visitor<'de> for FixedVisitor {
    type Value = Fixed;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a number")
    }

    visit_fn!(visit_u8 : u8 => from_u8);
    visit_fn!(visit_i8 : i8 => from_i8);
    visit_fn!(visit_u16 : u16 => from_u16);
    visit_fn!(visit_i16 : i16 => from_i16);
    try_visit_fn!(visit_u32 : u32 => try_from_u32);
    try_visit_fn!(visit_i32 : i32 => try_from_i32);
    try_visit_fn!(visit_u64 : u64 => try_from_u64);
    try_visit_fn!(visit_i64 : i64 => try_from_i64);
    try_visit_fn!(visit_u128 : u128 => try_from_u128);
    try_visit_fn!(visit_i128 : i128 => try_from_i128);
    try_visit_fn!(visit_f32 : f32 => try_from_f32);
    try_visit_fn!(visit_f64 : f64 => try_from_f64);
}

impl<'de> serde::Deserialize<'de> for Fixed {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_f64(FixedVisitor)
    }
}

#[cfg(feature = "inspector")]
mod inspector {
    use bevy_inspector_egui::inspector_egui_impls::InspectorPrimitive;
    use bevy_inspector_egui::reflect_inspector::InspectorUi;

    impl InspectorPrimitive for super::Fixed {
        fn ui(
            &mut self,
            ui: &mut egui::Ui,
            options: &dyn std::any::Any,
            id: egui::Id,
            env: InspectorUi<'_, '_>,
        ) -> bool {
            let mut value = self.to_f64();
            let result = InspectorPrimitive::ui(&mut value, ui, options, id, env);
            if let Some(value) = Self::try_from_f64(value) {
                *self = value;
                result
            } else {
                false
            }
        }

        fn ui_readonly(
            &self,
            ui: &mut egui::Ui,
            options: &dyn std::any::Any,
            id: egui::Id,
            env: InspectorUi<'_, '_>,
        ) {
            InspectorPrimitive::ui_readonly(&self.to_f64(), ui, options, id, env);
        }
    }
}

// TODO: tests
