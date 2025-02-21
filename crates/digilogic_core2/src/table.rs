//! A slot map that takes pre-hashed random IDs as keys.

use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::num::NonZeroU32;
use std::ops::Index;
use std::str::FromStr;

use nohash_hasher::IntMap;
use serde::{Deserialize, Serialize};

mod childindex;

pub use childindex::*;

/// Error type for the table.
#[derive(thiserror::Error, Debug)]
pub enum Error {
    /// An ID collision occurred.
    #[error("ID collision: {0}")]
    IDCollision(u32),

    /// An invalid digit was encountered while parsing an ID.
    #[error("invalid digit")]
    InvalidDigit,

    /// An invalid ID was parsed
    #[error("invalid ID")]
    InvalidId,
}

/// A unique identifier for a value in a table. This is a 32 bit
/// integer that is randomly generated so that the table index can
/// be a no-hash hash map. In other words, this ID is pre-hashed.
/// The ID is guaranteed to be non-zero, and so Option<Id> is the
/// same size as Id. It has a base32 string representation.
#[derive(Eq, Serialize, Deserialize, PartialOrd, Ord)]
pub struct Id<T> {
    id: NonZeroU32,
    _marker: PhantomData<T>,
}

impl<T> Clone for Id<T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Copy for Id<T> {}

impl<T> PartialEq for Id<T> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> std::hash::Hash for Id<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}

impl<T> std::fmt::Debug for Id<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Id(")?;
        std::fmt::Display::fmt(self, f)?;
        write!(f, ")")
    }
}

/// String representation of IDs will be base32 encoded.
impl<T> std::fmt::Display for Id<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        // base32 encode the ID
        let mut id = self.id.get();
        let mut buf = [0u8; 7];
        let mut i = 0;
        while id > 0 {
            buf[i] = match id % 32 {
                0..=9 => b'0' + (id % 32) as u8,
                10..=31 => b'a' + (id % 32 - 10) as u8,
                _ => unreachable!(),
            };
            id /= 32;
            i += 1;
        }
        for c in buf[..i].iter().rev() {
            write!(f, "{}", *c as char)?;
        }
        Ok(())
    }
}

/// Parse a base32 encoded string into an ID.
impl FromStr for Id<u32> {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut id = 0;
        for c in s.chars() {
            id *= 32;
            id += match c {
                '0'..='9' => c as u32 - '0' as u32,
                'a'..='v' => c as u32 - 'a' as u32 + 10,
                _ => return Err(Error::InvalidDigit),
            };
        }
        if id == 0 || id == u32::MAX {
            return Err(Error::InvalidId);
        }
        Ok(Id::new(NonZeroU32::new(id).ok_or(Error::InvalidId)?))
    }
}

impl<T> Id<T> {
    /// Create a new ID from a non-zero 32 bit integer.
    /// This is private on purpose, so that the only way to create
    /// an ID is through the table.
    fn new(id: NonZeroU32) -> Self {
        Self {
            id,
            _marker: PhantomData,
        }
    }

    /// Get the raw ID value.
    /// Private on purpose.
    fn id(&self) -> u32 {
        self.id.get()
    }
}

/// An IdGenerator generates a series of unique IDs that are random enough to
/// be used in a hash map without a being hashed first. It's important that
/// whatever random source is used, that it generates unique IDs that are not
/// repeated.
pub trait IdGenerator<'de, T>: Default + Serialize + Deserialize<'de> {
    fn next_id(&mut self) -> Id<T>;
}

/// A simple linear congruential generator (LCG) that generates random IDs.
/// This is the default ID generator for the table. It generates IDs that are
/// "random enough", but not perfectly random, and not cryptographically secure.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LCG {
    state: u32,
}

impl LCG {
    pub fn new(seed: u32) -> Self {
        Self { state: seed }
    }

