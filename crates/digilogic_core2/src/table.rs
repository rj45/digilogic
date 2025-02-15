//! A slot map that takes pre-hashed random IDs as keys.

use core::panic;
use std::hash::BuildHasher;
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ops::{Index, IndexMut};

use bloomfilter::Bloom;
use foldhash::quality::RandomState;
use nohash_hasher::IntMap;

mod revindex;
mod secondary;

pub use revindex::*;
pub use secondary::*;

#[derive(thiserror::Error, Debug)]
pub enum Error {
    IDCollision(u32),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Error::IDCollision(id) => write!(f, "ID collision: {}", id),
        }
    }
}

#[derive(Debug, Eq)]
pub struct Id<T> {
    id: u32,
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

impl<T> Id<T> {
    fn new(id: u32) -> Self {
        Self {
            id,
            _marker: PhantomData,
        }
    }
}

/// UIDGenerator generates unique, pre-hashed random IDs. It uses a bloom filter to check for
/// collisions.
#[derive(Debug)]
struct IdGenerator<T> {
    next_id: u32,
    bloom: Bloom<u32>,
    state: RandomState,
    _marker: PhantomData<T>,
}

impl<T> Default for IdGenerator<T> {
    fn default() -> Self {
        Self {
            next_id: 0,
            bloom: Bloom::new_for_fp_rate(10000000, 0.001).expect("failed to create bloom filter"),
            state: RandomState::default(),
            _marker: PhantomData,
        }
    }
}

impl<T> IdGenerator<T> {
    /// NOTE: must insert the generated ID if it's used, this function doesn't
    /// do that for you.
    fn next_id(&mut self) -> Id<T> {
        loop {
            self.next_id += 1;
            let hashed = self.state.hash_one(self.next_id);
            let hashed = ((hashed >> 32) ^ hashed) as u32;
            if !self.bloom.check(&hashed) {
                return Id::new(hashed);
            }
        }
    }

