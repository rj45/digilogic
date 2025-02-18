#![allow(missing_debug_implementations)]

use std::ops::{Deref, DerefMut};

use super::*;

pub trait Watcher<K, V> {
    type MutGuard<'a>
    where
        V: 'a,
        Self: 'a;

    fn insert(&mut self, key: Id<K>, value: &V);
    fn remove(&mut self, key: Id<K>, value: &V);
    fn update_mut<'a>(&'a mut self, key: Id<K>, value: &'a mut V) -> Self::MutGuard<'a>;
}

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

pub struct ObserverGuard<'a, K, V, O: Observer<K, V>> {
    key: Id<K>,
    value: &'a mut V,
    watcher: &'a mut ObserverWatcher<K, V, O>,
}

impl<K, V, O: Observer<K, V>> Deref for ObserverGuard<'_, K, V, O> {
    type Target = V;

    fn deref(&self) -> &Self::Target {
        self.value
    }
}

impl<K, V, O: Observer<K, V>> DerefMut for ObserverGuard<'_, K, V, O> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.value
    }
}

impl<K, V, O: Observer<K, V>> Drop for ObserverGuard<'_, K, V, O> {
    fn drop(&mut self) {
        if let Some(observer) = self.watcher.observer.as_mut() {
            observer.update(self.key, self.value);
        }
    }
}

pub trait Observer<K, V> {
    fn insert(&mut self, key: Id<K>, value: &V);
    fn remove(&mut self, key: Id<K>, value: &V);
    fn update(&mut self, key: Id<K>, value: &V);
}

pub struct ObserverWatcher<K, V, O> {
    observer: Option<O>,
    _phantom: PhantomData<fn(Id<K>, &V) -> &V>,
}

impl<K, V, O> Default for ObserverWatcher<K, V, O> {
    fn default() -> Self {
        Self {
            observer: None,
            _phantom: PhantomData,
        }
    }
}

impl<K, V, O: Observer<K, V>> ObserverWatcher<K, V, O> {
    pub fn set_observer(&mut self, observer: O) {
        self.observer = Some(observer);
    }

    pub fn observer(&self) -> Option<&O> {
        self.observer.as_ref()
    }
}

impl<K, V, O> Watcher<K, V> for ObserverWatcher<K, V, O>
where
    O: Observer<K, V>,
{
    type MutGuard<'a>
        = ObserverGuard<'a, K, V, O>
    where
        V: 'a,
        K: 'a,
        O: 'a;

    fn insert(&mut self, key: Id<K>, value: &V) {
        if let Some(observer) = self.observer.as_mut() {
            observer.insert(key, value);
        }
    }

    fn remove(&mut self, key: Id<K>, value: &V) {
        if let Some(observer) = self.observer.as_mut() {
            observer.remove(key, value);
        }
    }

    fn update_mut<'a>(&'a mut self, key: Id<K>, value: &'a mut V) -> Self::MutGuard<'a> {
        ObserverGuard {
            key,
            value,
            watcher: self,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_callback_watcher() {
        #[derive(PartialEq, Eq, Debug)]
        enum ChangeType {
            Insert,
            Update,
            Remove,
        }

        struct Obs {
            values: Vec<(ChangeType, Id<i32>, i32)>,
        }

        impl Observer<i32, i32> for Obs {
            fn insert(&mut self, id: Id<i32>, value: &i32) {
                self.values.push((ChangeType::Insert, id, *value));
            }

            fn remove(&mut self, id: Id<i32>, value: &i32) {
                self.values.push((ChangeType::Remove, id, *value));
            }

            fn update(&mut self, id: Id<i32>, value: &i32) {
                self.values.push((ChangeType::Update, id, *value));
            }
        }

        let mut table = Table::<i32, ObserverWatcher<_, _, Obs>>::default();

        let obs = Obs { values: Vec::new() };
        table.watcher().set_observer(obs);

        let id1 = table.insert(1);

        table.remove(&id1);

        let id2 = table.insert(2);

        if let Some(mut val) = table.get_mut(&id2) {
            *val = 3;
        }

        assert_eq!(
            table.watcher().observer().unwrap().values,
            vec![
                (ChangeType::Insert, id1, 1),
                (ChangeType::Remove, id1, 1),
                (ChangeType::Insert, id2, 2),
                (ChangeType::Update, id2, 3)
            ]
        );
    }
}
