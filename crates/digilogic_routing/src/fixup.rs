use crate::{NetQuery, VertexKind};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::entity::Entity;
use digilogic_core::components::Child;
use digilogic_core::{fixed, Fixed, HashMap};
use smallvec::{smallvec, SmallVec};

const MIN_WIRE_SPACING: Fixed = fixed!(10);

#[derive(Debug)]
struct VertexPair {
    start_inclusive: Fixed,
    end_inclusive: Fixed,
    net: Entity,
    index: usize,
    track: usize,
}

impl VertexPair {
    #[inline]
    fn new(start_inclusive: Fixed, end_inclusive: Fixed, net: Entity, index: usize) -> Self {
        Self {
            start_inclusive,
            end_inclusive,
            net,
            index,
            track: usize::MAX,
        }
    }

    #[inline]
    fn overlaps(&self, other: &Self) -> bool {
        !(((self.start_inclusive - MIN_WIRE_SPACING) > other.end_inclusive)
            || ((self.end_inclusive + MIN_WIRE_SPACING) < other.start_inclusive))
    }
}

#[derive(Debug)]
struct RangeItem {
    start_inclusive: Fixed,
    end_inclusive: Fixed,
    values: SmallVec<[VertexPair; 1]>,
    track_count: usize,
}

impl RangeItem {
    fn assign_tracks(&mut self) {
        // TODO: save memory using bitvec
        let mut used_tracks: SmallVec<[bool; 16]> = SmallVec::new();

        // This is essentially greedy graph coloring.
        for i in 0..self.values.len() {
            let (&mut ref head, tail) = self.values.split_at_mut(i);
            let current = tail.first_mut().unwrap();

            used_tracks.clear();
            for other in head {
                if current.overlaps(other) {
                    if used_tracks.len() <= other.track {
                        used_tracks.resize(other.track + 1, false);
                    }

                    used_tracks[other.track] = true;
                }
            }

            current.track = used_tracks
                .iter()
                .position(|&x| !x)
                .unwrap_or(used_tracks.len());
            self.track_count = self.track_count.max(current.track + 1);
        }
    }
}

#[derive(Debug, Default)]
struct RangeSet {
    ranges: Vec<RangeItem>,
}

impl RangeSet {
    #[cfg(debug_assertions)]
    fn is_sorted(&self) -> bool {
        for range in &self.ranges {
            if range.start_inclusive >= range.end_inclusive {
                return false;
            }
        }

        for pair in self.ranges.windows(2) {
            let [a, b] = pair else {
                unreachable!();
            };

            if a.end_inclusive >= b.start_inclusive {
                return false;
            }
        }

        true
    }

    fn merge_ranges(&mut self, index: usize) {
        while (index + 1) < self.ranges.len() {
            let (head, tail) = self.ranges.split_at_mut(index + 1);

            let dst_range = &mut head[index];
            let src_range = &mut tail[0];

            if src_range.start_inclusive > dst_range.end_inclusive {
                break;
            }

            dst_range.values.extend(src_range.values.drain(..));
            self.ranges.remove(index + 1);
        }
    }