    pub fn rand_u32(&mut self) -> NonZeroU32 {
        loop {
            self.state = self
                .state
                .wrapping_mul(1_664_525)
                .wrapping_add(1_013_904_223);
            // LCG's lower bits are not super random, so we xor with the upper bits
            // in order to compensate for that and get a better distribution.
            let value = self.state ^ (self.state >> 16);

            // reserve 0 and u32::MAX as invalid values
            if value != 0 && value != u32::MAX {
                // SAFETY: We are guaranteed that the value is not zero due to the check above.
                return unsafe { NonZeroU32::new_unchecked(value) };
            }
        }
    }
}

impl Default for LCG {
    fn default() -> Self {
        // TODO: maybe use a random seed?
        Self::new(0xcafef00d)
    }
}

impl<T> IdGenerator<'_, T> for LCG {
    fn next_id(&mut self) -> Id<T> {
        Id::new(self.rand_u32())
    }
}

type DefaultIdGenerator = LCG;

/// Table is a no-hash hash map that works in a similar way to a slot map, but with
/// random IDs that are "pre-hashed" to avoid hash collisions.
///
/// Values are stored densely in a Vec to ensure good cache locality and fast
/// iteration. A no-hash hash map is used only for random access from ID to value.
///
/// Iterators return (ID, &value) pairs, except the values() and keys() iterators.
pub type Table<T> = SecondaryTable<T, T, DefaultIdGenerator>;

/// Table and SecondaryTable are no-hash hash maps that work in a similar way to
/// a slot map, but with random IDs that are "pre-hashed" to avoid hash collisions.
/// A Table is a map from ID to value, and a SecondaryTable is meant to be used to
/// map the IDs from one Table to a different value.
///
/// Both store the values densely in a Vec to ensure good cache locality and fast
/// iteration. A no-hash hash map is used only for random access from ID to value.
///
/// Iterators return (ID, &value) pairs, except the values() and keys() iterators.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SecondaryTable<K, V, IdGen = ()> {
    rows: Vec<V>,
    ids: Vec<Id<K>>,
    index: IntMap<u32, u32>,
    idgen: IdGen,
}

impl<K, V, IdGen: Default> Default for SecondaryTable<K, V, IdGen> {
    fn default() -> Self {
        Self::with_capacity(0)
    }
}

impl<K, V, IdGen: Default> SecondaryTable<K, V, IdGen> {
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a new table with the given capacity.
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            rows: Vec::with_capacity(capacity),
            ids: Vec::with_capacity(capacity),
            index: IntMap::default(),
            idgen: IdGen::default(),
        }
    }
}

impl<'de, T, IdGen: IdGenerator<'de, T>> SecondaryTable<T, T, IdGen> {
    /// Reserve a valid Id without inserting a value.
    pub fn reserve_id(&mut self) -> Id<T> {
        self.idgen.next_id()
    }

    /// Insert a value and return the Id of the inserted value.
    pub fn insert(&mut self, value: T) -> Id<T> {
        match self.try_insert(value) {
            Ok(id) => id,
            Err(e) => panic!("{}", e),
        }
    }

    /// Try inserting a value, returning an error if the IdGenerator
    /// produces an invalid ID (likely due to roll over).
    pub fn try_insert(&mut self, value: T) -> Result<Id<T>, Error> {
        let id = self.idgen.next_id();
        self.insert_with_id(id, value).map(|_| id)
    }
}

impl<K, V, IdGen> SecondaryTable<K, V, IdGen> {
    /// Insert a value with a specific ID. Note: ID absolutely must be generated
    /// by the table's ID generator, as it must be random due to using a no-hash
    /// hash map.
    pub fn insert_with_id(&mut self, id: Id<K>, value: V) -> Result<(), Error> {
        debug_assert_ne!(id.id(), u32::MAX, "invalid ID");
        if self.index.contains_key(&id.id()) {
            return Err(Error::IDCollision(id.id()));
        }
        self.rows.push(value);
        self.ids.push(id);
        self.index.insert(id.id(), self.rows.len() as u32 - 1);
        Ok(())
    }

    /// Get a mutable reference to a value by ID.
    pub fn get_mut(&mut self, id: &Id<K>) -> Option<&mut V> {
        debug_assert_eq!(self.index.len(), self.rows.len());
        let value = self.index.get(&id.id());
        if let Some(&index) = value {
            debug_assert!(index < self.rows.len() as u32);
            // SAFETY: The index is always kept in sync with the rows.
            return Some(unsafe { self.rows.get_unchecked_mut(index as usize) });
        }
        None
    }