    fn insert(&mut self, id: Id<T>) {
        self.bloom.set(&id.id);
    }
}

#[derive(Debug)]
pub struct Table<T> {
    rows: Vec<T>,
    ids: Vec<Id<T>>,
    index: IntMap<u32, u32>,
    idgen: IdGenerator<T>,
}

impl<T> Default for Table<T> {
    fn default() -> Self {
        Self::with_capacity(0)
    }
}

impl<T> Table<T> {
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            rows: Vec::with_capacity(capacity),
            ids: Vec::with_capacity(capacity),
            index: IntMap::default(),
            idgen: IdGenerator::default(),
        }
    }

    pub fn len(&self) -> usize {
        self.rows.len()
    }

    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    pub fn capacity(&self) -> usize {
        self.rows.capacity()
    }

    pub fn reserve(&mut self, additional: usize) {
        self.rows.reserve(additional);
        self.ids.reserve(additional);
    }

    /// Reserve a valid Id without inserting a value.
    /// NOTE: The Id is not marked as used until a value is inserted.
    pub fn reserve_id(&mut self) -> Id<T> {
        self.idgen.next_id()
    }

    /// Mark an Id as used without inserting a value. The Id will not
    /// be generated again.
    pub fn use_id(&mut self, id: Id<T>) {
        self.idgen.insert(id);
    }

    pub fn contains_key(&self, id: &Id<T>) -> bool {
        self.index.contains_key(&id.id)
    }

    pub fn insert(&mut self, value: T) -> Id<T> {
        let id = self.idgen.next_id();
        if let Err(e) = self.insert_with_id(id, value) {
            panic!("{}", e);
        }
        id
    }

    pub fn insert_with_id(&mut self, id: Id<T>, value: T) -> Result<(), Error> {
        if self.index.contains_key(&id.id) {
            return Err(Error::IDCollision(id.id));
        }
        self.idgen.insert(id);
        self.rows.push(value);
        self.ids.push(id);
        self.index.insert(id.id, self.rows.len() as u32 - 1);
        Ok(())
    }

    pub fn get(&self, id: &Id<T>) -> Option<&T> {
        // SAFETY: The index is always kept in sync with the rows.
        self.index
            .get(&id.id)
            .map(|&index| unsafe { self.rows.get_unchecked(index as usize) })
    }

    pub fn get_mut(&mut self, id: &Id<T>) -> Option<&mut T> {
        let value = self.index.get(&id.id);
        if let Some(&index) = value {
            // SAFETY: The index is always kept in sync with the rows.
            return Some(unsafe { self.rows.get_unchecked_mut(index as usize) });
        }
        None
    }

    pub fn remove(&mut self, id: &Id<T>) -> Option<T> {
        if let Some(index) = self.index.remove(&id.id) {
            let old_id = self.ids.last().copied();
            let curr_id = self.ids.swap_remove(index as usize);

            assert_eq!(curr_id.id, id.id);
            let value = self.rows.swap_remove(index as usize);
            if let Some(old_id) = old_id {
                if index != self.rows.len() as u32 {
                    assert!(self.ids[index as usize] == old_id);
                    assert_ne!(old_id.id, curr_id.id);
                    let index_val = self.index.get_mut(&old_id.id).expect("old id not found");
                    *index_val = index;
                }
            }
            Some(value)
        } else {
            None
        }
    }

    pub fn retain<F>(&mut self, mut f: F)
    where
        F: FnMut(Id<T>, &mut T) -> bool,
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
    /// NOTE: This does not clear the Ids, they are still marked as used.
    pub fn clear(&mut self) {
        self.rows.clear();
        self.ids.clear();
        self.index.clear();
    }

    /// Clears the table and forgets all Ids.
    /// NOTE: This is not safe to use if the Ids could be in use elsewhere,
    /// such as a undo/redo system or serialized to disk where they could be
    /// reloaded.
    pub fn clear_and_forget(&mut self) {
        self.clear();
        self.idgen = IdGenerator::default();
    }

    pub fn get_disjoint_mut<const N: usize>(&mut self, keys: [Id<T>; N]) -> Option<[&mut T; N]> {
        // SAFETY: It's safe because the type of keys are maybe uninit, and we are initializing them
        // with the values from the table before returning any of them.
        let mut ptrs: [MaybeUninit<*mut T>; N] = unsafe { MaybeUninit::uninit().assume_init() };
        let mut indices: [u32; N] = [0; N];

        let mut i = 0;
        while i < N {
            let id = keys[i];

            // temporarily remove indices so that if the set is not disjoint, the following
            // if will fail and we will return None after fixing the index back up.
            // This gives us O(N) complexity.
            let index = if let Some(index) = self.index.remove(&id.id) {
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
            self.index.insert(keys[k].id, indices[k]);
        }

        if i == N {
            // SAFETY: All were valid and disjoint.
            Some(unsafe { core::mem::transmute_copy::<_, [&mut T; N]>(&ptrs) })
        } else {
            None
        }
    }

    /// # Safety
    /// The caller must ensure that no two keys are equal, otherwise it's
    /// unsafe (two mutable references to the same value).
    pub unsafe fn get_disjoint_unchecked_mut<const N: usize>(
        &mut self,
        keys: [Id<T>; N],
    ) -> Option<[&mut T; N]> {
        unsafe {
            // Safe, see get_disjoint_mut.
            let mut ptrs: [MaybeUninit<*mut T>; N] = MaybeUninit::uninit().assume_init();
            for i in 0..N {
                let &index = self.index.get(&keys[i].id)?;
                ptrs[i] = MaybeUninit::new(self.rows.get_unchecked_mut(index as usize));
            }
            Some(core::mem::transmute_copy::<_, [&mut T; N]>(&ptrs))
        }
    }

    pub fn iter(&self) -> Iter<T> {
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

    pub fn iter_mut(&mut self) -> IterMut<T> {
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

    pub fn values(&self) -> Values<T> {
        Values { inner: self.iter() }
    }

    pub fn values_mut(&mut self) -> ValuesMut<T> {
        ValuesMut {
            inner: self.iter_mut(),
        }
    }

    pub fn keys(&self) -> Keys<T> {
        Keys { inner: self.iter() }
    }

    /// Removes all values from the table and returns an iterator over the removed values.
    /// Iterates from the end of the table to the start, as that is the most efficient.
    pub fn drain(&mut self) -> Drain<T> {
        let cur = self.rows.len();
        Drain { table: self, cur }
    }
}

impl<T> Index<Id<T>> for Table<T> {
    type Output = T;

    fn index(&self, index: Id<T>) -> &Self::Output {
        self.get(&index).expect("no entry found for key")
    }
}

impl<T> IndexMut<Id<T>> for Table<T> {
    fn index_mut(&mut self, index: Id<T>) -> &mut Self::Output {
        self.get_mut(&index).expect("no entry found for key")
    }
}

impl<'a, T> IntoIterator for &'a Table<T> {
    type Item = (Id<T>, &'a T);
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut Table<T> {
    type Item = (Id<T>, &'a mut T);
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter_mut()
    }
}

impl<T> IntoIterator for Table<T> {
    type Item = (Id<T>, T);
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
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
pub struct Drain<'a, T> {
    table: &'a mut Table<T>,
    cur: usize,
}

impl<T> Iterator for Drain<'_, T> {
    type Item = (Id<T>, T);

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

impl<T> Drop for Drain<'_, T> {
    fn drop(&mut self) {
        self.for_each(|_drop| {});
    }
}

#[derive(Debug)]
pub struct IntoIter<T> {
    num_left: usize,
    rows: std::vec::IntoIter<T>,
    ids: std::vec::IntoIter<Id<T>>,
    index: std::collections::hash_map::IntoIter<u32, u32>,
    _k: PhantomData<fn(Id<T>) -> Id<T>>,
}

impl<T> Iterator for IntoIter<T> {
    type Item = (Id<T>, T);

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
pub struct Iter<'a, T> {
    num_left: usize,
    rows: core::slice::Iter<'a, T>,
    ids: core::slice::Iter<'a, Id<T>>,
    _k: PhantomData<fn(Id<T>) -> Id<T>>,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = (Id<T>, &'a T);

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

impl<T> Clone for Iter<'_, T> {
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
pub struct IterMut<'a, T> {
    num_left: usize,
    rows: core::slice::IterMut<'a, T>,
    ids: core::slice::Iter<'a, Id<T>>,
    _k: PhantomData<fn(Id<T>) -> Id<T>>,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = (Id<T>, &'a mut T);

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

#[derive(Debug)]
pub struct Values<'a, T> {
    inner: Iter<'a, T>,
}

impl<'a, T> Iterator for Values<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

impl<T> Clone for Values<'_, T> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}

#[derive(Debug)]
pub struct ValuesMut<'a, T> {
    inner: IterMut<'a, T>,
}

impl<'a, T> Iterator for ValuesMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

#[derive(Debug)]
pub struct Keys<'a, T> {
    inner: Iter<'a, T>,
}

impl<T> Iterator for Keys<'_, T> {
    type Item = Id<T>;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(id, _)| id)
    }
}

impl<T> Clone for Keys<'_, T> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_map() {
        let mut map = Table::default();
        let id1 = map.insert(1);
        let id2 = map.insert(2);
        let id3 = map.insert(3);
        assert_eq!(map.get(&id1), Some(&1));
        assert_eq!(map.get(&id2), Some(&2));
        assert_eq!(map.get(&id3), Some(&3));
        assert_eq!(map.remove(&id2), Some(2));
        assert_eq!(map.get(&id2), None);
        assert_eq!(map.get(&id3), Some(&3));
        let id4 = map.insert(4);
        assert_eq!(map.get(&id4), Some(&4));
        assert_eq!(map.get(&id3), Some(&3));
        assert_eq!(map.remove(&id3), Some(3));
        assert_eq!(map.get(&id3), None);
        assert_eq!(map.get(&id4), Some(&4));
        assert_eq!(map.remove(&id1), Some(1));
        assert_eq!(map.remove(&id4), Some(4));
        assert_eq!(map.get(&id1), None);
        assert_eq!(map.get(&id4), None);
    }
}
