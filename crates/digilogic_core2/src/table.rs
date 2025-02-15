//! A slot map that takes pre-hashed random IDs as keys.

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

pub trait IdGenerator<T>: Default {
    fn next_id(&mut self) -> Id<T>;
}

#[derive(Debug)]
pub struct DefaultIdGenerator(Pcg32);

impl Default for DefaultIdGenerator {
    fn default() -> Self {
        Self(Pcg32::new(0xcafef00dd15ea5e5, 0xa02bdbf7bb3c0a7))
    }
}

impl<T> IdGenerator<T> for DefaultIdGenerator {
    fn next_id(&mut self) -> Id<T> {
        Id::new(self.0.next_u32())
    }
}

/// A table that uses random IDs as keys.
pub type Table<T> = SecondaryTable<T, T, DefaultIdGenerator>;

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_table() {
        let mut table = Table::default();
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
}
