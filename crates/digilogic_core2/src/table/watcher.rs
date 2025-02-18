use std::ops::{Deref, DerefMut};

use super::*;

/// A trait for watching changes to a table. This is a trait to make it optional
/// since it involves a lot of copies and may be expensive.
///
/// The MutGuard type is used to allow monitoring changes to values via DerefMut.
pub trait Watcher<K, V> {
    type MutGuard<'a>
    where
        V: 'a,
        Self: 'a;

    fn insert(&mut self, key: Id<K>, value: &V);
    fn remove(&mut self, key: Id<K>, value: &V);
    fn update_mut<'a>(&'a mut self, key: Id<K>, value: &'a mut V) -> Self::MutGuard<'a>;
}

/// A watcher that does nothing, and simply returns a mutable reference directly.
#[derive(Debug, Default)]
pub struct NoWatcher;

impl<K, V> Watcher<K, V> for NoWatcher {
    type MutGuard<'a>
        = &'a mut V
    where
        V: 'a;

    fn insert(&mut self, _key: Id<K>, _value: &V) {}
    fn remove(&mut self, _key: Id<K>, _value: &V) {}
    fn update_mut<'a>(&mut self, _key: Id<K>, value: &'a mut V) -> Self::MutGuard<'a> {
        value
    }
}

/// An entry in a change log. Each entry is a tuple of (index, key, value) where
/// the index is the number of changes that have occurred so far starting from 0.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Change<K, V> {
    /// An entry was inserted.
    Insert(u32, Id<K>, V),
    /// An entry was updated.
    /// NOTE: This represents a DerefMut operation, it does not check if the value
    /// has actually changed.
    Update(u32, Id<K>, V),

    /// An entry was removed.
    Remove(u32, Id<K>, V),
}

/// A log of changes to a table.
#[derive(Debug, Clone)]
pub struct ChangeLog<K, V> {
    changes: Vec<Change<K, V>>,
    index: u32,
}

impl<K, V> ChangeLog<K, V> {
    pub fn new() -> Self {
        Self::default()
    }

    /// Log a change.
    fn log(&mut self, key: Id<K>, value: V) {
        self.changes.push(Change::Insert(self.index, key, value));
        self.index = self.index.wrapping_add(1);
    }

    /// Get a list of changes. This can be thought of as a list of events that
    /// have occurred to the table. It's best to periodically feed these changes
    /// as a batch into everything that cares about them, and then clear the log.
    pub fn changes(&self) -> &[Change<K, V>] {
        &self.changes
    }

    /// Clear the log, and reuse memory for the next batch of changes.
    /// Does not reset the index counter to zero.
    pub fn clear(&mut self) {
        self.changes.clear();
    }
}

impl<K, V> Default for ChangeLog<K, V> {
    /// Create a new empty change log.
    fn default() -> Self {
        Self {
            changes: Vec::new(),
            index: 0,
        }
    }
}

/// A guard that logs the value of the Table row when it is dropped.
/// NOTE:
///  - The value is not logged until the guard is dropped.
///  - The value is not checked if it's actually changed, it's assumed that
///    the value has changed.
/// - The value is cloned when it is logged.
pub struct RecorderGuard<'a, K, V: Clone> {
    key: Id<K>,
    value: &'a mut V,
    rec: &'a mut Recorder<K, V>,
}

impl<K, V: std::fmt::Debug + Clone> std::fmt::Debug for RecorderGuard<'_, K, V> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RecorderGuard")
            .field("key", &self.key)
            .field("value", &self.value)
            .finish()
    }
}

impl<K, V: Clone> Deref for RecorderGuard<'_, K, V> {
    type Target = V;

    fn deref(&self) -> &Self::Target {
        self.value
    }
}

impl<K, V: Clone> DerefMut for RecorderGuard<'_, K, V> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.value
    }
}

impl<K, V: Clone> Drop for RecorderGuard<'_, K, V> {
    fn drop(&mut self) {
        self.rec.log.log(self.key, self.value.clone());
    }
}

/// A watcher that records changes to a table in a ChangeLog.
/// NOTE:
///  - This watcher clones the values when logging changes (may be expensive).
///  - This watcher does not check if the value has actually changed, just if it
///    was mutably borrowed.
#[derive(Debug, Clone)]
pub struct Recorder<K, V> {
    log: ChangeLog<K, V>,
    _phantom: PhantomData<fn(Id<K>, &V) -> &V>,
}

impl<K, V> Default for Recorder<K, V> {
    fn default() -> Self {
        Self {
            log: ChangeLog::default(),
            _phantom: PhantomData,
        }
    }
}

impl<K, V: Clone> Watcher<K, V> for Recorder<K, V> {
    type MutGuard<'a>
        = RecorderGuard<'a, K, V>
    where
        V: Clone + 'a,
        K: 'a;

    fn insert(&mut self, key: Id<K>, value: &V) {
        self.log.log(key, value.clone());
    }

    fn remove(&mut self, key: Id<K>, value: &V) {
        self.log.log(key, value.clone());
    }

    fn update_mut<'a>(&'a mut self, key: Id<K>, value: &'a mut V) -> Self::MutGuard<'a> {
        RecorderGuard {
            key,
            value,
            rec: self,
        }
    }
}

/// An extension trait for tables that have a Recorder watcher to add methods
/// for accessing the change log.
pub trait Recorded<K, V> {
    /// Get a reference to the change log.
    fn changelog(&self) -> &ChangeLog<K, V>;

    /// Get a mutable reference to the change log.
    fn changelog_mut(&mut self) -> &mut ChangeLog<K, V>;
}

impl<K, V, IdGen> Recorded<K, V> for SecondaryTable<K, V, IdGen, Recorder<K, V>> {
    /// Get a reference to the change log.
    fn changelog(&self) -> &ChangeLog<K, V> {
        &self.watcher.log
    }

    /// Get a mutable reference to the change log.
    fn changelog_mut(&mut self) -> &mut ChangeLog<K, V> {
        &mut self.watcher.log
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_callback_watcher() {
        let mut table = Table::<i32, Recorder<_, _>>::default();

        let id1 = table.insert(1);

        table.remove(&id1);

        let id2 = table.insert(2);

        if let Some(mut val) = table.get_mut(&id2) {
            *val = 3;
        }

        assert_eq!(
            table.changelog().changes().to_vec(),
            vec![
                Change::Insert(0, id1, 1),
                Change::Remove(1, id1, 1),
                Change::Insert(2, id2, 2),
                Change::Update(3, id2, 3),
            ]
        );
    }
}
