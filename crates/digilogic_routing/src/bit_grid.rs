use smallvec::SmallVec;
use std::ops::Range;

#[cfg(not(test))]
type Word = usize;

#[cfg(test)]
type Word = u8;

pub type EmptyRanges = SmallVec<[Range<u32>; 8]>;

#[derive(Default, Debug)]
pub struct BitGrid {
    major_size: u32,
    minor_size: u32,
    bits: Vec<Word>,
}

impl BitGrid {
    pub fn reset(&mut self, major_size: u32, minor_size: u32) {
        self.major_size = major_size;
        self.minor_size = minor_size;

        let major_word_count = major_size.div_ceil(Word::BITS) as usize;
        let minor_count = minor_size as usize;

        self.bits.clear();
        self.bits.resize(major_word_count * minor_count, 0);
    }

    #[inline]
    fn major_word_count(&self) -> u32 {
        self.major_size.div_ceil(Word::BITS)
    }

    pub fn fill_rect(
        &mut self,
        major_min_inclusive: u32,
        minor_min_inclusive: u32,
        major_max_exclusive: u32,
        minor_max_exclusive: u32,
    ) {
        assert!(major_min_inclusive < major_max_exclusive);
        assert!(minor_min_inclusive < minor_max_exclusive);
        assert!(major_max_exclusive <= self.major_size);
        assert!(minor_max_exclusive <= self.minor_size);

        let major_min_word_inclusive = major_min_inclusive / Word::BITS;
        let major_max_word_exclusive = major_max_exclusive.div_ceil(Word::BITS);

        let major_min_bit_inclusive = major_min_inclusive % Word::BITS;
        let major_max_bit_exclusive = major_max_exclusive % Word::BITS;
        let start_mask = Word::MAX << major_min_bit_inclusive;
        let end_mask = Word::MAX.wrapping_shr(Word::BITS - major_max_bit_exclusive);

        for major in major_min_word_inclusive..major_max_word_exclusive {
            let mut bits = Word::MAX;
            if major == major_min_word_inclusive {
                bits &= start_mask;
            }
            if major == (major_max_word_exclusive - 1) {
                bits &= end_mask;
            }

            for minor in minor_min_inclusive..minor_max_exclusive {
                let word_index =
                    (minor as usize) * (self.major_word_count() as usize) + (major as usize);
                self.bits[word_index] |= bits;
            }
        }
    }

    pub fn find_empty_ranges(&self, minor: u32) -> EmptyRanges {
        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        enum State {
            Empty,
            Occupied,
        }

        if self.bits.is_empty() {
            return SmallVec::new();
        }

        let minor_offset = (minor as usize) * (self.major_word_count() as usize);
        let mut state = if self.bits[minor_offset].trailing_ones() > 0 {
            State::Occupied
        } else {
            State::Empty
        };

        let mut ranges = SmallVec::new();
        let mut range = 0u32..0u32;
        for major in 0..self.major_word_count() {
            let word_index = minor_offset + (major as usize);
            let mut word = self.bits[word_index];

            let word_bits = if major == (self.major_word_count() - 1) {
                self.major_size - major * Word::BITS
            } else {
                Word::BITS
            };

            let mut bit_offset = 0u32;
            while bit_offset < Word::BITS {
                match state {
                    State::Empty => {
                        let trailing_zeros = word.trailing_zeros();
                        if trailing_zeros >= (word_bits - bit_offset) {
                            break;
                        }

                        word >>= trailing_zeros;
                        bit_offset += trailing_zeros;
                        state = State::Occupied;
                        range.end = major * Word::BITS + bit_offset;
                        ranges.push(range.clone());
                    }
                    State::Occupied => {
                        let trailing_ones = word.trailing_ones();
                        if trailing_ones >= (word_bits - bit_offset) {
                            break;
                        }

                        word >>= trailing_ones;
                        bit_offset += trailing_ones;
                        state = State::Empty;
                        range.start = major * Word::BITS + bit_offset;
                    }
                }
            }
        }

        if state == State::Empty {
            range.end = self.major_size;
            ranges.push(range);
        }

        ranges
    }
}

#[allow(clippy::single_range_in_vec_init)]
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reset() {
        let mut map = BitGrid::default();

        map.reset(8, 0);
        assert_eq!(map.major_size, 8);
        assert_eq!(map.minor_size, 0);
        assert_eq!(map.major_word_count(), 1);

        map.reset(0, 8);
        assert_eq!(map.major_size, 0);
        assert_eq!(map.minor_size, 8);
        assert_eq!(map.major_word_count(), 0);

        map.reset(12, 4);
        assert_eq!(map.major_size, 12);
        assert_eq!(map.minor_size, 4);
        assert_eq!(map.major_word_count(), 2);
    }

    #[test]
    fn fill_within_word() {
        let mut map = BitGrid::default();
        map.reset(24, 1);
        map.fill_rect(10, 0, 14, 1);
        assert_eq!(map.bits, [0b00000000, 0b00111100, 0b00000000]);
    }

    #[test]
    fn fill_across_word() {
        let mut map = BitGrid::default();
        map.reset(24, 1);
        map.fill_rect(6, 0, 10, 1);
        assert_eq!(map.bits, [0b11000000, 0b00000011, 0b00000000]);
    }

    #[test]
    fn fill_encompassing_word() {
        let mut map = BitGrid::default();
        map.reset(24, 1);
        map.fill_rect(6, 0, 18, 1);
        assert_eq!(map.bits, [0b11000000, 0b11111111, 0b00000011]);
    }

    #[test]
    fn fill_whole_word() {
        let mut map = BitGrid::default();
        map.reset(24, 1);
        map.fill_rect(8, 0, 16, 1);
        assert_eq!(map.bits, [0b00000000, 0b11111111, 0b00000000]);
    }

    #[test]
    fn fill_multiple() {
        let mut map = BitGrid::default();
        map.reset(24, 1);
        map.fill_rect(6, 0, 10, 1);
        map.fill_rect(11, 0, 13, 1);
        map.fill_rect(14, 0, 18, 1);
        assert_eq!(map.bits, [0b11000000, 0b11011011, 0b00000011]);
    }

    #[test]
    fn find_all_empty() {
        let map = BitGrid {
            major_size: 24,
            minor_size: 1,
            bits: vec![0b00000000, 0b00000000, 0b00000000],
        };

        let ranges = map.find_empty_ranges(0);
        assert_eq!(ranges.as_slice(), [0..24]);
    }

    #[test]
    fn find_all_occupied() {
        let map = BitGrid {
            major_size: 24,
            minor_size: 1,
            bits: vec![0b11111111, 0b11111111, 0b11111111],
        };

        let ranges = map.find_empty_ranges(0);
        assert_eq!(ranges.as_slice(), []);
    }

    #[test]
    fn find_whole_word() {
        let map = BitGrid {
            major_size: 24,
            minor_size: 1,
            bits: vec![0b11111111, 0b00000000, 0b11111111],
        };

        let ranges = map.find_empty_ranges(0);
        assert_eq!(ranges.as_slice(), [8..16]);
    }

    #[test]
    fn find_multiple() {
        let map = BitGrid {
            major_size: 24,
            minor_size: 1,
            bits: vec![0b11000000, 0b11011011, 0b00000011],
        };

        let ranges = map.find_empty_ranges(0);
        assert_eq!(ranges.as_slice(), [0..6, 10..11, 13..14, 18..24]);
    }
}
