use crate::{JunctionKind, NetQuery, Vertex, VertexKind};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::entity::Entity;
use digilogic_core::components::Child;
use digilogic_core::{fixed, Fixed, HashMap};
use smallvec::SmallVec;
use std::ops::{Index, IndexMut};

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
    fn overlaps(&self, other: &Self) -> bool {
        !(((self.start_inclusive - MIN_WIRE_SPACING) > other.end_inclusive)
            || ((self.end_inclusive + MIN_WIRE_SPACING) < other.start_inclusive))
    }
}

#[derive(Debug, Default)]
struct Corridor {
    pairs: SmallVec<[VertexPair; 1]>,
    track_count: usize,
}

impl Corridor {
    #[inline]
    fn insert(&mut self, start_inclusive: Fixed, end_inclusive: Fixed, net: Entity, index: usize) {
        self.pairs.push(VertexPair {
            start_inclusive,
            end_inclusive,
            net,
            index,
            track: usize::MAX,
        })
    }

    fn assign_tracks(&mut self) {
        // TODO: save memory using bitvec
        let mut used_tracks: SmallVec<[bool; 16]> = SmallVec::new();

        // This is essentially greedy graph coloring.
        for i in 0..self.pairs.len() {
            let (&mut ref head, tail) = self.pairs.split_at_mut(i);
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

struct HeadTail<'a, T> {
    gap_size: usize,
    head: &'a mut [T],
    tail: &'a mut [T],
}

impl<'a, T> HeadTail<'a, T> {
    fn split_pair(list: &'a mut [T], pair_index: usize) -> (&'a mut T, &'a mut T, Self) {
        let (head, tail) = list.split_at_mut(pair_index);
        let (a, tail) = tail.split_first_mut().unwrap();
        let (b, tail) = tail.split_first_mut().unwrap();

        (
            a,
            b,
            Self {
                gap_size: 2,
                head,
                tail,
            },
        )
    }
}

impl<T> Index<usize> for HeadTail<'_, T> {
    type Output = T;

    fn index(&self, index: usize) -> &Self::Output {
        if index < self.head.len() {
            &self.head[index]
        } else if index >= (self.head.len() + self.gap_size) {
            &self.tail[index - self.head.len() - self.gap_size]
        } else {
            panic!("attempt to index into gap")
        }
    }
}

impl<T> IndexMut<usize> for HeadTail<'_, T> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        if index < self.head.len() {
            &mut self.head[index]
        } else if index >= (self.head.len() + self.gap_size) {
            &mut self.tail[index - self.head.len() - self.gap_size]
        } else {
            panic!("attempt to index into gap")
        }
    }
}

fn move_junctions(a: &Vertex, b: &Vertex, vertices: &mut HeadTail<Vertex>) {
    for junction in &a.connected_junctions {
        let junction_index = junction.vertex_index as usize;

        match junction.kind {
            JunctionKind::LineSegment => {
                if a.position.y == b.position.y {
                    vertices[junction_index].position.y = a.position.y;
                } else if a.position.x == b.position.x {
                    vertices[junction_index].position.x = a.position.x;
                }
            }
            JunctionKind::Corner => {
                let is_horizontal =
                    vertices[junction_index].position.y == vertices[junction_index - 1].position.y;
                let is_vertical =
                    vertices[junction_index].position.x == vertices[junction_index - 1].position.x;

                vertices[junction_index].position = a.position;
                if matches!(
                    vertices[junction_index - 1].kind,
                    VertexKind::WireStart { .. }
                ) {
                    // TODO: we can't move this vertex because it connects to a port, but this prodcues a diagonal wire
                } else {
                    if is_horizontal {
                        vertices[junction_index - 1].position.y = a.position.y;
                    } else if is_vertical {
                        vertices[junction_index - 1].position.x = a.position.x;
                    }

                    // TODO: move junctions recursively
                }
            }
        }
    }

    for junction in &b.connected_junctions {
        let junction_index = junction.vertex_index as usize;

        match junction.kind {
            JunctionKind::LineSegment => (),
            JunctionKind::Corner => {
                let is_horizontal =
                    vertices[junction_index].position.y == vertices[junction_index - 1].position.y;
                let is_vertical =
                    vertices[junction_index].position.x == vertices[junction_index - 1].position.x;

                vertices[junction_index].position = b.position;
                if matches!(
                    vertices[junction_index - 1].kind,
                    VertexKind::WireStart { .. }
                ) {
                    // TODO: we can't move this vertex because it connects to a port, but this prodcues a diagonal wire
                } else {
                    if is_horizontal {
                        vertices[junction_index - 1].position.y = b.position.y;
                    } else if is_vertical {
                        vertices[junction_index - 1].position.x = b.position.x;
                    }

                    // TODO: move junctions recursively
                }
            }
        }
    }
}

#[tracing::instrument(skip_all)]
pub fn separate_wires(circuit_children: &RelationsItem<Child>, nets: &mut NetQuery) {
    let mut horizontal_corridors: HashMap<Fixed, Corridor> = HashMap::default();
    let mut vertical_corridors: HashMap<Fixed, Corridor> = HashMap::default();

    circuit_children
        .join::<Child>(&*nets)
        .for_each(|((net, vertices), _)| {
            for (i, pair) in vertices.windows(2).enumerate() {
                let [a, b] = pair else {
                    unreachable!();
                };

                match (a.kind, b.kind) {
                    (
                        VertexKind::Normal,
                        VertexKind::Normal
                        | VertexKind::WireEnd {
                            junction_kind: Some(JunctionKind::LineSegment),
                        },
                    ) => (),
                    _ => continue,
                }

                if a.position.y == b.position.y {
                    let min_x = a.position.x.min(b.position.x);
                    let max_x = a.position.x.max(b.position.x);
                    horizontal_corridors
                        .entry(a.position.y)
                        .or_default()
                        .insert(min_x, max_x, net, i);
                } else if a.position.x == b.position.x {
                    let min_y = a.position.y.min(b.position.y);
                    let max_y = a.position.y.max(b.position.y);
                    vertical_corridors
                        .entry(a.position.x)
                        .or_default()
                        .insert(min_y, max_y, net, i);
                }
            }
        });

    for (y, mut corridor) in horizontal_corridors {
        corridor.assign_tracks();
        let track_offset = Fixed::try_from_usize(corridor.track_count - 1).unwrap() * fixed!(0.5);

        for pair in corridor.pairs {
            let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
            let (a, b, mut vertices) = HeadTail::split_pair(&mut vertices.0, pair.index);

            let offset = Fixed::try_from_usize(pair.track).unwrap() - track_offset;
            let y = y + offset * MIN_WIRE_SPACING;
            a.position.y = y;
            b.position.y = y;

            move_junctions(a, b, &mut vertices);
        }
    }

    for (x, mut corridor) in vertical_corridors {
        corridor.assign_tracks();
        let track_offset = Fixed::try_from_usize(corridor.track_count - 1).unwrap() * fixed!(0.5);

        for pair in corridor.pairs {
            let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
            let (a, b, mut vertices) = HeadTail::split_pair(&mut vertices.0, pair.index);

            let offset = Fixed::try_from_usize(pair.track).unwrap() - track_offset;
            let x = x + offset * MIN_WIRE_SPACING;
            a.position.x = x;
            b.position.x = x;

            move_junctions(a, b, &mut vertices);
        }
    }
}
