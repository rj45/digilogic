use std::{
    collections::HashMap,
    hash::{Hash, Hasher},
    sync::{Arc, Weak},
};

#[derive(Default, Clone, Debug)]
pub struct Intern(HashMap<u64, Vec<Weak<str>>>); // TODO: use smallvec, AHashMap

impl Intern {
    pub fn intern(&mut self, s: &str) -> Arc<str> {
        let mut hasher = std::collections::hash_map::DefaultHasher::new();
        s.hash(&mut hasher);
        let hash = hasher.finish();
        if let Some(weak) = self.0.get_mut(&hash) {
            weak.retain(|weak| weak.upgrade().is_some());
            for weak in weak.iter() {
                if let Some(arc) = weak.upgrade() {
                    if arc == s.into() {
                        return arc;
                    }
                }
            }
            if weak.is_empty() {
                self.0.remove(&hash);
            }
        }
        let arc = s.into();
        let weak = Arc::downgrade(&arc);
        let found = self.0.get_mut(&hash);
        if let Some(v) = found {
            v.push(weak);
        } else {
            self.0.insert(hash, vec![weak]);
        }
        arc
    }

    pub fn merge(&mut self, other: &Intern) {
        for (_, weaks) in other.0.iter() {
            let weaks: Vec<Arc<str>> = weaks.iter().filter_map(|weak| weak.upgrade()).collect();
            for weak in weaks {
                self.intern(&weak);
            }
        }
    }
}
