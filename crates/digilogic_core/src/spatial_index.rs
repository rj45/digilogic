use crate::transform::{AbsoluteBoundingBox, BoundingBox};
use crate::HashMap;
use bevy_ecs::prelude::*;
use bvh_arena::{Bvh, VolumeHandle};

#[allow(missing_debug_implementations)]
#[derive(Resource, Default)]
pub struct SpatialIndex {
    index: Bvh<Entity, BoundingBox>,
    handles: HashMap<Entity, Vec<VolumeHandle>>,
}

impl SpatialIndex {
    pub fn remove(&mut self, entity: Entity) {
        if let Some(handles) = self.handles.remove(&entity) {
            for handle in handles {
                self.index.remove(handle);
            }
        }
    }

    /// Update the spatial index for the given entity with a single bounding box.
    /// Any existing bounding boxes for the entity will be removed.
    pub fn update(&mut self, entity: Entity, bounds: BoundingBox) {
        self.update_all(entity, vec![bounds]);
    }

    /// Update the spatial index for the given entity with multiple bounding boxes.
    /// Any existing bounding boxes for the entity will be removed.
    pub fn update_all(&mut self, entity: Entity, bounds: Vec<BoundingBox>) {
        let handles = if let Some(handles) = self.handles.get_mut(&entity) {
            for handle in handles.iter() {
                self.index.remove(*handle);
            }
            handles.clear();
            handles
        } else {
            self.handles.insert(entity, Vec::new());
            self.handles.get_mut(&entity).unwrap()
        };
        for bound in bounds {
            let handle = self.index.insert(entity, bound);
            handles.push(handle);
        }
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
