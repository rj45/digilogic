use crate::graph::{Graph, NodeIndex, INVALID_NODE_INDEX};
use crate::path_finding::*;
use crate::{EndpointQuery, HashMap, Vertex, VertexKind, WaypointQuery};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::{fixed, Fixed};
use std::cell::RefCell;

#[derive(Default)]
struct ThreadLocalData {
    path_finder: PathFinder,
    ends: Vec<Vec2>,
    centering_candidates: Vec<CenteringCandidate>,
    junctions: JunctionMap,
}

fn pick_root_path(
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
) -> Option<(Entity, Entity)> {
    let mut max_dist = fixed!(0);
    let mut max_pair: Option<(Entity, Entity)> = None;

    net_children
        .join::<Child>(endpoints)
        .for_each(|((a, transform_a), _)| {
            let pos_a = transform_a.translation;

            net_children
                .join::<Child>(endpoints)
                .for_each(|((b, transform_b), _)| {
                    let pos_b = transform_b.translation;

                    let dist = pos_a.manhatten_distance_to(pos_b);
                    if dist >= max_dist {
                        max_dist = dist;
                        max_pair = Some((a, b));
                    }
                });
        });

    max_pair
}

pub(crate) struct CenteringCandidate {
    node_a: NodeIndex,
    node_b: NodeIndex,
    vertex_index: usize,
}

fn push_vertices(
    path: &Path,
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    ends: &mut Vec<Vec2>,
    centering_candidates: &mut Vec<CenteringCandidate>,
    is_root: bool,
    is_junction: bool,
) {
    ends.reserve(path.nodes().len());
    for node in path.nodes() {
        ends.push(node.position);
    }

    let mut first = true;
    let mut prev_prev_dir = None;
    let mut prev_node: Option<PathNode> = None;
    for (_, node) in path.iter_pruned() {
        if let Some(prev_node) = prev_node {
            if (prev_node.kind == PathNodeKind::Normal)
                && (node.kind == PathNodeKind::Normal)
                && (prev_prev_dir == Some(node.bend_direction))
                && (prev_node.bend_direction != node.bend_direction)
            {
                centering_candidates.push(CenteringCandidate {
                    node_a: graph
                        .find_node(prev_node.position)
                        .expect("invalid path node"),
                    node_b: graph.find_node(node.position).expect("invalid path node"),
                    vertex_index: vertices.len(),
                });
            }

            vertices.push(Vertex {
                position: prev_node.position,
                kind: if first {
                    VertexKind::WireStart { is_root }
                } else {
                    VertexKind::Normal
                },
            });

            first = false;
        }

        prev_prev_dir = prev_node.map(|prev_node| prev_node.bend_direction);
        prev_node = Some(node);
    }

    if let Some(prev_node) = prev_node {
        vertices.push(Vertex {
            position: prev_node.position,
            kind: VertexKind::WireEnd { is_junction },
        });
    }
}

fn find_fallback_junction(endpoint: Vec2, ends: &[Vec2]) -> Vec2 {
    let mut min_dist = endpoint.manhatten_distance_to(ends[0]);
    let mut min_end = ends[0];

    for &end in &ends[1..] {
        let dist = endpoint.manhatten_distance_to(end);
        if dist < min_dist {
            min_dist = dist;
            min_end = end;
        }
    }

    min_end
}