    /// Remove a value by ID.
    pub fn remove(&mut self, id: &Id<K>) -> Option<V> {
        if let Some(index) = self.index.remove(&id.id()) {
            let old_id = self.ids.last().copied();
            let curr_id = self.ids.swap_remove(index as usize);
            debug_assert_eq!(curr_id.id, id.id);

            let value = self.rows.swap_remove(index as usize);
            if let Some(old_id) = old_id {
                if index != self.rows.len() as u32 {
                    debug_assert!(self.ids[index as usize] == old_id);
                    debug_assert_ne!(old_id.id, curr_id.id);
                    let index_val = self.index.get_mut(&old_id.id()).expect("old id not found");
                    *index_val = index;
                }
            }
            Some(value)
        } else {
            None
        }
    }
}

impl<K, V, IdGen> SecondaryTable<K, V, IdGen> {
    /// Returns the number of elements in the table.
    pub fn len(&self) -> usize {
        self.rows.len()
    }

    /// Returns true if the table is empty.
    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    /// Returns the number of elements the table can hold without reallocating.
    pub fn capacity(&self) -> usize {
        self.rows.capacity()
    }

    /// Ensures there is capacity for at least `additional` more elements to be inserted.
    /// Note: Not additive with the current capacity.
    pub fn reserve(&mut self, additional: usize) {
        self.rows.reserve(additional);
        self.ids.reserve(additional);
    }

    /// Returns true if the table contains the given ID.
    pub fn contains_key(&self, id: &Id<K>) -> bool {
        self.index.contains_key(&id.id())
    }

    /// Get a reference to a value by ID.
    pub fn get(&self, id: &Id<K>) -> Option<&V> {
        debug_assert_eq!(self.index.len(), self.rows.len());
        // SAFETY: The index is always kept in sync with the rows.
        self.index.get(&id.id()).map(|&index| {
            debug_assert!(index < self.rows.len() as u32);
            unsafe { self.rows.get_unchecked(index as usize) }
        })
    }

    /// Retain only the elements specified by the predicate.
    pub fn retain<F>(&mut self, mut f: F)
    where
        F: FnMut(Id<K>, &mut V) -> bool,
    {
        let mut i = 0;
        while i < self.rows.len() {
            let id = self.ids[i];
            let value = &mut self.rows[i];
            if !f(id, value) {
                self.remove(&id);
            } else {
                i += 1;
            }
        }
    }

    /// Clears the table, removing all values.
    pub fn clear(&mut self) {
        self.drain();
    }

    /// Get a mutable reference to multiple values by ID. If any of the keys are
    /// not found, returns None. If there are any duplicate keys, returns None.
    pub fn get_disjoint_mut<const N: usize>(&mut self, keys: [Id<K>; N]) -> Option<[&mut V; N]> {
        // SAFETY: It's safe because the type of keys are maybe uninit, and we are initializing them
        // with the values from the table before returning any of them.
        let mut ptrs: [MaybeUninit<*mut V>; N] = unsafe { MaybeUninit::uninit().assume_init() };
        let mut indices: [u32; N] = [0; N];

        let mut i = 0;
        while i < N {
            let id = keys[i];

            // temporarily remove indices so that if the set is not disjoint, the following
            // if will fail and we will return None after fixing the index back up.
            // This gives us O(N) complexity.
            let index = if let Some(index) = self.index.remove(&id.id()) {
                indices[i] = index;
                index
            } else {
                break;
            };

            // SAFETY: The index is always kept in sync with the rows.
            let value = unsafe { self.rows.get_unchecked_mut(index as usize) };
            ptrs[i] = MaybeUninit::new(value);

            i += 1;
        }

        // reinsert removed indices
        for k in 0..i {
            self.index.insert(keys[k].id(), indices[k]);
        }

        if i == N {
            // SAFETY: All were valid and disjoint.
            Some(unsafe { core::mem::transmute_copy::<_, [&mut V; N]>(&ptrs) })
        } else {
            None
        }
    }

