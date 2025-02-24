use serde::{Deserialize, Serialize};
use smallvec::SmallVec;

#[derive(
    Default, Debug, Clone, Copy, Deserialize, Serialize, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
pub enum Bit {
    L,
    H,
    #[default]
    X,
    Z,
}

impl std::str::FromStr for Bit {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "0" | "l" | "L" => Ok(Self::L),
            "1" | "h" | "H" => Ok(Self::H),
            "x" | "X" => Ok(Self::X),
            "z" | "Z" => Ok(Self::Z),
            _ => Err(anyhow::anyhow!("invalid wire state: {}", s)),
        }
    }
}

impl From<char> for Bit {
    fn from(c: char) -> Self {
        match c {
            '0' | 'l' | 'L' => Self::L,
            '1' | 'h' | 'H' => Self::H,
            'x' | 'X' => Self::X,
            'z' | 'Z' => Self::Z,
            _ => panic!("invalid wire state: {}", c),
        }
    }
}

impl From<Bit> for char {
    fn from(bit: Bit) -> char {
        match bit {
            Bit::L => '0',
            Bit::H => '1',
            Bit::X => 'X',
            Bit::Z => 'Z',
        }
    }
}

impl std::fmt::Display for Bit {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", char::from(*self))
    }
}

impl Bit {
    pub fn lh_plane(&self) -> usize {
        match self {
            Bit::L => 0,
            Bit::H => 1,
            Bit::X => 0,
            Bit::Z => 1,
        }
    }

    pub fn xz_plane(&self) -> usize {
        match self {
            Bit::L => 0,
            Bit::H => 0,
            Bit::X => 1,
            Bit::Z => 1,
        }
    }

    // Get the bit value from the given planes. It's expected that only the first
    // bit is set in each plane, otherwise it will panic.
    pub fn from_planes(lh_plane: usize, xz_plane: usize) -> Self {
        match (lh_plane, xz_plane) {
            (0, 0) => Self::L,
            (1, 0) => Self::H,
            (0, 1) => Self::X,
            (1, 1) => Self::Z,
            _ => panic!("invalid bit planes: {} {}", lh_plane, xz_plane),
        }
    }
}

pub union Value {
    inline: (usize, usize),
    heap: (usize, *mut usize),
}

impl Value {
    const BITS: usize = usize::BITS as usize;
    const INLINE_WIDTH: usize = ((usize::BITS * 2 - 8) / 2) as usize;

    pub fn width(&self) -> usize {
        unsafe { self.inline.0 & 0xFF }
    }

    fn inline(&self) -> (usize, usize) {
        unsafe { self.inline }
    }

    fn is_inline(&self) -> bool {
        self.width() < Self::INLINE_WIDTH
    }

    fn heap(&self) -> (usize, *mut usize) {
        unsafe { self.heap }
    }

    pub fn get(&self, i: usize) -> Bit {
        if !self.is_inline() {
            let (width, ptr) = self.heap();
            let mut next = width / Self::BITS;
            if width % Self::BITS > 0 {
                next += 1;
            }
            let lh_plane = unsafe { (*(ptr.wrapping_add(i / Self::BITS)) >> (i % Self::BITS)) & 1 };
            let xz_plane =
                unsafe { (*(ptr.wrapping_add((i / Self::BITS) + next)) >> (i % Self::BITS)) & 1 };
            Bit::from_planes(lh_plane, xz_plane)
        } else {
            match usize::BITS {
                32 => {
                    let inline = self.inline();
                    let val: u64 = inline.0 as u64 | ((inline.1 as u64) << Self::BITS);
                    let lh_plane = (val >> (i + 8)) & 1;
                    let xz_plane = (val >> (i + Self::INLINE_WIDTH + 8)) & 1;
                    Bit::from_planes(lh_plane as usize, xz_plane as usize)
                }
                64 => {
                    let inline = self.inline();
                    let val: u128 = inline.0 as u128 | ((inline.1 as u128) << Self::BITS);
                    let lh_plane = (val >> (i + 8)) & 1;
                    let xz_plane = (val >> (i + Self::INLINE_WIDTH + 8)) & 1;
                    Bit::from_planes(lh_plane as usize, xz_plane as usize)
                }
                _ => unreachable!(),
            }
        }
    }
}