fn push_fallback_vertices(
    start: Vec2,
    end: Vec2,
    start_dirs: Directions,
    vertices: &mut Vec<Vertex>,
    is_root: bool,
    is_junction: bool,
) -> Direction {
    vertices.push(Vertex {
        position: start,
        kind: VertexKind::WireStart { is_root },
    });

    let (middle, dir) = if start_dirs.intersects(Directions::X) {
        let middle = Vec2 {
            x: end.x,
            y: start.y,
        };

        let dir = if end.y < start.y {
            Direction::NegY
        } else {
            Direction::PosY
        };

        (middle, dir)
    } else {
        let middle = Vec2 {
            x: end.x,
            y: start.y,
        };

        let dir = if end.x < start.x {
            Direction::NegX
        } else {
            Direction::PosX
        };

        (middle, dir)
    };

    if (middle != start) && (middle != end) {
        vertices.push(Vertex {
            position: middle,
            kind: VertexKind::Normal,
        });
    }

    vertices.push(Vertex {
        position: end,
        kind: VertexKind::WireEnd { is_junction },
    });

    dir
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RoutingError {
    NotEnoughEndpoints,
    InvalidPoint,
}

fn route_root_wire(
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    root_start: Entity,
    root_end: Entity,
    endpoints: &EndpointQuery,
    waypoints: &WaypointQuery,
    thread_local_data: &mut ThreadLocalData,
) -> Result<(), RoutingError> {
    let ((_, root_start_transform), _) = endpoints.get(root_start).unwrap();
    let ((_, root_end_transform), _) = endpoints.get(root_end).unwrap();
    let root_start_pos = root_start_transform.translation;
    let root_end_pos = root_end_transform.translation;

    let ThreadLocalData {
        path_finder,
        ends,
        centering_candidates,
        ..
    } = thread_local_data;

    let (last_waypoint, last_waypoint_dir) = match path_finder.find_path_waypoints(
        graph,
        root_start_pos,
        &[root_start, root_end],
        endpoints,
        waypoints,
    ) {
        PathFindResult::Found(path) => {
            if path.nodes().len() < 2 {
                (root_start_pos, None)
            } else {
                push_vertices(
                    &path,
                    graph,
                    vertices,
                    ends,
                    centering_candidates,
                    true,
                    false,
                );
                let (last, head) = path.nodes().split_last().unwrap();
                let prev_last = head.last().unwrap();
                (last.position, prev_last.bend_direction)
            }
        }
        PathFindResult::NotFound => (root_start_pos, None),
        PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
            return Err(RoutingError::InvalidPoint);
        }
    };

    match path_finder.find_path(graph, last_waypoint, last_waypoint_dir, root_end_pos) {
        PathFindResult::Found(path) => {
            push_vertices(
                &path,
                graph,
                vertices,
                ends,
                centering_candidates,
                true,
                false,
            );
        }
        PathFindResult::NotFound => {
            println!(
                "no path between ({}, {}) and ({}, {}) found, generating fallback wire",
                last_waypoint.x, last_waypoint.y, root_end_pos.x, root_end_pos.y
            );

            let root_end_node = &graph.nodes[graph.find_node(root_end_pos).unwrap()];
            push_fallback_vertices(
                root_end_pos,
                last_waypoint,
                root_end_node.legal_directions,
                vertices,
                true,
                false,
            );

            ends.push(root_end_pos);
        }
        PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
            return Err(RoutingError::InvalidPoint);
        }
    }

    Ok(())
}

#[derive(Debug, Clone)]
pub(crate) enum JunctionKind {
    // A junction already has 2 connections from the root, so at most 2 other connections can be made.
    Single {
        vertex_index: usize,
        inbound_dir: Direction,
    },
    Double {
        vertex_index: [usize; 2],
        inbound_dir: [Direction; 2],
    },
    /// The junction is in a state the centering algorithm cannot deal with, so ignore it.
    Degenerate,
}

impl JunctionKind {
    fn iter(&self) -> impl Iterator<Item = (usize, Direction)> {
        enum Iter {
            Single(Option<(usize, Direction)>),
            Double([Option<(usize, Direction)>; 2]),
            Degenerate,
        }

        impl Iterator for Iter {
            type Item = (usize, Direction);

            fn next(&mut self) -> Option<Self::Item> {
                match self {
                    Iter::Single(opt) => opt.take(),
                    Iter::Double([opt1, opt2]) => opt1.take().or_else(|| opt2.take()),
                    Iter::Degenerate => None,
                }
            }
        }

        match self {
            &JunctionKind::Single {
                vertex_index,
                inbound_dir,
            } => Iter::Single(Some((vertex_index, inbound_dir))),
            JunctionKind::Double {
                vertex_index,
                inbound_dir,
            } => Iter::Double([
                Some((vertex_index[0], inbound_dir[0])),
                Some((vertex_index[1], inbound_dir[1])),
            ]),
            JunctionKind::Degenerate => Iter::Degenerate,
        }
    }
}

pub(crate) type JunctionMap = HashMap<Vec2, JunctionKind>;

