use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ops::{Index, IndexMut};

use nohash_hasher::IntMap;

use super::Error;
use super::Id;
use super::IdGenerator;

#[derive(Debug)]
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

    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            rows: Vec::with_capacity(capacity),
            ids: Vec::with_capacity(capacity),
            index: IntMap::default(),
            idgen: IdGen::default(),
        }
    }
}

impl<T, IdGen: IdGenerator<T>> SecondaryTable<T, T, IdGen> {
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

    pub fn try_insert(&mut self, value: T) -> Result<Id<T>, Error> {
        let id = self.idgen.next_id();
        self.insert_with_id(id, value).map(|_| id)
    }
}

impl<K, V, IdGen> SecondaryTable<K, V, IdGen> {
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

    pub fn contains_key(&self, id: &Id<K>) -> bool {
        self.index.contains_key(&id.id)
    }

    pub fn insert_with_id(&mut self, id: Id<K>, value: V) -> Result<(), Error> {
        if self.index.contains_key(&id.id) {
            return Err(Error::IDCollision(id.id));
        }
        self.rows.push(value);
        self.ids.push(id);
        self.index.insert(id.id, self.rows.len() as u32 - 1);
        Ok(())
    }

    pub fn get(&self, id: &Id<K>) -> Option<&V> {
        // SAFETY: The index is always kept in sync with the rows.
        self.index
            .get(&id.id)
            .map(|&index| unsafe { self.rows.get_unchecked(index as usize) })
    }

    pub fn get_mut(&mut self, id: &Id<K>) -> Option<&mut V> {
        let value = self.index.get(&id.id);
        if let Some(&index) = value {
            // SAFETY: The index is always kept in sync with the rows.
            return Some(unsafe { self.rows.get_unchecked_mut(index as usize) });
        }
        None
    }

    pub fn remove(&mut self, id: &Id<K>) -> Option<V> {
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
        self.rows.clear();
        self.ids.clear();
        self.index.clear();
    }

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
            Some(unsafe { core::mem::transmute_copy::<_, [&mut V; N]>(&ptrs) })
        } else {
            None
        }
    }

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
                let &index = self.index.get(&keys[i].id)?;
                ptrs[i] = MaybeUninit::new(self.rows.get_unchecked_mut(index as usize));
            }
            Some(core::mem::transmute_copy::<_, [&mut V; N]>(&ptrs))
        }
    }

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

    pub fn values(&self) -> Values<K, V> {
        Values { inner: self.iter() }
    }

    pub fn values_mut(&mut self) -> ValuesMut<K, V> {
        ValuesMut {
            inner: self.iter_mut(),
        }
    }

    pub fn keys(&self) -> Keys<K, V> {
        Keys { inner: self.iter() }
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

impl<K, V, IdGen> IndexMut<Id<K>> for SecondaryTable<K, V, IdGen> {
    fn index_mut(&mut self, index: Id<K>) -> &mut Self::Output {
        self.get_mut(&index).expect("no entry found for key")
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

#[derive(Debug)]
pub struct Values<'a, K, V> {
    inner: Iter<'a, K, V>,
}

impl<'a, K, V: 'a> Iterator for Values<'a, K, V> {
    type Item = &'a V;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

impl<K, V> Clone for Values<'_, K, V> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}

#[derive(Debug)]
pub struct ValuesMut<'a, K, V> {
    inner: IterMut<'a, K, V>,
}

impl<'a, K, V: 'a> Iterator for ValuesMut<'a, K, V> {
    type Item = &'a mut V;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

#[derive(Debug)]
pub struct Keys<'a, K, V> {
    inner: Iter<'a, K, V>,
}

impl<'a, K, V: 'a> Iterator for Keys<'a, K, V> {
    type Item = Id<K>;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(id, _)| id)
    }
}

impl<K, V> Clone for Keys<'_, K, V> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}
