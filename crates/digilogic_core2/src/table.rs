//! A slot map that takes pre-hashed random IDs as keys.

use core::panic;
use std::marker::PhantomData;

use rand_pcg::rand_core::RngCore;
use rand_pcg::Pcg32;

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

pub trait IdGenerator<T> {
    fn new_gen() -> Self;
    fn next_id(&mut self) -> Id<T>;
}

impl<T> IdGenerator<T> for Pcg32 {
    fn new_gen() -> Self {
        Pcg32::new(0xcafef00dd15ea5e5, 0xa02bdbf7bb3c0a7)
    }
    fn next_id(&mut self) -> Id<T> {
        Id::new(self.next_u32())
    }
}

impl<T> IdGenerator<T> for () {
    fn new_gen() -> Self {}
    fn next_id(&mut self) -> Id<T> {
        panic!("no ID generator provided");
    }
}

type Table<T> = SecondaryTable<T, T, Pcg32>;

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