fn insert_junction(
    junctions: &mut JunctionMap,
    position: Vec2,
    vertex_index: usize,
    inbound_dir: Direction,
) {
    use std::collections::hash_map::Entry;

    match junctions.entry(position) {
        Entry::Vacant(entry) => {
            entry.insert(JunctionKind::Single {
                vertex_index,
                inbound_dir,
            });
        }
        Entry::Occupied(mut entry) => {
            let kind = entry.get_mut();

            match *kind {
                JunctionKind::Single {
                    vertex_index: prev_vertex_index,
                    inbound_dir: prev_inbound_dir,
                } => {
                    *kind = JunctionKind::Double {
                        vertex_index: [prev_vertex_index, vertex_index],
                        inbound_dir: [prev_inbound_dir, inbound_dir],
                    };
                }
                JunctionKind::Double { .. } => {
                    // With normal routing this is impossible, because it requires routing
                    // on top of an existing wire that should have been connected to instead.
                    // However if a wire cannot be routed it ignores geometry so there is
                    // a small chance for it to happen.
                    *kind = JunctionKind::Degenerate;
                }
                JunctionKind::Degenerate => (),
            }
        }
    }
}

fn route_branch_wires(
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    roots: [Entity; 2],
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
    waypoints: &WaypointQuery,
    thread_local_data: &mut ThreadLocalData,
) -> Result<(), RoutingError> {
    let ThreadLocalData {
        path_finder,
        ends,
        centering_candidates,
        junctions,
    } = thread_local_data;

    let mut result = Ok(());

    net_children
        .join::<Child>(endpoints)
        .for_each(|((endpoint, endpoint_transform), _)| {
            if roots.contains(&endpoint) {
                return JCF::Continue;
            }

            let endpoint_pos = endpoint_transform.translation;
            let end_count = ends.len();

            let (last_waypoint, last_waypoint_dir) = match path_finder.find_path_waypoints(
                graph,
                endpoint_pos,
                &[endpoint],
                endpoints,
                waypoints,
            ) {
                PathFindResult::Found(path) => {
                    if path.nodes().len() < 2 {
                        (endpoint_pos, None)
                    } else {
                        push_vertices(
                            &path,
                            graph,
                            vertices,
                            ends,
                            centering_candidates,
                            false,
                            false,
                        );
                        let (last, head) = path.nodes().split_last().unwrap();
                        let prev_last = head.last().unwrap();
                        (last.position, prev_last.bend_direction)
                    }
                }
                PathFindResult::NotFound => (endpoint_pos, None),
                PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
                    result = Err(RoutingError::InvalidPoint);
                    return JCF::Exit;
                }
            };

            match path_finder.find_path_multi(
                graph,
                last_waypoint,
                last_waypoint_dir,
                &ends[..end_count],
            ) {
                PathFindResult::Found(path) => {
                    if path.nodes().len() < 2 {
                        // The final wire segment is degenerate, therefore the previous one actually ends in a junction.
                        if last_waypoint_dir.is_some() {
                            let Some(Vertex {
                                kind: VertexKind::WireEnd { is_junction },
                                ..
                            }) = vertices.last_mut()
                            else {
                                panic!("invalid last vertex");
                            };
                            *is_junction = true;
                        }

                        return JCF::Continue;
                    }

                    push_vertices(
                        &path,
                        graph,
                        vertices,
                        ends,
                        centering_candidates,
                        false,
                        true,
                    );
                    let (last, head) = path.nodes().split_last().unwrap();
                    let prev_last = head.last().unwrap();
                    insert_junction(
                        junctions,
                        last.position,
                        vertices.len(),
                        prev_last.bend_direction.unwrap(),
                    );
                }
                PathFindResult::NotFound => {
                    println!(
                        "no path between ({}, {}) and root net found, generating fallback wire",
                        last_waypoint.x, last_waypoint.y
                    );

                    let junction_pos = find_fallback_junction(endpoint_pos, ends);
                    let endpoint_node = &graph.nodes[graph.find_node(endpoint_pos).unwrap()];
                    let junction_dir = push_fallback_vertices(
                        endpoint_pos,
                        junction_pos,
                        endpoint_node.legal_directions,
                        vertices,
                        false,
                        true,
                    );

                    insert_junction(junctions, junction_pos, vertices.len(), junction_dir);
                    ends.push(endpoint_pos);
                }
                PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
                    result = Err(RoutingError::InvalidPoint);
                    return JCF::Exit;
                }
            }

            JCF::Continue
        });

    Ok(())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ConnectionKind {
    Connected {
        through_anchor: bool,
        through_junction: bool,
    },
    Unconnected,
}