impl Drop for Value {
    fn drop(&mut self) {
        if !self.is_inline() {
            unsafe {
                drop(Box::from_raw(self.heap.1));
            }
        }
    }
}

impl Clone for Value {
    fn clone(&self) -> Self {
        if self.is_inline() {
            Self {
                inline: self.inline(),
            }
        } else {
            let (width, ptr) = self.heap();
            let mut boxed: SmallVec<[usize; 4]> = SmallVec::with_capacity(width / Self::BITS * 2);
            let mut next = width / Self::BITS;
            if width % Self::BITS > 0 {
                next += 1;
            }
            for i in 0..next {
                boxed.push(unsafe { *ptr.add(i) });
            }
            for i in 0..next {
                boxed.push(unsafe { *ptr.add(i + next) });
            }
            let ptr = boxed.as_mut_ptr();
            std::mem::forget(boxed);
            Self { heap: (width, ptr) }
        }
    }
}

impl Default for Value {
    fn default() -> Self {
        Self { inline: (0, 0) }
    }
}

impl std::str::FromStr for Value {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut lh_plane: SmallVec<[usize; 2]> = SmallVec::new();
        let mut xz_plane: SmallVec<[usize; 2]> = SmallVec::new();
        let mut width = 0;

        for c in s.chars() {
            let bit: Bit = c.into();
            let lh = bit.lh_plane();
            let xz = bit.xz_plane();
            if width % Self::BITS == 0 {
                lh_plane.push(0);
                xz_plane.push(0);
            }
            let lh_mut: &mut usize = lh_plane.last_mut().unwrap();
            *lh_mut |= lh << (width % Self::BITS);
            let xz_mut: &mut usize = xz_plane.last_mut().unwrap();
            *xz_mut |= xz << (width % Self::BITS);
            width += 1;
        }

        if width < Self::INLINE_WIDTH {
            if Self::BITS == 32 {
                let val = ((width as u64) | (lh_plane[0] << 8) as u64)
                    | ((xz_plane[0] as u64) << (Self::INLINE_WIDTH + 8));
                Ok(Self {
                    inline: ((val >> 32) as usize, val as usize),
                })
            } else {
                let val = ((width as u128) | (lh_plane[0] << 8) as u128)
                    | ((xz_plane[0] as u128) << (Self::INLINE_WIDTH + 8));
                Ok(Self {
                    inline: ((val >> 64) as usize, val as usize),
                })
            }
        } else {
            let mut boxed = lh_plane
                .iter()
                .chain(xz_plane.iter())
                .copied()
                .collect::<Box<[_]>>();
            let ptr = boxed.as_mut_ptr();
            std::mem::forget(boxed);

            Ok(Self { heap: (width, ptr) })
        }
    }
}

impl std::fmt::Display for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        for i in 0..self.width() {
            let c: char = self.get(i).into();
            write!(f, "{}", c)?;
        }
        Ok(())
    }
}

impl PartialEq for Value {
    fn eq(&self, other: &Self) -> bool {
        if self.width() != other.width() {
            return false;
        }
        for i in 0..self.width() {
            if self.get(i) != other.get(i) {
                return false;
            }
        }
        true
    }
}

impl Eq for Value {}

impl std::fmt::Debug for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "v\"{}\"", self)
    }
}

impl Serialize for Value {
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(&format!("{}", self))
    }
}

impl<'de> Deserialize<'de> for Value {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let s = String::deserialize(deserializer)?;
        s.parse().map_err(serde::de::Error::custom)
    }
}
