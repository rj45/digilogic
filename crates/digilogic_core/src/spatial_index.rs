use crate::transform::{AbsoluteBoundingBox, BoundingBox};
use bevy_ecs::prelude::*;
use bvh_arena::{Bvh, VolumeHandle};
use std::collections::HashMap;

#[derive(Resource, Default)]
pub struct SpatialIndex {
    index: Bvh<Entity, BoundingBox>,
    handles: HashMap<Entity, VolumeHandle>,
}

impl SpatialIndex {
    pub fn insert(&mut self, entity: Entity, bounds: BoundingBox) {
        let handle = self.index.insert(entity, bounds);
        self.handles.insert(entity, handle);
    }

    pub fn remove(&mut self, entity: Entity) {
        if let Some(handle) = self.handles.remove(&entity) {
            self.index.remove(handle);
        }
    }

    pub fn update(&mut self, entity: Entity, bounds: BoundingBox) {
        if let Some(handle) = self.handles.get(&entity) {
            self.index.remove(*handle);
        }
        let handle = self.index.insert(entity, bounds);
        self.handles.insert(entity, handle);
    }

    pub fn query(&self, bounds: BoundingBox, cb: impl FnMut(&Entity)) {
        self.index.for_each_overlaps(&bounds, cb);
    }
}

pub(crate) fn update_spatial_index(
    mut index: ResMut<SpatialIndex>,
    query: Query<(Entity, &AbsoluteBoundingBox), Changed<AbsoluteBoundingBox>>,
) {
    query.iter().for_each(|(entity, bounds)| {
        index.update(entity, **bounds);
    });
}

pub(crate) fn on_remove_update_spatial_index(
    trigger: Trigger<OnRemove, AbsoluteBoundingBox>,
    mut index: ResMut<SpatialIndex>,
) {
    index.remove(trigger.entity());
}