fn are_connected_vertically(
    graph: &Graph,
    mut a: NodeIndex,
    b: NodeIndex,
    junctions: &JunctionMap,
) -> ConnectionKind {
    let node_a = &graph.nodes[a];
    let node_b = &graph.nodes[b];

    let dir = if node_a.position.y < node_b.position.y {
        Direction::PosY
    } else {
        assert!(node_a.position.y > node_b.position.y);
        Direction::NegY
    };

    let mut through_anchor = node_a.is_explicit || node_b.is_explicit;
    let mut through_junction =
        junctions.contains_key(&node_a.position) || junctions.contains_key(&node_b.position);

    a = node_a.neighbors[dir];
    while a != INVALID_NODE_INDEX {
        if a == b {
            return ConnectionKind::Connected {
                through_anchor,
                through_junction,
            };
        }

        let node = &graph.nodes[a];
        if node.is_explicit {
            through_anchor = true;
        }
        match junctions.get(&node.position) {
            Some(JunctionKind::Degenerate) => {
                through_anchor = true;
                through_junction = true;
            }
            Some(_) => through_junction = true,
            _ => (),
        }

        a = node.neighbors[dir];
    }

    ConnectionKind::Unconnected
}

fn are_connected_horizontally(
    graph: &Graph,
    mut a: NodeIndex,
    b: NodeIndex,
    junctions: &JunctionMap,
) -> ConnectionKind {
    let node_a = &graph.nodes[a];
    let node_b = &graph.nodes[b];

    let dir = if node_a.position.x < node_b.position.x {
        Direction::PosX
    } else {
        assert!(node_a.position.x > node_b.position.x);
        Direction::NegX
    };

    let mut through_anchor = node_a.is_explicit || node_b.is_explicit;
    let mut through_junction =
        junctions.contains_key(&node_a.position) || junctions.contains_key(&node_b.position);

    a = node_a.neighbors[dir];
    while a != INVALID_NODE_INDEX {
        if a == b {
            return ConnectionKind::Connected {
                through_anchor,
                through_junction,
            };
        }

        let node = &graph.nodes[a];
        if node.is_explicit {
            through_anchor = true;
        }
        match junctions.get(&node.position) {
            Some(JunctionKind::Degenerate) => {
                through_anchor = true;
                through_junction = true;
            }
            Some(_) => through_junction = true,
            _ => (),
        }

        a = node.neighbors[dir];
    }

    ConnectionKind::Unconnected
}

#[derive(Debug, Clone, Copy)]
enum NudgeOffset {
    None,
    Horizontal(Fixed),
    Vertical(Fixed),
}

