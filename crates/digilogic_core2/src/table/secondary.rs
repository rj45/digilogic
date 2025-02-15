use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ops::{Index, IndexMut};

use nohash_hasher::IntMap;

use super::Error;
use super::Id;

#[derive(Debug)]
pub struct SecondaryTable<K, V> {
    rows: Vec<V>,
    ids: Vec<Id<K>>,
    index: IntMap<u32, u32>,
}

impl<K, V> Default for SecondaryTable<K, V> {
    fn default() -> Self {
        Self::with_capacity(0)
    }
}

impl<K, V> SecondaryTable<K, V> {
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            rows: Vec::with_capacity(capacity),
            ids: Vec::with_capacity(capacity),
            index: IntMap::default(),
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
    /// NOTE: This does not clear the Ids, they are still marked as used.
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

    pub fn iter(&self) -> SecondaryIter<K, V> {
        let rows = self.rows.iter();
        let ids = self.ids.iter();
        let num_left = rows.len();
        SecondaryIter {
            num_left,
            rows,
            ids,
            _k: PhantomData,
        }
    }

    pub fn iter_mut(&mut self) -> SecondaryIterMut<K, V> {
        let rows = self.rows.iter_mut();
        let ids = self.ids.iter();
        let num_left = rows.len();
        SecondaryIterMut {
            num_left,
            rows,
            ids,
            _k: PhantomData,
        }
    }

    pub fn values(&self) -> SecondaryValues<K, V> {
        SecondaryValues { inner: self.iter() }
    }

    pub fn values_mut(&mut self) -> SecondaryValuesMut<K, V> {
        SecondaryValuesMut {
            inner: self.iter_mut(),
        }
    }

    pub fn keys(&self) -> SecondaryKeys<K, V> {
        SecondaryKeys { inner: self.iter() }
    }

    /// Removes all values from the table and returns an iterator over the removed values.
    /// Iterates from the end of the table to the start, as that is the most efficient.
    pub fn drain(&mut self) -> SecondaryDrain<K, V> {
        let cur = self.rows.len();
        SecondaryDrain { table: self, cur }
    }
}

impl<K, V> Index<Id<K>> for SecondaryTable<K, V> {
    type Output = V;

    fn index(&self, index: Id<K>) -> &Self::Output {
        self.get(&index).expect("no entry found for key")
    }
}

impl<K, V> IndexMut<Id<K>> for SecondaryTable<K, V> {
    fn index_mut(&mut self, index: Id<K>) -> &mut Self::Output {
        self.get_mut(&index).expect("no entry found for key")
    }
}

impl<'a, K, V> IntoIterator for &'a SecondaryTable<K, V> {
    type Item = (Id<K>, &'a V);
    type IntoIter = SecondaryIter<'a, K, V>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a, K, V> IntoIterator for &'a mut SecondaryTable<K, V> {
    type Item = (Id<K>, &'a mut V);
    type IntoIter = SecondaryIterMut<'a, K, V>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter_mut()
    }
}

impl<K, V> IntoIterator for SecondaryTable<K, V> {
    type Item = (Id<K>, V);
    type IntoIter = SecondaryIntoIter<K, V>;

    fn into_iter(self) -> SecondaryIntoIter<K, V> {
        let num_left = self.rows.len();
        let rows = self.rows.into_iter();
        let ids = self.ids.into_iter();
        let index = self.index.into_iter();
        SecondaryIntoIter {
            num_left,
            rows,
            ids,
            index,
            _k: PhantomData,
        }
    }
}

#[derive(Debug)]
pub struct SecondaryDrain<'a, K, V> {
    table: &'a mut SecondaryTable<K, V>,
    cur: usize,
}

impl<K, V> Iterator for SecondaryDrain<'_, K, V> {
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

impl<K, V> Drop for SecondaryDrain<'_, K, V> {
    fn drop(&mut self) {
        self.for_each(|_drop| {});
    }
}

#[derive(Debug)]
pub struct SecondaryIntoIter<K, V> {
    num_left: usize,
    rows: std::vec::IntoIter<V>,
    ids: std::vec::IntoIter<Id<K>>,
    index: std::collections::hash_map::IntoIter<u32, u32>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<K, V> Iterator for SecondaryIntoIter<K, V> {
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
pub struct SecondaryIter<'a, K, V> {
    num_left: usize,
    rows: core::slice::Iter<'a, V>,
    ids: core::slice::Iter<'a, Id<K>>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<'a, K, V> Iterator for SecondaryIter<'a, K, V> {
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

#[derive(Debug)]
pub struct SecondaryIterMut<'a, K, V> {
    num_left: usize,
    rows: core::slice::IterMut<'a, V>,
    ids: core::slice::Iter<'a, Id<K>>,
    _k: PhantomData<fn(Id<K>) -> Id<K>>,
}

impl<'a, K, V> Iterator for SecondaryIterMut<'a, K, V> {
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
pub struct SecondaryValues<'a, K, V> {
    inner: SecondaryIter<'a, K, V>,
}

impl<'a, K, V> Iterator for SecondaryValues<'a, K, V> {
    type Item = &'a V;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

#[derive(Debug)]
pub struct SecondaryValuesMut<'a, K, V> {
    inner: SecondaryIterMut<'a, K, V>,
}

impl<'a, K, V> Iterator for SecondaryValuesMut<'a, K, V> {
    type Item = &'a mut V;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(_, value)| value)
    }
}

#[derive(Debug)]
pub struct SecondaryKeys<'a, K, V> {
    inner: SecondaryIter<'a, K, V>,
}

impl<K, V> Iterator for SecondaryKeys<'_, K, V> {
    type Item = Id<K>;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|(id, _)| id)
    }
}