    /// Get a mutable reference to multiple values by ID. Does not check if
    /// the keys are disjoint. Returns None if any key doesn't exist.
    /// # Safety
    /// The caller must ensure that no two keys are equal, otherwise it's
    /// unsafe (two mutable references to the same value).
    pub unsafe fn get_disjoint_unchecked_mut<const N: usize>(
        &mut self,
        keys: [Id<K>; N],
    ) -> Option<[&mut V; N]> {
        unsafe {
            // Safe, see get_disjoint_mut.
            let mut ptrs: [MaybeUninit<*mut V>; N] = MaybeUninit::uninit().assume_init();
            for i in 0..N {
                let &index = self.index.get(&keys[i].id())?;
                ptrs[i] = MaybeUninit::new(self.rows.get_unchecked_mut(index as usize));
            }
            Some(core::mem::transmute_copy::<_, [&mut V; N]>(&ptrs))
        }
    }

    /// Get an iterator over (ID, &value) pairs.
    pub fn iter(&self) -> Iter<K, V> {
        let rows = self.rows.iter();
        let ids = self.ids.iter();
        let num_left = rows.len();
        Iter {
            num_left,
            rows,
            ids,
            _k: PhantomData,
        }
    }

    /// Get an iterator over (ID, &mut value) pairs.
    pub fn iter_mut(&mut self) -> IterMut<K, V> {
        let rows = self.rows.iter_mut();
        let ids = self.ids.iter();
        let num_left = rows.len();
        IterMut {
            num_left,
            rows,
            ids,
            _k: PhantomData,
        }
    }

    /// Get an iterator over references to values.
    pub fn values(&self) -> impl Iterator<Item = &V> {
        self.rows.iter()
    }

    /// Get an iterator over mutable references to values.
    pub fn values_mut(&mut self) -> impl Iterator<Item = &mut V> {
        self.rows.iter_mut()
    }

    /// Get an iterator over keys.
    pub fn keys(&self) -> impl Iterator<Item = Id<K>> + use<'_, K, V, IdGen> {
        self.ids.iter().copied()
    }

    /// Removes all values from the table and returns an iterator over the removed values.
    /// Iterates from the end of the table to the start, as that is the most efficient.
    pub fn drain(&mut self) -> Drain<K, V, IdGen> {
        let cur = self.rows.len();
        Drain { table: self, cur }
    }
}

impl<K, V, IdGen> Index<Id<K>> for SecondaryTable<K, V, IdGen> {
    type Output = V;

    fn index(&self, index: Id<K>) -> &Self::Output {
        self.get(&index).expect("no entry found for key")
    }
}

impl<'a, K, V, IdGen> IntoIterator for &'a SecondaryTable<K, V, IdGen> {
    type Item = (Id<K>, &'a V);
    type IntoIter = Iter<'a, K, V>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a, K, V, IdGen> IntoIterator for &'a mut SecondaryTable<K, V, IdGen> {
    type Item = (Id<K>, &'a mut V);
    type IntoIter = IterMut<'a, K, V>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter_mut()
    }
}

impl<K, V, IdGen> IntoIterator for SecondaryTable<K, V, IdGen> {
    type Item = (Id<K>, V);
    type IntoIter = IntoIter<K, V>;

    fn into_iter(self) -> IntoIter<K, V> {
        let num_left = self.rows.len();
        let rows = self.rows.into_iter();
        let ids = self.ids.into_iter();
        let index = self.index.into_iter();
        IntoIter {
            num_left,
            rows,
            ids,
            index,
            _k: PhantomData,
        }
    }
}

#[derive(Debug)]
pub struct Drain<'a, K, V, IdGen> {
    table: &'a mut SecondaryTable<K, V, IdGen>,
    cur: usize,
}

impl<K, V, IdGen> Iterator for Drain<'_, K, V, IdGen> {
    type Item = (Id<K>, V);

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur == 0 {
            return None;
        }
        self.cur -= 1;
        let id = self.table.ids[self.cur];
        self.table.remove(&id).map(|value| (id, value))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.cur, Some(self.cur))
    }
}