fn center_in_alley(
    graph: &Graph,
    node_a_index: NodeIndex,
    node_b_index: NodeIndex,
    vertex_index: usize,
    vertices: &mut [Vertex],
    junctions: &JunctionMap,
) -> NudgeOffset {
    let node_a = &graph.nodes[node_a_index];
    let node_b = &graph.nodes[node_b_index];

    if node_a.position.x == node_b.position.x {
        match are_connected_vertically(graph, node_a_index, node_b_index, junctions) {
            ConnectionKind::Connected {
                through_junction, ..
            } => {
                if through_junction {
                    return NudgeOffset::None;
                }
            }
            ConnectionKind::Unconnected => panic!("wire segment not connected in graph"),
        }

        let mut min_x = node_a.position.x;
        let mut max_x = node_a.position.x;
        let mut x_min_cap = None;
        let mut x_max_cap = None;

        let mut current_node_a = node_a;
        let mut current_node_b = node_b;

        loop {
            let next_a_index = current_node_a.neighbors[Direction::NegX];
            let next_b_index = current_node_b.neighbors[Direction::NegX];

            if (next_a_index == INVALID_NODE_INDEX) || (next_b_index == INVALID_NODE_INDEX) {
                break;
            }

            current_node_a = &graph.nodes[next_a_index];
            current_node_b = &graph.nodes[next_b_index];

            if current_node_a.position.x != current_node_b.position.x {
                break;
            }

            match are_connected_vertically(graph, next_a_index, next_b_index, junctions) {
                ConnectionKind::Connected {
                    through_anchor,
                    through_junction,
                } => {
                    min_x = current_node_a.position.x;

                    if through_junction && x_min_cap.is_none() {
                        x_min_cap = Some(current_node_a.position.x);
                    }

                    if through_anchor {
                        break;
                    } else {
                        continue;
                    }
                }
                ConnectionKind::Unconnected => break,
            }
        }

        current_node_a = node_a;
        current_node_b = node_b;

        loop {
            let next_a_index = current_node_a.neighbors[Direction::PosX];
            let next_b_index = current_node_b.neighbors[Direction::PosX];

            if (next_a_index == INVALID_NODE_INDEX) || (next_b_index == INVALID_NODE_INDEX) {
                break;
            }

            current_node_a = &graph.nodes[next_a_index];
            current_node_b = &graph.nodes[next_b_index];

            if current_node_a.position.x != current_node_b.position.x {
                break;
            }

            match are_connected_vertically(graph, next_a_index, next_b_index, junctions) {
                ConnectionKind::Connected {
                    through_anchor,
                    through_junction,
                } => {
                    max_x = current_node_a.position.x;

                    if through_junction && x_max_cap.is_none() {
                        x_max_cap = Some(current_node_a.position.x);
                    }

                    if through_anchor {
                        break;
                    } else {
                        continue;
                    }
                }
                ConnectionKind::Unconnected => break,
            }
        }

        let vertex_start = vertex_index;
        let vertex_end = vertex_start + 1;
        let [vertex_a, vertex_b] = &mut vertices[vertex_start..=vertex_end] else {
            panic!("invalid vertex offset");
        };

        let center_x = ((min_x + max_x) / fixed!(2))
            .clamp(x_min_cap.unwrap_or(min_x), x_max_cap.unwrap_or(max_x));
        vertex_a.position.x = center_x;
        vertex_b.position.x = center_x;

        NudgeOffset::Horizontal(center_x - node_a.position.x)
    } else {
        assert_eq!(node_a.position.y, node_b.position.y);

        match are_connected_horizontally(graph, node_a_index, node_b_index, junctions) {
            ConnectionKind::Connected {
                through_junction, ..
            } => {
                if through_junction {
                    return NudgeOffset::None;
                }
            }
            ConnectionKind::Unconnected => panic!("wire segment not connected in graph"),
        }

        let mut min_y = node_a.position.y;
        let mut max_y = node_a.position.y;
        let mut y_min_cap = None;
        let mut y_max_cap = None;

        let mut current_node_a = node_a;
        let mut current_node_b = node_b;

        loop {
            let next_a_index = current_node_a.neighbors[Direction::NegY];
            let next_b_index = current_node_b.neighbors[Direction::NegY];

            if (next_a_index == INVALID_NODE_INDEX) || (next_b_index == INVALID_NODE_INDEX) {
                break;
            }

            current_node_a = &graph.nodes[next_a_index];
            current_node_b = &graph.nodes[next_b_index];

            if current_node_a.position.y != current_node_b.position.y {
                break;
            }

            match are_connected_horizontally(graph, next_a_index, next_b_index, junctions) {
                ConnectionKind::Connected {
                    through_anchor,
                    through_junction,
                } => {
                    min_y = current_node_a.position.y;

                    if through_junction && y_min_cap.is_none() {
                        y_min_cap = Some(current_node_a.position.y);
                    }

                    if through_anchor {
                        break;
                    } else {
                        continue;
                    }
                }
                ConnectionKind::Unconnected => break,
            }
        }

        current_node_a = node_a;
        current_node_b = node_b;

        loop {
            let next_a_index = current_node_a.neighbors[Direction::PosY];
            let next_b_index = current_node_b.neighbors[Direction::PosY];

            if (next_a_index == INVALID_NODE_INDEX) || (next_b_index == INVALID_NODE_INDEX) {
                break;
            }

            current_node_a = &graph.nodes[next_a_index];
            current_node_b = &graph.nodes[next_b_index];

            if current_node_a.position.y != current_node_b.position.y {
                break;
            }

            match are_connected_horizontally(graph, next_a_index, next_b_index, junctions) {
                ConnectionKind::Connected {
                    through_anchor,
                    through_junction,
                } => {
                    max_y = current_node_a.position.y;

                    if through_junction && y_max_cap.is_none() {
                        y_max_cap = Some(current_node_a.position.y);
                    }

                    if through_anchor {
                        break;
                    } else {
                        continue;
                    }
                }
                ConnectionKind::Unconnected => break,
            }
        }

        let vertex_start = vertex_index;
        let vertex_end = vertex_start + 1;
        let [vertex_a, vertex_b] = &mut vertices[vertex_start..=vertex_end] else {
            panic!("invalid vertex offset");
        };

        let center_y = ((min_y + max_y) / fixed!(2))
            .clamp(y_min_cap.unwrap_or(min_y), y_max_cap.unwrap_or(max_y));
        vertex_a.position.y = center_y;
        vertex_b.position.y = center_y;

        NudgeOffset::Vertical(center_y - node_a.position.y)
    }
}

