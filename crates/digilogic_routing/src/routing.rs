use crate::graph::Graph;
use crate::path_finding::*;
use crate::{EndpointQuery, Vertex, VertexKind};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_log::debug;
use digilogic_core::components::*;
use digilogic_core::fixed;
use digilogic_core::transform::*;
use smallvec::SmallVec;
use std::cell::RefCell;

#[derive(Default)]
struct ThreadLocalData {
    path_finder: PathFinder,
    ends: Vec<Vec2>,
}

fn pick_root_path(
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
) -> Option<(Entity, Entity)> {
    let mut max_dist = fixed!(0);
    let mut max_pair: Option<(Entity, Entity)> = None;

    net_children
        .join::<Child>(endpoints)
        .for_each(|(a, transform_a, _)| {
            let pos_a = transform_a.translation;

            net_children
                .join::<Child>(endpoints)
                .for_each(|(b, transform_b, _)| {
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

fn push_vertices(
    path: &Path,
    vertices: &mut Vec<Vertex>,
    ends: &mut Vec<Vec2>,
    is_root: bool,
    is_junction: bool,
) {
    ends.reserve(path.nodes().len());
    for node in path.nodes() {
        ends.push(node.position);
    }

    let mut first = true;
    let mut prev_node: Option<PathNode> = None;
    for (_, node) in path.iter_pruned() {
        if let Some(prev_node) = prev_node {
            vertices.push(Vertex {
                position: prev_node.position,
                kind: if first {
                    VertexKind::WireStart { is_root }
                } else {
                    VertexKind::Normal
                },
                connected_junctions: SmallVec::new(),
            });

            first = false;
        }

        prev_node = Some(node);
    }

    if let Some(prev_node) = prev_node {
        vertices.push(Vertex {
            position: prev_node.position,
            kind: VertexKind::WireEnd { is_junction },
            connected_junctions: SmallVec::new(),
        });
    }
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
        connected_junctions: SmallVec::new(),
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
            connected_junctions: SmallVec::new(),
        });
    }

    vertices.push(Vertex {
        position: end,
        kind: VertexKind::WireEnd { is_junction },
        connected_junctions: SmallVec::new(),
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
    thread_local_data: &mut ThreadLocalData,
) -> Result<(), RoutingError> {
    let (_, root_start_transform, _) = endpoints.get(root_start).unwrap();
    let (_, root_end_transform, _) = endpoints.get(root_end).unwrap();
    let root_start_pos = root_start_transform.translation;
    let root_end_pos = root_end_transform.translation;

    let ThreadLocalData {
        path_finder, ends, ..
    } = thread_local_data;

    match path_finder.find_path(graph, root_start_pos, root_end_pos) {
        PathFindResult::Found(path) => {
            push_vertices(&path, vertices, ends, true, false);
        }
        PathFindResult::NotFound => {
            debug!(
                "no path between ({}, {}) and ({}, {}) found, generating fallback wire",
                root_start_pos.x, root_start_pos.y, root_end_pos.x, root_end_pos.y
            );

            let root_end_node = &graph.nodes[graph.find_node(root_end_pos).unwrap()];
            push_fallback_vertices(
                root_end_pos,
                root_start_pos,
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

fn route_branch_wires(
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    roots: [Entity; 2],
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
    thread_local_data: &mut ThreadLocalData,
) -> Result<(), RoutingError> {
    let ThreadLocalData { path_finder, ends } = thread_local_data;

    let mut result = Ok(());

    net_children
        .join::<Child>(endpoints)
        .for_each(|(endpoint, endpoint_transform, _)| {
            if roots.contains(&endpoint) {
                return JCF::Continue;
            }

            let endpoint_pos = endpoint_transform.translation;
            match path_finder.find_path_multi(graph, endpoint_pos, ends) {
                PathFindResult::Found(path) => {
                    push_vertices(&path, vertices, ends, false, true);
                }
                PathFindResult::NotFound => {
                    debug!(
                        "no path between ({}, {}) and root net found, generating fallback wire",
                        endpoint_pos.x, endpoint_pos.y
                    );

                    ends.push(endpoint_pos);
                }
                PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
                    result = Err(RoutingError::InvalidPoint);
                    return JCF::Exit;
                }
            }

            JCF::Continue
        });

    result
}

pub(crate) fn connect_net(
    graph: &Graph,
    vertices: &mut Vec<Vertex>,
    net_children: &RelationsItem<Child>,
    endpoints: &EndpointQuery,
) -> Result<(), RoutingError> {
    thread_local! {
        static THREAD_LOCAL_DATA: RefCell<ThreadLocalData> = RefCell::default();
    }

    THREAD_LOCAL_DATA.with_borrow_mut(|thread_local_data| {
        let (root_start, root_end) =
            pick_root_path(net_children, endpoints).ok_or(RoutingError::NotEnoughEndpoints)?;

        vertices.clear();
        thread_local_data.ends.clear();

        route_root_wire(
            graph,
            vertices,
            root_start,
            root_end,
            endpoints,
            thread_local_data,
        )?;

        route_branch_wires(
            graph,
            vertices,
            [root_start, root_end],
            net_children,
            endpoints,
            thread_local_data,
        )?;

        Ok(())
    })
}