impl<K, V, IdGen> Drop for Drain<'_, K, V, IdGen> {
    fn drop(&mut self) {
        self.for_each(|_drop| {});
    }
}

#[derive(Debug)]
pub struct IntoIter<K, V> {
    num_left: usize,
    rows: std::vec::IntoIter<V>,
    ids: std::vec::IntoIter<Id<K>>,
    index: std::collections::hash_map::IntoIter<u32, u32>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<K, V> Iterator for IntoIter<K, V> {
    type Item = (Id<K>, V);

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(id) = self.ids.next() {
            let value = self.rows.next().unwrap();
            self.index.next().unwrap();
            self.num_left -= 1;

            Some((id, value))
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.num_left, Some(self.num_left))
    }
}

#[derive(Debug)]
pub struct Iter<'a, K, V> {
    num_left: usize,
    rows: core::slice::Iter<'a, V>,
    ids: core::slice::Iter<'a, Id<K>>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<'a, K, V: 'a> Iterator for Iter<'a, K, V> {
    type Item = (Id<K>, &'a V);

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(id) = self.ids.next() {
            // TODO: optimize this
            let value = self.rows.next().unwrap();
            self.num_left -= 1;

            Some((*id, value))
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.num_left, Some(self.num_left))
    }
}

impl<K, V> Clone for Iter<'_, K, V> {
    fn clone(&self) -> Self {
        Self {
            num_left: self.num_left,
            rows: self.rows.clone(),
            ids: self.ids.clone(),
            _k: self._k,
        }
    }
}

