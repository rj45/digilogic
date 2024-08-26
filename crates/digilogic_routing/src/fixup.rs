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
    index: u32,
    track: u32,
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
    immovable_pairs: u32,
    track_count: u32,
}

impl Corridor {
    fn insert(
        &mut self,
        start_inclusive: Fixed,
        end_inclusive: Fixed,
        net: Entity,
        index: u32,
        can_move: bool,
    ) {
        let pair = VertexPair {
            start_inclusive,
            end_inclusive,
            net,
            index,
            track: u32::MAX,
        };

        if can_move {
            self.pairs.push(pair);
        } else {
            if self.pairs.len() > (self.immovable_pairs as usize) {
                // Swap-insert to avoid memcopy
                let prev = std::mem::replace(&mut self.pairs[self.immovable_pairs as usize], pair);
                self.pairs.push(prev);
            } else {
                self.pairs.push(pair);
            }

            self.immovable_pairs += 1;
        }
    }

    #[inline]
    fn track_offset(&self) -> u32 {
        self.track_count / 2
    }

    fn assign_tracks(&mut self) {
        // TODO: save memory using bitvec
        let mut used_tracks: SmallVec<[bool; 16]> = SmallVec::new();

        // This is essentially greedy graph coloring.
        for i in (self.immovable_pairs as usize)..self.pairs.len() {
            let (&mut ref head, tail) = self.pairs.split_at_mut(i);
            let head = &head[self.immovable_pairs as usize..];
            let current = tail.first_mut().unwrap();

            used_tracks.clear();
            for other in head {
                if current.overlaps(other) {
                    if used_tracks.len() <= (other.track as usize) {
                        used_tracks.resize((other.track as usize) + 1, false);
                    }

                    used_tracks[other.track as usize] = true;
                }
            }

            current.track = used_tracks
                .iter()
                .position(|&x| !x)
                .unwrap_or(used_tracks.len()) as u32;
            self.track_count = self.track_count.max(current.track + 1);
        }

        if self.immovable_pairs > 0 {
            self.track_count += 1;
            let track_offset = self.track_offset();

            self.pairs[..self.immovable_pairs as usize]
                .iter_mut()
                .for_each(|pair| pair.track = track_offset);
            self.pairs[self.immovable_pairs as usize..]
                .iter_mut()
                .filter(|pair| pair.track >= track_offset)
                .for_each(|pair| pair.track += 1);
        }
    }
}

struct Tail<'a, T> {
    offset: usize,
    tail: &'a mut [T],
}

impl<T> Tail<'_, T> {
    fn split_pair(&mut self, pair_index: usize) -> (&mut T, &mut T, Tail<T>) {
        let (a, tail) = self.tail[(pair_index - self.offset)..]
            .split_first_mut()
            .unwrap();
        let (b, tail) = tail.split_first_mut().unwrap();

        (
            a,
            b,
            Tail {
                offset: pair_index + 2,
                tail,
            },
        )
    }
}

impl<'a, T> From<&'a mut [T]> for Tail<'a, T> {
    #[inline]
    fn from(tail: &'a mut [T]) -> Self {
        Self { offset: 0, tail }
    }
}

impl<T> Index<usize> for Tail<'_, T> {
    type Output = T;

    #[inline]
    fn index(&self, index: usize) -> &Self::Output {
        if index >= self.offset {
            &self.tail[index - self.offset]
        } else {
            panic!("attempt to index into gap")
        }
    }
}

impl<T> IndexMut<usize> for Tail<'_, T> {
    #[inline]
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        if index >= self.offset {
            &mut self.tail[index - self.offset]
        } else {
            panic!("attempt to index into gap")
        }
    }
}

fn find_min_max_x(v: &Vertex, vertices: &[Vertex], min_x: &mut Fixed, max_x: &mut Fixed) {
    // Recursively follows all horizontal corner junctions to find the actual start and and X coordinates.

    for junction in &v.connected_junctions {
        match junction.kind {
            JunctionKind::LineSegment => (),
            JunctionKind::Corner => {
                let prev = &vertices[junction.vertex_index as usize - 1];
                if prev.position.y == v.position.y {
                    *min_x = (*min_x).min(prev.position.x);
                    *max_x = (*max_x).max(prev.position.x);
                    find_min_max_x(prev, vertices, min_x, max_x);
                }
            }
        }
    }
}