fn center_wires(graph: &Graph, vertices: &mut [Vertex], thread_local_data: &ThreadLocalData) {
    let ThreadLocalData {
        centering_candidates,
        junctions,
        ..
    } = thread_local_data;

    for centering_candidate in centering_candidates {
        let offset = center_in_alley(
            graph,
            centering_candidate.node_a,
            centering_candidate.node_b,
            centering_candidate.vertex_index,
            vertices,
            junctions,
        );

        if matches!(offset, NudgeOffset::None) {
            continue;
        }

        let node_a = &graph.nodes[centering_candidate.node_a];
        let node_b = &graph.nodes[centering_candidate.node_b];

        for (&junction_point, junction_kind) in junctions {
            for (junction_vertex, junction_dir) in junction_kind.iter() {
                match offset {
                    NudgeOffset::None => unreachable!(),
                    NudgeOffset::Horizontal(offset) => {
                        assert_eq!(node_a.position.x, node_b.position.x);

                        let min_y = node_a.position.y.min(node_b.position.y);
                        let max_y = node_a.position.y.max(node_b.position.y);

                        if (junction_point.x == node_a.position.x)
                            && matches!(junction_dir, Direction::NegX | Direction::PosX)
                            && (junction_point.y >= min_y)
                            && (junction_point.y <= max_y)
                        {
                            vertices[junction_vertex].position.x += offset;
                        }

                        if junction_point.x == (node_a.position.x + offset) {
                            match junction_dir {
                                Direction::PosY => {
                                    if junction_point.y == max_y {
                                        vertices[junction_vertex].position.y = min_y;
                                    }
                                }
                                Direction::NegY => {
                                    if junction_point.y == min_y {
                                        vertices[junction_vertex].position.y = max_y;
                                    }
                                }
                                _ => (),
                            }
                        }
                    }
                    NudgeOffset::Vertical(offset) => {
                        assert_eq!(node_a.position.y, node_b.position.y);

                        let min_x = node_a.position.x.min(node_b.position.x);
                        let max_x = node_a.position.x.max(node_b.position.x);

                        if (junction_point.y == node_a.position.y)
                            && matches!(junction_dir, Direction::NegY | Direction::PosY)
                            && (junction_point.x >= min_x)
                            && (junction_point.x <= max_x)
                        {
                            vertices[junction_vertex].position.y += offset;
                        }

                        if junction_point.y == (node_a.position.y + offset) {
                            match junction_dir {
                                Direction::PosX => {
                                    if junction_point.x == max_x {
                                        vertices[junction_vertex].position.x = min_x;
                                    }
                                }
                                Direction::NegX => {
                                    if junction_point.x == min_x {
                                        vertices[junction_vertex].position.x = max_x;
                                    }
                                }
                                _ => (),
                            }
                        }
                    }
                }
            }
        }
    }
}

pub(crate) fn connect_net(
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
    waypoints: &WaypointQuery,
    center_wires: bool,
) -> Result<(), RoutingError> {
    thread_local! {
        static THREAD_LOCAL_DATA: RefCell<ThreadLocalData> = RefCell::default();
    }

    THREAD_LOCAL_DATA.with_borrow_mut(|thread_local_data| {
        let (root_start, root_end) =
            pick_root_path(net_children, endpoints).ok_or(RoutingError::NotEnoughEndpoints)?;

        vertices.clear();
        thread_local_data.ends.clear();
        thread_local_data.centering_candidates.clear();
        thread_local_data.junctions.clear();

        route_root_wire(
            graph,
            vertices,
            root_start,
            root_end,
            endpoints,
            waypoints,
            thread_local_data,
        )?;

        route_branch_wires(
            graph,
            vertices,
            [root_start, root_end],
            net_children,
            endpoints,
            waypoints,
            thread_local_data,
        )?;

        if center_wires {
            self::center_wires(graph, vertices, thread_local_data);
        }

        Ok(())
    })
}