#[derive(Debug)]
pub struct IterMut<'a, K, V> {
    num_left: usize,
    rows: core::slice::IterMut<'a, V>,
    ids: core::slice::Iter<'a, Id<K>>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<'a, K, V: 'a> Iterator for IterMut<'a, K, V> {
    type Item = (Id<K>, &'a mut V);

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(id) = self.ids.next() {
            // TODO: optimize this
            let value = self.rows.next().unwrap();
            self.num_left -= 1;

            Some((*id, value))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::collections::HashSet;

    #[test]
    fn test_id_size() {
        assert_eq!(size_of::<Id<u32>>(), 4);
        // make sure the size of an Option<Id> is the same as the size of an Id
        // this ensures that Id doesn't need a null/invalid value, Option can be
        // used instead.
        assert_eq!(size_of::<Option<Id<u32>>>(), 4);
    }

    #[test]
    fn test_id_string_and_parse() {
        let id: Id<u32> = Id::new(NonZeroU32::new(0x12345678).unwrap());
        assert_eq!(id.to_string(), "938ljo");
        assert_eq!(Id::from_str("938ljo").unwrap(), id);
    }

    #[test]
    fn test_table() {
        let mut table: Table<i32> = Table::default();
        let id1 = table.insert(1);
        let id2 = table.insert(2);
        let id3 = table.insert(3);
        assert_eq!(table.get(&id1), Some(&1));
        assert_eq!(table.get(&id2), Some(&2));
        assert_eq!(table.get(&id3), Some(&3));
        assert_eq!(table.remove(&id2), Some(2));
        assert_eq!(table.get(&id2), None);
        assert_eq!(table.get(&id3), Some(&3));
        let id4 = table.insert(4);
        assert_eq!(table.get(&id4), Some(&4));
        assert_eq!(table.get(&id3), Some(&3));
        assert_eq!(table.remove(&id3), Some(3));
        assert_eq!(table.get(&id3), None);
        assert_eq!(table.get(&id4), Some(&4));
        assert_eq!(table.remove(&id1), Some(1));
        assert_eq!(table.remove(&id4), Some(4));
        assert_eq!(table.get(&id1), None);
        assert_eq!(table.get(&id4), None);
    }

    #[test]
    fn test_basic_operations() {
        let mut table: Table<String> = Table::new();

        // Test insert
        let id1 = table.insert("value1".to_string());
        let id2 = table.insert("value2".to_string());

        assert_eq!(table.len(), 2);
        assert_eq!(table[id1], "value1");
        assert_eq!(table[id2], "value2");

        // Test remove
        let removed = table.remove(&id1);
        assert_eq!(removed, Some("value1".to_string()));
        assert_eq!(table.len(), 1);
        assert!(!table.contains_key(&id1));
    }

    #[test]
    fn test_get_disjoint_mut() {
        let mut table: Table<String> = Table::new();

        let id1 = table.insert("value1".to_string());
        let id2 = table.insert("value2".to_string());
        let _id3 = table.insert("value3".to_string());

        // Test valid disjoint access
        if let Some([v1, v2]) = table.get_disjoint_mut([id1, id2]) {
            *v1 = "new1".to_string();
            *v2 = "new2".to_string();
        }

        assert_eq!(table[id1], "new1");
        assert_eq!(table[id2], "new2");

        // Test with duplicate IDs (should return None)
        assert!(table.get_disjoint_mut([id1, id1]).is_none());
    }

    #[test]
    fn test_unsafe_get_disjoint_unchecked_mut() {
        let mut table: Table<String> = Table::new();

        let id1 = table.insert("value1".to_string());
        let id2 = table.insert("value2".to_string());

        // Safe usage
        unsafe {
            if let Some([v1, v2]) = table.get_disjoint_unchecked_mut([id1, id2]) {
                *v1 = "new1".to_string();
                *v2 = "new2".to_string();
            }
        }

        assert_eq!(table[id1], "new1");
        assert_eq!(table[id2], "new2");
    }

    #[test]
    fn test_iterator() {
        let mut table: Table<i32> = Table::new();

        let _id1 = table.insert(1);
        let _id2 = table.insert(2);
        let _id3 = table.insert(3);

        let mut values = HashSet::new();
        for (_id, value) in table.iter() {
            values.insert(*value);
        }

        assert_eq!(values.len(), 3);
        assert!(values.contains(&1));
        assert!(values.contains(&2));
        assert!(values.contains(&3));
    }

    #[test]
    fn test_drain() {
        let mut table: Table<i32> = Table::new();

        let _id1 = table.insert(1);
        let _id2 = table.insert(2);

        let drained: Vec<_> = table.drain().collect();
        assert_eq!(drained.len(), 2);
        assert!(table.is_empty());
    }

    #[test]
    fn test_retain() {
        let mut table: Table<i32> = Table::new();

        let id1 = table.insert(1);
        let id2 = table.insert(2);
        let id3 = table.insert(3);

        table.retain(|_, value| *value % 2 == 0);

        assert_eq!(table.len(), 1);
        assert!(table.contains_key(&id2));
        assert!(!table.contains_key(&id1));
        assert!(!table.contains_key(&id3));
    }

    #[test]
    #[should_panic]
    fn test_id_collision() {
        let mut table: Table<i32> = Table::new();
        let id = Id::new(NonZeroU32::new(1).unwrap());

        table.insert_with_id(id, 1).unwrap();
        // This should panic
        table.insert_with_id(id, 2).unwrap();
    }

    // Test for memory safety
    #[test]
    fn test_drop_behavior() {
        use std::cell::RefCell;
        use std::rc::Rc;

        #[derive(Default)]
        struct DropCounter(Rc<RefCell<i32>>);

        impl Drop for DropCounter {
            fn drop(&mut self) {
                *self.0.borrow_mut() += 1;
            }
        }

        let counter = Rc::new(RefCell::new(0));
        {
            let mut table: Table<DropCounter> = Table::new();
            table.insert(DropCounter(counter.clone()));
            table.insert(DropCounter(counter.clone()));
        }

        assert_eq!(*counter.borrow(), 2);
    }

    #[test]
    fn test_capacity_and_reserve() {
        let mut table: Table<i32> = Table::with_capacity(10);
        assert!(table.capacity() >= 10);

        table.reserve(20);
        eprintln!("capacity: {}", table.capacity());
        assert!(table.capacity() >= 20);
    }

    #[test]
    fn test_clear() {
        let mut table: Table<i32> = Table::new();
        let id1 = table.insert(1);
        let id2 = table.insert(2);

        assert_eq!(table.len(), 2);
        table.clear();
        assert_eq!(table.len(), 0);
        assert!(!table.contains_key(&id1));
        assert!(!table.contains_key(&id2));
    }

    #[test]
    fn test_iterator_mut() {
        let mut table: Table<i32> = Table::new();
        let _id1 = table.insert(1);
        let _id2 = table.insert(2);
        let _id3 = table.insert(3);

        // Test mutable iteration
        for (_id, value) in table.iter_mut() {
            *value *= 2;
        }

        let values: Vec<i32> = table.values().copied().collect();
        assert!(values.contains(&2));
        assert!(values.contains(&4));
        assert!(values.contains(&6));
    }

    #[test]
    fn test_values_and_keys() {
        let mut table: Table<String> = Table::new();
        let id1 = table.insert("one".to_string());
        let id2 = table.insert("two".to_string());

        let values: Vec<String> = table.values().cloned().collect();
        assert_eq!(values.len(), 2);
        assert!(values.contains(&"one".to_string()));
        assert!(values.contains(&"two".to_string()));

        let keys: Vec<Id<String>> = table.keys().collect();
        assert_eq!(keys.len(), 2);
        assert!(keys.contains(&id1));
        assert!(keys.contains(&id2));
    }

    #[test]
    fn test_get_disjoint_mut_edge_cases() {
        let mut table: Table<i32> = Table::new();
        let id1 = table.insert(1);
        let id2 = table.insert(2);
        let id3 = table.insert(3);

        // Test with non-existent ID
        let non_existent_id = Id::new(NonZeroU32::new(99999).unwrap());
        assert!(table.get_disjoint_mut([id1, non_existent_id]).is_none());

        // Test with three values
        if let Some([v1, v2, v3]) = table.get_disjoint_mut([id1, id2, id3]) {
            *v1 *= 10;
            *v2 *= 10;
            *v3 *= 10;
        }

        assert_eq!(table[id1], 10);
        assert_eq!(table[id2], 20);
        assert_eq!(table[id3], 30);
    }

    #[test]
    fn test_reserve_id() {
        let mut table: Table<i32> = Table::new();
        let reserved_id = table.reserve_id();

        // The table should be empty even after reserving an ID
        assert_eq!(table.len(), 0);
        assert!(!table.contains_key(&reserved_id));

        // We should be able to use the reserved ID
        table.insert_with_id(reserved_id, 42).unwrap();
        assert_eq!(table[reserved_id], 42);
    }

    #[test]
    fn test_try_insert() {
        let mut table: Table<i32> = Table::new();

        // Normal insertion should work
        let result = table.try_insert(42);
        assert!(result.is_ok());

        // Test that the value was actually inserted
        let id = result.unwrap();
        assert_eq!(table[id], 42);
    }

    #[test]
    fn test_complex_remove_scenario() {
        let mut table: Table<String> = Table::new();

        // Insert several items
        let ids: Vec<_> = (0..5)
            .map(|i| table.insert(format!("value{}", i)))
            .collect();

        // Remove items in different order
        assert_eq!(table.remove(&ids[2]), Some("value2".to_string()));
        assert_eq!(table.remove(&ids[0]), Some("value0".to_string()));
        assert_eq!(table.remove(&ids[4]), Some("value4".to_string()));

        // Check remaining items
        assert_eq!(table.len(), 2);
        assert_eq!(table[ids[1]], "value1");
        assert_eq!(table[ids[3]], "value3");

        // Try to remove already removed item
        assert_eq!(table.remove(&ids[2]), None);
    }

    #[test]
    fn test_adding_a_million_items() {
        let mut table: Table<i32> = Table::new();

        let mut ids = Vec::new();

        for i in 0..1_000_000 {
            ids.push(table.insert(i));
        }

        assert_eq!(table.len(), 1_000_000);

        for i in 0..1_000_000 {
            assert_eq!(table[ids[i]], i as i32);
        }

        for (i, id) in ids.iter().enumerate() {
            assert_eq!(table.remove(id), Some(i as i32));
        }

        assert_eq!(table.len(), 0);
    }
}
