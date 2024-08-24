use crate::bit_grid::*;
use crate::segment_tree::*;
use crate::{CircuitTree, SymbolQuery};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::{fixed, Fixed, HashMap};
use serde::{Deserialize, Serialize};
use std::cell::RefCell;
use std::ops::{Index, IndexMut};

pub type NodeIndex = u32;
pub const INVALID_NODE_INDEX: NodeIndex = u32::MAX;

const BOUNDING_BOX_PADDING: Fixed = fixed!(10);

#[derive(Debug, Clone, Copy)]
#[repr(C)]
struct Anchor {
    position: Vec2,
    bounding_box: Entity,
    connect_directions: Directions,
}

impl Anchor {
    #[inline]
    const fn new(position: Vec2, connect_directions: Directions) -> Self {
        Self {
            position,
            bounding_box: Entity::PLACEHOLDER,
            connect_directions,
        }
    }

    #[inline]
    const fn new_port(
        position: Vec2,
        connect_directions: Directions,
        bounding_box: Entity,
    ) -> Self {
        Self {
            position,
            bounding_box,
            connect_directions,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[repr(C)]
pub(crate) struct NeighborList(
    /// The neighbor in the positive X direction, or `RT_INVALID_NODE_INDEX` if none.
    NodeIndex,
    /// The neighbor in the negative X direction, or `RT_INVALID_NODE_INDEX` if none.
    NodeIndex,
    /// The neighbor in the positive Y direction, or `RT_INVALID_NODE_INDEX` if none.
    NodeIndex,
    /// The neighbor in the negative Y direction, or `RT_INVALID_NODE_INDEX` if none.
    NodeIndex,
);

impl NeighborList {
    #[inline]
    const fn new() -> Self {
        Self(
            INVALID_NODE_INDEX,
            INVALID_NODE_INDEX,
            INVALID_NODE_INDEX,
            INVALID_NODE_INDEX,
        )
    }

    #[inline]
    fn count(&self) -> usize {
        (self.0 != INVALID_NODE_INDEX) as usize
            + (self.1 != INVALID_NODE_INDEX) as usize
            + (self.2 != INVALID_NODE_INDEX) as usize
            + (self.3 != INVALID_NODE_INDEX) as usize
    }

    pub(crate) fn find(&self, node: NodeIndex) -> Option<Direction> {
        if node == INVALID_NODE_INDEX {
            return None;
        }

        Direction::ALL.into_iter().find(|&dir| self[dir] == node)
    }
}

impl Index<Direction> for NeighborList {
    type Output = NodeIndex;

    #[inline]
    fn index(&self, index: Direction) -> &Self::Output {
        match index {
            Direction::PosX => &self.0,
            Direction::NegX => &self.1,
            Direction::PosY => &self.2,
            Direction::NegY => &self.3,
        }
    }
}

impl IndexMut<Direction> for NeighborList {
    #[inline]
    fn index_mut(&mut self, index: Direction) -> &mut Self::Output {
        match index {
            Direction::PosX => &mut self.0,
            Direction::NegX => &mut self.1,
            Direction::PosY => &mut self.2,
            Direction::NegY => &mut self.3,
        }
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
pub struct Node {
    /// The position of the node.
    pub position: Vec2,
    /// The neighbors of the node.
    pub(crate) neighbors: NeighborList,
    /// Whether this node was created from an explicit point of interest.
    pub is_explicit: bool,
    /// The directions this node is allowed to connect to.  
    /// A direction being legal does not mean a neighbor in that direction actually exists.
    pub legal_directions: Directions,
}

impl Node {
    /// The number of neighbors this node has.
    #[inline]
    pub fn neighbor_count(&self) -> usize {
        self.neighbors.count()
    }

    /// Gets the index of the nodes neighbor in the specified direction.
    #[inline]
    pub fn get_neighbor(&self, dir: Direction) -> Option<usize> {
        let index = self.neighbors[dir];
        if index == INVALID_NODE_INDEX {
            None
        } else {
            Some(index as usize)
        }
    }

    /// Determines if the given node is a neighbor of this one, and if yes in which direction.
    #[inline]
    pub fn is_neighbor(&self, node: usize) -> Option<Direction> {
        let node: NodeIndex = node.try_into().ok()?;
        self.neighbors.find(node)
    }
}

#[derive(Default, Debug, Clone)]
#[repr(transparent)]
pub(crate) struct NodeList(Vec<Node>);

impl NodeList {
    #[inline]
    fn clear(&mut self) {
        self.0.clear();
    }

    #[inline]
    fn push(
        &mut self,
        position: Vec2,
        is_explicit: bool,
        legal_directions: Directions,
    ) -> NodeIndex {
        let index: NodeIndex = self.0.len().try_into().expect("too many nodes");
        assert_ne!(index, INVALID_NODE_INDEX, "too many nodes");

        self.0.push(Node {
            position,
            neighbors: NeighborList::new(),
            is_explicit,
            legal_directions,
        });

        index
    }
}

impl Index<NodeIndex> for NodeList {
    type Output = Node;

    #[inline]
    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.0[index as usize]
    }
}

impl IndexMut<NodeIndex> for NodeList {
    #[inline]
    fn index_mut(&mut self, index: NodeIndex) -> &mut Self::Output {
        &mut self.0[index as usize]
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct HorizontalBoundingBox {
    id: Entity,
    min_x: Fixed,
    max_x: Fixed,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct VerticalBoundingBox {
    id: Entity,
    min_y: Fixed,
    max_y: Fixed,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize)]
pub(crate) struct BoundingBoxList {
    horizontal_bounding_boxes: SegmentTree<HorizontalBoundingBox>,
    vertical_bounding_boxes: SegmentTree<VerticalBoundingBox>,
}

impl BoundingBoxList {
    fn build(
        &mut self,
    ) -> (
        SegmentTreeBuilder<HorizontalBoundingBox>,
        SegmentTreeBuilder<VerticalBoundingBox>,
    ) {
        (
            self.horizontal_bounding_boxes.build(),
            self.vertical_bounding_boxes.build(),
        )
    }

    #[inline]
    fn iter_containing_horizontal(&self, y: Fixed) -> ContainingSegmentIter<HorizontalBoundingBox> {
        self.horizontal_bounding_boxes.iter_containing(y)
    }

    #[inline]
    fn iter_containing_vertical(&self, x: Fixed) -> ContainingSegmentIter<VerticalBoundingBox> {
        self.vertical_bounding_boxes.iter_containing(x)
    }
}

/// Determines if two horizontally aligned points have a sightline to each other.
fn points_have_horizontal_sightline(
    bounding_boxes: ContainingSegmentIter<HorizontalBoundingBox>,
    x1: Fixed,
    x2: Fixed,
    ignore_box: Entity,
) -> bool {
    assert!(x1 < x2);

    for bb in bounding_boxes {
        if (ignore_box != Entity::PLACEHOLDER) && (bb.id == ignore_box) {
            continue;
        }

        if (x2 < bb.min_x) || (x1 > bb.max_x) {
            continue;
        }

        return false;
    }

    true
}

/// Determines if two vertically aligned points have a sightline to each other.
fn points_have_vertical_sightline(
    bounding_boxes: ContainingSegmentIter<VerticalBoundingBox>,
    y1: Fixed,
    y2: Fixed,
    ignore_box: Entity,
) -> bool {
    assert!(y1 < y2);

    for bb in bounding_boxes {
        if (ignore_box != Entity::PLACEHOLDER) && (bb.id == ignore_box) {
            continue;
        }

        if (y2 < bb.min_y) || (y1 > bb.max_y) {
            continue;
        }

        return false;
    }

    true
}

/// Finds the last (inclusive) x1 coordinate in the negative direction
/// that shares a sightline with the given point (x2, y).
fn find_neg_x_cutoff(
    bounding_boxes: ContainingSegmentIter<HorizontalBoundingBox>,
    x1_coords: &[Fixed],
    x2: Fixed,
    offset: usize,
    ignore_box: Entity,
) -> usize {
    if x1_coords.is_empty() {
        return offset;
    }

    let center = x1_coords.len() / 2;
    let x1 = x1_coords[center];

    if points_have_horizontal_sightline(bounding_boxes.clone(), x1, x2, ignore_box) {
        find_neg_x_cutoff(bounding_boxes, &x1_coords[..center], x2, offset, ignore_box)
    } else {
        find_neg_x_cutoff(
            bounding_boxes,
            &x1_coords[(center + 1)..],
            x2,
            offset + center + 1,
            ignore_box,
        )
    }
}

/// Finds the last (exclusive) x2 coordinate in the positive direction
/// that shares a sightline with the given point (x1, y).
fn find_pos_x_cutoff(
    bounding_boxes: ContainingSegmentIter<HorizontalBoundingBox>,
    x1: Fixed,
    x2_coords: &[Fixed],
    offset: usize,
    ignore_box: Entity,
) -> usize {
    if x2_coords.is_empty() {
        return offset;
    }

    let center = x2_coords.len() / 2;
    let x2 = x2_coords[center];

    if points_have_horizontal_sightline(bounding_boxes.clone(), x1, x2, ignore_box) {
        find_pos_x_cutoff(
            bounding_boxes,
            x1,
            &x2_coords[(center + 1)..],
            offset + center + 1,
            ignore_box,
        )
    } else {
        find_pos_x_cutoff(bounding_boxes, x1, &x2_coords[..center], offset, ignore_box)
    }
}

/// Finds the last (inclusive) y1 coordinate in the negative direction
/// that shares a sightline with the given point (x, y2).
fn find_neg_y_cutoff(
    bounding_boxes: ContainingSegmentIter<VerticalBoundingBox>,
    y1_coords: &[Fixed],
    y2: Fixed,
    offset: usize,
    ignore_box: Entity,
) -> usize {
    if y1_coords.is_empty() {
        return offset;
    }

    let center = y1_coords.len() / 2;
    let y1 = y1_coords[center];

    if points_have_vertical_sightline(bounding_boxes.clone(), y1, y2, ignore_box) {
        find_neg_y_cutoff(bounding_boxes, &y1_coords[..center], y2, offset, ignore_box)
    } else {
        find_neg_y_cutoff(
            bounding_boxes,
            &y1_coords[(center + 1)..],
            y2,
            offset + center + 1,
            ignore_box,
        )
    }
}

/// Finds the last (exclusive) y2 coordinate in the positive direction
/// that shares a sightline with the given point (x, y1).
fn find_pos_y_cutoff(
    bounding_boxes: ContainingSegmentIter<VerticalBoundingBox>,
    y1: Fixed,
    y2_coords: &[Fixed],
    offset: usize,
    ignore_box: Entity,
) -> usize {
    if y2_coords.is_empty() {
        return offset;
    }

    let center = y2_coords.len() / 2;
    let y2 = y2_coords[center];

    if points_have_vertical_sightline(bounding_boxes.clone(), y1, y2, ignore_box) {
        find_pos_y_cutoff(
            bounding_boxes,
            y1,
            &y2_coords[(center + 1)..],
            offset + center + 1,
            ignore_box,
        )
    } else {
        find_pos_y_cutoff(bounding_boxes, y1, &y2_coords[..center], offset, ignore_box)
    }
}

fn get_or_insert_node(
    node_map: &mut HashMap<Vec2, NodeIndex>,
    nodes: &mut NodeList,
    point: Vec2,
) -> (u32, bool) {
    use std::collections::hash_map::Entry;

    match node_map.entry(point) {
        Entry::Occupied(entry) => {
            let index = *entry.get();
            nodes[index].legal_directions = Directions::ALL;
            (index, true)
        }
        Entry::Vacant(entry) => {
            let index = nodes.push(point, false, Directions::ALL);
            entry.insert(index);
            (index, false)
        }
    }
}

struct ScanXData<'a> {
    node_map: &'a mut HashMap<Vec2, NodeIndex>,
    nodes: &'a mut NodeList,
    x_coords: &'a [Fixed],
    x_index: usize,
    bounding_boxes: ContainingSegmentIter<'a, HorizontalBoundingBox>,
    anchor: &'a Anchor,
    anchor_index: u32,
}

fn scan_neg_x(
    ScanXData {
        node_map,
        nodes,
        x_coords,
        x_index,
        bounding_boxes,
        anchor,
        anchor_index,
    }: ScanXData,
) {
    // Find how far in the negative X direction this anchor point has a sightline to.
    let neg_x_cutoff = find_neg_x_cutoff(
        bounding_boxes,
        &x_coords[..x_index],
        anchor.position.x,
        0,
        anchor.bounding_box,
    );

    // Create edges for all nodes between `neg_x_cutoff` and `x_index`.
    let mut prev_index = anchor_index;
    for x in x_coords[neg_x_cutoff..x_index].iter().copied().rev() {
        let current_point = Vec2 {
            x,
            y: anchor.position.y,
        };

        let (current_index, existed) = get_or_insert_node(node_map, nodes, current_point);

        nodes[prev_index].neighbors[Direction::NegX] = current_index;
        nodes[current_index].neighbors[Direction::PosX] = prev_index;

        if existed && (nodes[current_index].neighbors[Direction::NegX] != INVALID_NODE_INDEX) {
            break;
        }

        prev_index = current_index;
    }
}

fn scan_pos_x(
    ScanXData {
        node_map,
        nodes,
        x_coords,
        x_index,
        bounding_boxes,
        anchor,
        anchor_index,
    }: ScanXData,
) {
    // Find how far in the positive X direction this anchor point has a sightline to.
    let pos_x_cutoff = find_pos_x_cutoff(
        bounding_boxes,
        anchor.position.x,
        &x_coords[(x_index + 1)..],
        x_index + 1,
        anchor.bounding_box,
    );

    // Create edges for all nodes between `x_index` and `pos_x_cutoff`.
    let mut prev_index = anchor_index;
    for x in x_coords[(x_index + 1)..pos_x_cutoff].iter().copied() {
        let current_point = Vec2 {
            x,
            y: anchor.position.y,
        };

        let (current_index, existed) = get_or_insert_node(node_map, nodes, current_point);

        nodes[prev_index].neighbors[Direction::PosX] = current_index;
        nodes[current_index].neighbors[Direction::NegX] = prev_index;

        if existed && (nodes[current_index].neighbors[Direction::PosX] != INVALID_NODE_INDEX) {
            break;
        }

        prev_index = current_index;
    }
}

struct ScanYData<'a> {
    node_map: &'a mut HashMap<Vec2, NodeIndex>,
    nodes: &'a mut NodeList,
    y_coords: &'a [Fixed],
    y_index: usize,
    bounding_boxes: ContainingSegmentIter<'a, VerticalBoundingBox>,
    anchor: &'a Anchor,
    anchor_index: u32,
}

fn scan_neg_y(
    ScanYData {
        node_map,
        nodes,
        y_coords,
        y_index,
        bounding_boxes,
        anchor,
        anchor_index,
    }: ScanYData,
) {
    // Find how far in the negative Y direction this anchor point has a sightline to.
    let neg_y_cutoff = find_neg_y_cutoff(
        bounding_boxes,
        &y_coords[..y_index],
        anchor.position.y,
        0,
        anchor.bounding_box,
    );

    // Create edges for all nodes between `neg_y_cutoff` and `y_index`.
    let mut prev_index = anchor_index;
    for y in y_coords[neg_y_cutoff..y_index].iter().copied().rev() {
        let current_point = Vec2 {
            x: anchor.position.x,
            y,
        };

        let (current_index, existed) = get_or_insert_node(node_map, nodes, current_point);

        nodes[prev_index].neighbors[Direction::NegY] = current_index;
        nodes[current_index].neighbors[Direction::PosY] = prev_index;

        if existed && (nodes[current_index].neighbors[Direction::NegY] != INVALID_NODE_INDEX) {
            break;
        }

        prev_index = current_index;
    }
}

fn scan_pos_y(
    ScanYData {
        node_map,
        nodes,
        y_coords,
        y_index,
        bounding_boxes,
        anchor,
        anchor_index,
    }: ScanYData,
) {
    // Find how far in the positive Y direction this anchor point has a sightline to.
    let pos_y_cutoff = find_pos_y_cutoff(
        bounding_boxes,
        anchor.position.y,
        &y_coords[(y_index + 1)..],
        y_index + 1,
        anchor.bounding_box,
    );

    // Create edges for all nodes between `y_index` and `pos_y_cutoff`.
    let mut prev_index = anchor_index;
    for y in y_coords[(y_index + 1)..pos_y_cutoff].iter().copied() {
        let current_point = Vec2 {
            x: anchor.position.x,
            y,
        };

        let (current_index, existed) = get_or_insert_node(node_map, nodes, current_point);

        nodes[prev_index].neighbors[Direction::PosY] = current_index;
        nodes[current_index].neighbors[Direction::NegY] = prev_index;

        if existed && (nodes[current_index].neighbors[Direction::PosY] != INVALID_NODE_INDEX) {
            break;
        }

        prev_index = current_index;
    }
}

#[derive(Default, Debug)]
struct ThreadLocalData {
    explicit_anchors: Vec<Anchor>,
    implicit_anchors: Vec<Anchor>,
    x_coords: Vec<Fixed>,
    y_coords: Vec<Fixed>,
    horizontal_grid: BitGrid,
    vertical_grid: BitGrid,
    empty_ranges: Vec<EmptyRanges>,
}

fn generate_explicit_anchors(
    circuit_children: &RelationsItem<Child>,
    tree: &CircuitTree,
    bounding_boxes: &mut BoundingBoxList,
    explicit_anchors: &mut Vec<Anchor>,
) {
    explicit_anchors.clear();

    let (mut horizontal_builder, mut vertical_builder) = bounding_boxes.build();
    circuit_children
        .join::<Child>(&tree.symbols)
        .for_each(|((id, bb), symbol_children)| {
            symbol_children
                .join::<Child>(&tree.ports)
                .for_each(|(transform, directions)| {
                    let anchor = Anchor::new_port(transform.translation, **directions, id);
                    explicit_anchors.push(anchor);
                });

            horizontal_builder.push(Segment {
                start_inclusive: bb.min().y - BOUNDING_BOX_PADDING,
                end_inclusive: bb.max().y + BOUNDING_BOX_PADDING,
                value: HorizontalBoundingBox {
                    id,
                    min_x: bb.min().x - BOUNDING_BOX_PADDING,
                    max_x: bb.max().x + BOUNDING_BOX_PADDING,
                },
            });

            vertical_builder.push(Segment {
                start_inclusive: bb.min().x - BOUNDING_BOX_PADDING,
                end_inclusive: bb.max().x + BOUNDING_BOX_PADDING,
                value: VerticalBoundingBox {
                    id,
                    min_y: bb.min().y - BOUNDING_BOX_PADDING,
                    max_y: bb.max().y + BOUNDING_BOX_PADDING,
                },
            });
        });

    drop(horizontal_builder);
    drop(vertical_builder);

    circuit_children
        .join::<Child>(&tree.nets)
        .for_each(|(_, net_children)| {
            net_children.join::<Child>(&tree.endpoints).for_each(
                |(_, endpoint_transform, has_port)| {
                    if !has_port {
                        let anchor = Anchor::new(endpoint_transform.translation, Directions::ALL);
                        explicit_anchors.push(anchor);
                    }
                },
            );
        });
}

fn generate_implicit_anchors(
    circuit_children: &RelationsItem<Child>,
    symbols: &SymbolQuery,
    thread_local_data: &mut ThreadLocalData,
) {
    const PADDING: Vec2 = Vec2::splat(BOUNDING_BOX_PADDING);

    let ThreadLocalData {
        implicit_anchors,
        x_coords,
        y_coords,
        horizontal_grid,
        vertical_grid,
        empty_ranges,
        ..
    } = thread_local_data;

    implicit_anchors.clear();
    x_coords.clear();
    y_coords.clear();

    circuit_children
        .join::<Child>(symbols)
        .for_each(|((_, bb), _)| {
            for corner in bb.extrude(PADDING).corners() {
                x_coords.push(corner.x);
                y_coords.push(corner.y);
            }
        });

    x_coords.sort_unstable();
    x_coords.dedup();
    y_coords.sort_unstable();
    y_coords.dedup();

    let x_grid_cells = x_coords.len().saturating_sub(1) as u32;
    let y_grid_cells = y_coords.len().saturating_sub(1) as u32;
    horizontal_grid.reset(x_grid_cells, y_grid_cells);
    vertical_grid.reset(y_grid_cells, x_grid_cells);

    circuit_children
        .join::<Child>(symbols)
        .for_each(|((_, bb), _)| {
            let bb = bb.extrude(PADDING);

            let min_x_index = x_coords.binary_search(&bb.min().x).unwrap() as u32;
            let min_y_index = y_coords.binary_search(&bb.min().y).unwrap() as u32;
            let max_x_index = x_coords.binary_search(&bb.max().x).unwrap() as u32;
            let max_y_index = y_coords.binary_search(&bb.max().y).unwrap() as u32;

            horizontal_grid.fill_rect(min_x_index, min_y_index, max_x_index, max_y_index);
            vertical_grid.fill_rect(min_y_index, min_x_index, max_y_index, max_x_index);
        });

    empty_ranges.clear();
    for y in 0..y_grid_cells {
        empty_ranges.push(horizontal_grid.find_empty_ranges(y));
    }

    for i in 0..empty_ranges.len() {
        let empty_ranges = &mut empty_ranges[i..];
        let (current_ranges, following_ranges) = empty_ranges.split_first_mut().unwrap();

        for range in current_ranges.iter() {
            let mut height = 1;
            for following_ranges in following_ranges.iter_mut() {
                if let Some(following_index) = following_ranges
                    .iter()
                    .position(|following_range| following_range == range)
                {
                    height += 1;
                    following_ranges.swap_remove(following_index);
                } else {
                    break;
                }
            }

            let corridor_min_x = x_coords[range.start as usize] + Fixed::EPSILON;
            let corridor_max_x = x_coords[range.end as usize] - Fixed::EPSILON;
            let corridor_min_y = y_coords[i];
            let corridor_max_y = y_coords[i + height];
            let corridor_y = (corridor_min_y + corridor_max_y) * fixed!(0.5);

            implicit_anchors.push(Anchor::new(
                Vec2 {
                    x: corridor_min_x,
                    y: corridor_y,
                },
                Directions::X,
            ));
            implicit_anchors.push(Anchor::new(
                Vec2 {
                    x: corridor_max_x,
                    y: corridor_y,
                },
                Directions::X,
            ));
        }
    }

    empty_ranges.clear();
    for x in 0..x_grid_cells {
        empty_ranges.push(vertical_grid.find_empty_ranges(x));
    }

    for i in 0..empty_ranges.len() {
        let empty_ranges = &mut empty_ranges[i..];
        let (current_ranges, following_ranges) = empty_ranges.split_first_mut().unwrap();

        for range in current_ranges.iter() {
            let mut width = 1;
            for following_ranges in following_ranges.iter_mut() {
                if let Some(following_index) = following_ranges
                    .iter()
                    .position(|following_range| following_range == range)
                {
                    width += 1;
                    following_ranges.swap_remove(following_index);
                } else {
                    break;
                }
            }

            let corridor_min_y = y_coords[range.start as usize] + Fixed::EPSILON;
            let corridor_max_y = y_coords[range.end as usize] - Fixed::EPSILON;
            let corridor_min_x = x_coords[i];
            let corridor_max_x = x_coords[i + width];
            let corridor_x = (corridor_min_x + corridor_max_x) * fixed!(0.5);

            implicit_anchors.push(Anchor::new(
                Vec2 {
                    x: corridor_x,
                    y: corridor_min_y,
                },
                Directions::Y,
            ));
            implicit_anchors.push(Anchor::new(
                Vec2 {
                    x: corridor_x,
                    y: corridor_max_y,
                },
                Directions::Y,
            ));
        }
    }
}

#[derive(Default, Debug, Clone, Component)]
pub struct Graph {
    pub(crate) bounding_boxes: BoundingBoxList,
    node_map: HashMap<Vec2, NodeIndex>,
    pub(crate) nodes: NodeList,
}

impl Graph {
    fn scan(&mut self, anchor: &Anchor, anchor_index: u32, x_coords: &[Fixed], y_coords: &[Fixed]) {
        if anchor.connect_directions.intersects(Directions::X) {
            let x_index = x_coords
                .binary_search(&anchor.position.x)
                .expect("invalid anchor point");

            let bounding_boxes = self
                .bounding_boxes
                .iter_containing_horizontal(anchor.position.y);

            if anchor.connect_directions.contains(Directions::NEG_X) {
                scan_neg_x(ScanXData {
                    node_map: &mut self.node_map,
                    nodes: &mut self.nodes,
                    x_coords,
                    x_index,
                    bounding_boxes: bounding_boxes.clone(),
                    anchor,
                    anchor_index,
                });
            }

            if anchor.connect_directions.contains(Directions::POS_X) {
                scan_pos_x(ScanXData {
                    node_map: &mut self.node_map,
                    nodes: &mut self.nodes,
                    x_coords,
                    x_index,
                    bounding_boxes,
                    anchor,
                    anchor_index,
                });
            }
        }

        if anchor.connect_directions.intersects(Directions::Y) {
            let y_index = y_coords
                .binary_search(&anchor.position.y)
                .expect("invalid anchor point");

            let bounding_boxes = self
                .bounding_boxes
                .iter_containing_vertical(anchor.position.x);

            if anchor.connect_directions.contains(Directions::NEG_Y) {
                scan_neg_y(ScanYData {
                    node_map: &mut self.node_map,
                    nodes: &mut self.nodes,
                    y_coords,
                    y_index,
                    bounding_boxes: bounding_boxes.clone(),
                    anchor,
                    anchor_index,
                });
            }

            if anchor.connect_directions.contains(Directions::POS_Y) {
                scan_pos_y(ScanYData {
                    node_map: &mut self.node_map,
                    nodes: &mut self.nodes,
                    y_coords,
                    y_index,
                    bounding_boxes,
                    anchor,
                    anchor_index,
                });
            }
        }
    }

    #[tracing::instrument(skip_all)]
    fn remove_redundant_nodes(&mut self) {
        struct HeadTail<'a> {
            node_index: usize,
            head: &'a mut [Node],
            tail: &'a mut [Node],
        }

        impl<'a> HeadTail<'a> {
            #[inline]
            fn new(nodes: &'a mut [Node], node_index: usize) -> (&'a mut Node, Self) {
                let (head, tail) = nodes.split_at_mut(node_index);
                let (node, tail) = tail.split_first_mut().unwrap();

                (
                    node,
                    Self {
                        node_index,
                        head,
                        tail,
                    },
                )
            }

            #[inline]
            fn get_neighbor(&mut self, neighbor_index: NodeIndex) -> &mut Node {
                use std::cmp::Ordering;

                let neighbor_index = neighbor_index as usize;
                match neighbor_index.cmp(&self.node_index) {
                    Ordering::Less => &mut self.head[neighbor_index],
                    Ordering::Equal => unreachable!("node cannot be its own neighbor"),
                    Ordering::Greater => &mut self.tail[neighbor_index - self.node_index - 1],
                }
            }
        }

        let nodes = self.nodes.0.as_mut_slice();
        let mut nodes_len = nodes.len();

        // Remove all nodes that have a "straight through" connection (and aren't anchors).
        for node_index in (0..nodes_len).rev() {
            let (node, mut head_tail) = HeadTail::new(&mut nodes[..nodes_len], node_index);

            if !node.is_explicit {
                let neg_x_neighbor_index = node.neighbors[Direction::NegX];
                let pos_x_neighbor_index = node.neighbors[Direction::PosX];
                let neg_y_neighbor_index = node.neighbors[Direction::NegY];
                let pos_y_neighbor_index = node.neighbors[Direction::PosY];

                let remove = if (neg_x_neighbor_index != INVALID_NODE_INDEX)
                    && (pos_x_neighbor_index != INVALID_NODE_INDEX)
                    && (neg_y_neighbor_index == INVALID_NODE_INDEX)
                    && (pos_y_neighbor_index == INVALID_NODE_INDEX)
                {
                    let neg_x_neighbor = head_tail.get_neighbor(neg_x_neighbor_index);
                    neg_x_neighbor.neighbors[Direction::PosX] = pos_x_neighbor_index;

                    let pos_x_neighbor = head_tail.get_neighbor(pos_x_neighbor_index);
                    pos_x_neighbor.neighbors[Direction::NegX] = neg_x_neighbor_index;

                    true
                } else if (neg_x_neighbor_index == INVALID_NODE_INDEX)
                    && (pos_x_neighbor_index == INVALID_NODE_INDEX)
                    && (neg_y_neighbor_index != INVALID_NODE_INDEX)
                    && (pos_y_neighbor_index != INVALID_NODE_INDEX)
                {
                    let neg_y_neighbor = head_tail.get_neighbor(neg_y_neighbor_index);
                    neg_y_neighbor.neighbors[Direction::PosY] = pos_y_neighbor_index;

                    let pos_y_neighbor = head_tail.get_neighbor(pos_y_neighbor_index);
                    pos_y_neighbor.neighbors[Direction::NegY] = neg_y_neighbor_index;

                    true
                } else {
                    false
                };

                if remove {
                    nodes_len -= 1;
                    self.node_map
                        .remove(&node.position)
                        .expect("node not in map");

                    if let Some(last) = head_tail.tail.last_mut() {
                        std::mem::swap(node, last);
                        self.node_map
                            .insert(node.position, node_index as NodeIndex)
                            .expect("swapped node not in map");

                        for dir in Direction::ALL {
                            let neighbor_index = node.neighbors[dir];
                            if neighbor_index != INVALID_NODE_INDEX {
                                let neighbor = head_tail.get_neighbor(neighbor_index);
                                neighbor.neighbors[dir.opposite()] = node_index as NodeIndex;
                            }
                        }
                    }
                }
            }
        }

        self.nodes.0.truncate(nodes_len);
    }

    #[cfg(debug_assertions)]
    #[tracing::instrument(skip_all)]
    fn assert_graph_is_valid(&self) {
        for (node_index, node) in self.nodes.0.iter().enumerate() {
            for dir in Direction::ALL {
                if let Some(neighbor_index) = node.get_neighbor(dir) {
                    let neighbor = &self.nodes.0[neighbor_index];
                    assert_eq!(neighbor.get_neighbor(dir.opposite()), Some(node_index));

                    match dir {
                        Direction::PosX | Direction::NegX => {
                            assert_eq!(node.position.y, neighbor.position.y);
                        }
                        Direction::PosY | Direction::NegY => {
                            assert_eq!(node.position.x, neighbor.position.x);
                        }
                    }

                    match dir {
                        Direction::PosX => {
                            assert!(node.position.x < neighbor.position.x);
                        }
                        Direction::NegX => {
                            assert!(node.position.x > neighbor.position.x);
                        }
                        Direction::PosY => {
                            assert!(node.position.y < neighbor.position.y);
                        }
                        Direction::NegY => {
                            assert!(node.position.y > neighbor.position.y);
                        }
                    }
                }
            }
        }
    }

    #[cfg(not(debug_assertions))]
    fn assert_graph_is_valid(&self) {}

    /// Builds the graph.
    ///
    /// If the graph had previously been built, this will reset it and reuse the resources.
    #[tracing::instrument(skip_all, name = "build_graph")]
    pub(crate) fn build(
        &mut self,
        circuit_children: &RelationsItem<Child>,
        tree: &CircuitTree,
        prune: bool,
    ) {
        use std::collections::hash_map::Entry;

        thread_local! {
            static THREAD_LOCAL_DATA: RefCell<ThreadLocalData> = RefCell::default();
        }

        THREAD_LOCAL_DATA.with_borrow_mut(|thread_local_data| {
            generate_explicit_anchors(
                circuit_children,
                tree,
                &mut self.bounding_boxes,
                &mut thread_local_data.explicit_anchors,
            );
            generate_implicit_anchors(circuit_children, &tree.symbols, thread_local_data);

            let ThreadLocalData {
                explicit_anchors,
                implicit_anchors,
                x_coords,
                y_coords,
                ..
            } = thread_local_data;

            x_coords.clear();
            x_coords.extend(explicit_anchors.iter().map(|anchor| anchor.position.x));
            x_coords.extend(implicit_anchors.iter().map(|anchor| anchor.position.x));
            x_coords.sort_unstable();
            x_coords.dedup();

            y_coords.clear();
            y_coords.extend(explicit_anchors.iter().map(|anchor| anchor.position.y));
            y_coords.extend(implicit_anchors.iter().map(|anchor| anchor.position.y));
            y_coords.sort_unstable();
            y_coords.dedup();

            self.node_map.clear();
            self.nodes.clear();

            for anchor in explicit_anchors.iter() {
                match self.node_map.entry(anchor.position) {
                    Entry::Occupied(entry) => {
                        let index = *entry.get();
                        self.nodes[index].legal_directions |= anchor.connect_directions;
                    }
                    Entry::Vacant(entry) => {
                        let index =
                            self.nodes
                                .push(anchor.position, true, anchor.connect_directions);
                        entry.insert(index);
                    }
                }
            }

            for anchor in implicit_anchors.iter() {
                match self.node_map.entry(anchor.position) {
                    Entry::Occupied(entry) => {
                        let index = *entry.get();
                        self.nodes[index].legal_directions |= anchor.connect_directions;
                    }
                    Entry::Vacant(entry) => {
                        let index =
                            self.nodes
                                .push(anchor.position, false, anchor.connect_directions);
                        entry.insert(index);
                    }
                }
            }

            for anchor in explicit_anchors.iter() {
                let anchor_index = self.node_map[&anchor.position];
                self.scan(anchor, anchor_index, x_coords, y_coords);
            }

            for anchor in implicit_anchors.iter() {
                let anchor_index = self.node_map[&anchor.position];
                self.scan(anchor, anchor_index, x_coords, y_coords);
            }

            self.assert_graph_is_valid();

            if prune {
                self.remove_redundant_nodes();
                self.assert_graph_is_valid();
            }
        });
    }

    /// The nodes in the graph.
    #[inline]
    pub fn nodes(&self) -> &[Node] {
        &self.nodes.0
    }

    /// Finds the index of the node at the given position.
    #[inline]
    pub fn find_node(&self, position: Vec2) -> Option<NodeIndex> {
        self.node_map.get(&position).copied()
    }
}