fn find_min_max_y(v: &Vertex, vertices: &[Vertex], min_y: &mut Fixed, max_y: &mut Fixed) {
    // Recursively follows all vertical corner junctions to find the actual start and and Y coordinates.

    for junction in &v.connected_junctions {
        match junction.kind {
            JunctionKind::LineSegment => (),
            JunctionKind::Corner => {
                let prev = &vertices[junction.vertex_index as usize - 1];
                if prev.position.x == v.position.x {
                    *min_y = (*min_y).min(prev.position.y);
                    *max_y = (*max_y).max(prev.position.y);
                    find_min_max_y(prev, vertices, min_y, max_y);
                }
            }
        }
    }
}

fn move_junctions(a: &Vertex, b: &Vertex, vertices: &mut Tail<Vertex>) {
    // We can use the tail as the vertex list because junction vertices
    // will always occur after the line segment they are connected to.

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

                    let (a, b, mut vertices) = vertices.split_pair(junction_index - 1);
                    move_junctions(a, b, &mut vertices);
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

                    let (a, b, mut vertices) = vertices.split_pair(junction_index - 1);
                    move_junctions(a, b, &mut vertices);
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

                let can_move = match (a.kind, b.kind) {
                    (VertexKind::WireEnd { .. }, _) => continue,
                    // Corner junctions are not inserted because they are
                    // considered part of the segment they connect to.
                    (
                        _,
                        VertexKind::WireEnd {
                            junction_kind: Some(JunctionKind::Corner),
                        },
                    ) => continue,
                    (VertexKind::WireStart { .. }, _) => false,
                    (
                        _,
                        VertexKind::WireEnd {
                            junction_kind: None,
                        },
                    ) => false,
                    _ => true,
                };

                if a.position.y == b.position.y {
                    let mut min_x = a.position.x.min(b.position.x);
                    let mut max_x = a.position.x.max(b.position.x);

                    find_min_max_x(a, vertices, &mut min_x, &mut max_x);
                    find_min_max_x(b, vertices, &mut min_x, &mut max_x);

                    horizontal_corridors
                        .entry(a.position.y)
                        .or_default()
                        .insert(min_x, max_x, net, i as u32, can_move);
                } else if a.position.x == b.position.x {
                    let mut min_y = a.position.y.min(b.position.y);
                    let mut max_y = a.position.y.max(b.position.y);

                    find_min_max_y(a, vertices, &mut min_y, &mut max_y);
                    find_min_max_y(b, vertices, &mut min_y, &mut max_y);

                    vertical_corridors
                        .entry(a.position.x)
                        .or_default()
                        .insert(min_y, max_y, net, i as u32, can_move);
                }
            }
        });

    for (y, mut corridor) in horizontal_corridors {
        corridor.assign_tracks();
        let track_offset = Fixed::try_from_u32(corridor.track_offset()).unwrap();

        for pair in corridor.pairs {
            let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
            let mut vertices = Tail::from(vertices.0.as_mut_slice());
            let (a, b, mut vertices) = vertices.split_pair(pair.index as usize);

            let offset = Fixed::try_from_u32(pair.track).unwrap() - track_offset;
            if offset != fixed!(0) {
                let y = y + offset * MIN_WIRE_SPACING;
                a.position.y = y;
                b.position.y = y;

                move_junctions(a, b, &mut vertices);
            }
        }
    }

    for (x, mut corridor) in vertical_corridors {
        corridor.assign_tracks();
        let track_offset = Fixed::try_from_u32(corridor.track_offset()).unwrap();

        for pair in corridor.pairs {
            let ((_, mut vertices), _) = nets.get_mut(pair.net).unwrap();
            let mut vertices = Tail::from(vertices.0.as_mut_slice());
            let (a, b, mut vertices) = vertices.split_pair(pair.index as usize);

            let offset = Fixed::try_from_u32(pair.track).unwrap() - track_offset;
            if offset != fixed!(0) {
                let x = x + offset * MIN_WIRE_SPACING;
                a.position.x = x;
                b.position.x = x;

                move_junctions(a, b, &mut vertices);
            }
        }
    }
}