    fn insert(
        &mut self,
        start_inclusive: Fixed,
        end_inclusive: Fixed,
        net: Entity,
        vertex_index: usize,
    ) {
        match self
            .ranges
            .binary_search_by_key(&start_inclusive, |range| range.start_inclusive)
        {
            Ok(index) => {
                let existing_range = &mut self.ranges[index];

                // Start of the new range is identical to an existing one, so they need to merge.
                existing_range.values.push(VertexPair::new(
                    start_inclusive,
                    end_inclusive,
                    net,
                    vertex_index,
                ));

                if existing_range.end_inclusive < end_inclusive {
                    // Existing range is shorter than the new one, extend it.
                    existing_range.end_inclusive = end_inclusive;

                    // Merge following ranges if they overlap with the extended one.
                    self.merge_ranges(index);
                }
            }
            Err(0) => {
                // The new range will be the new first range in the list.

                'insert: {
                    // Check if it needs to merge with following ranges.
                    if let Some(next_range) = self.ranges.first_mut() {
                        if next_range.start_inclusive <= end_inclusive {
                            // The ranges do need to merge, so use the existing range object.
                            next_range.start_inclusive = start_inclusive;
                            next_range.values.push(VertexPair::new(
                                start_inclusive,
                                end_inclusive,
                                net,
                                vertex_index,
                            ));

                            if next_range.end_inclusive < end_inclusive {
                                // Existing range is shorter than the new one, extend it.
                                next_range.end_inclusive = end_inclusive;

                                // Merge following ranges if they overlap with the extended one.
                                self.merge_ranges(0);
                            }

                            break 'insert;
                        }
                    }

                    // No merge required, insert a new range.
                    self.ranges.insert(
                        0,
                        RangeItem {
                            start_inclusive,
                            end_inclusive,
                            values: smallvec![VertexPair::new(
                                start_inclusive,
                                end_inclusive,
                                net,
                                vertex_index
                            )],
                            track_count: 0,
                        },
                    );
                }
            }
            Err(index) => {
                'insert: {
                    // Check if the new range needs to be merged with the previous one.
                    if let Some(prev_range) = self.ranges.get_mut(index - 1) {
                        if prev_range.end_inclusive >= start_inclusive {
                            // The ranges do need to merge, so use the existing range object.
                            prev_range.values.push(VertexPair::new(
                                start_inclusive,
                                end_inclusive,
                                net,
                                vertex_index,
                            ));

                            if prev_range.end_inclusive < end_inclusive {
                                // Existing range is shorter than the new one, extend it.
                                prev_range.end_inclusive = end_inclusive;

                                // Merge following ranges if they overlap with the extended one.
                                self.merge_ranges(index - 1);
                            }

                            break 'insert;
                        }
                    }

                    // Check if it needs to merge with following ranges.
                    if let Some(next_range) = self.ranges.get_mut(index) {
                        if next_range.start_inclusive <= end_inclusive {
                            // The ranges do need to merge, so use the existing range object.
                            next_range.start_inclusive = start_inclusive;
                            next_range.values.push(VertexPair::new(
                                start_inclusive,
                                end_inclusive,
                                net,
                                vertex_index,
                            ));

                            if next_range.end_inclusive < end_inclusive {
                                // Existing range is shorter than the new one, extend it.
                                next_range.end_inclusive = end_inclusive;

                                // Merge following ranges if they overlap with the extended one.
                                self.merge_ranges(index);
                            }

                            break 'insert;
                        }
                    }

                    // No merge required, insert a new range.
                    self.ranges.insert(
                        index,
                        RangeItem {
                            start_inclusive,
                            end_inclusive,
                            values: smallvec![VertexPair::new(
                                start_inclusive,
                                end_inclusive,
                                net,
                                vertex_index
                            )],
                            track_count: 0,
                        },
                    );
                }
            }
        }

        #[cfg(debug_assertions)]
        if !self.is_sorted() {
            eprintln!("{self:#?}");
            panic!();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert() {
        let mut set = RangeSet::default();
        set.insert(fixed!(-460), fixed!(-430), Entity::PLACEHOLDER, 0);
        set.insert(fixed!(-160), fixed!(4720), Entity::PLACEHOLDER, 0);
        set.insert(fixed!(-280), fixed!(3240), Entity::PLACEHOLDER, 0);
    }
}

#[tracing::instrument(skip_all)]
pub fn separate_wires(circuit_children: &RelationsItem<Child>, nets: &mut NetQuery) {
    let mut horizontal_sets: HashMap<Fixed, RangeSet> = HashMap::default();
    let mut vertical_sets: HashMap<Fixed, RangeSet> = HashMap::default();

    circuit_children
        .join::<Child>(&*nets)
        .for_each(|((net, vertices), _)| {
            for (i, pair) in vertices.windows(2).enumerate() {
                let [a, b] = pair else {
                    unreachable!();
                };

                if (a.kind != VertexKind::Normal) || (b.kind != VertexKind::Normal) {
                    continue;
                }

                if a.position.y == b.position.y {
                    let min_x = a.position.x.min(b.position.x);
                    let max_x = a.position.x.max(b.position.x);
                    horizontal_sets
                        .entry(a.position.y)
                        .or_default()
                        .insert(min_x, max_x, net, i);
                } else if a.position.x == b.position.x {
                    let min_y = a.position.y.min(b.position.y);
                    let max_y = a.position.y.max(b.position.y);
                    vertical_sets
                        .entry(a.position.x)
                        .or_default()
                        .insert(min_y, max_y, net, i);
                }
            }
        });

    for (y, set) in horizontal_sets {
        for mut range in set.ranges {
            range.assign_tracks();
            let track_offset = Fixed::try_from_usize(range.track_count - 1).unwrap() * fixed!(0.5);

            for pair in range.values {
                let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
                let offset = Fixed::try_from_usize(pair.track).unwrap() - track_offset;
                vertices.0[pair.index].position.y = y + offset * MIN_WIRE_SPACING;
                vertices.0[pair.index + 1].position.y = y + offset * MIN_WIRE_SPACING;
            }
        }
    }

    for (x, set) in vertical_sets {
        for mut range in set.ranges {
            range.assign_tracks();
            let track_offset = Fixed::try_from_usize(range.track_count - 1).unwrap() * fixed!(0.5);

            for pair in range.values {
                let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
                let offset = Fixed::try_from_usize(pair.track).unwrap() - track_offset;
                vertices.0[pair.index].position.x = x + offset * MIN_WIRE_SPACING;
                vertices.0[pair.index + 1].position.x = x + offset * MIN_WIRE_SPACING;
            }
        }
    }
}
