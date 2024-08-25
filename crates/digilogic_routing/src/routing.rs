use crate::graph::Graph;
use crate::path_finding::*;
use crate::{EndpointQuery, Junction, JunctionKind, Vertex, VertexKind};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_log::debug;
use digilogic_core::components::*;
use digilogic_core::fixed;
use digilogic_core::transform::*;
use smallvec::SmallVec;
use std::cell::RefCell;

#[derive(Debug)]
struct PathFindingEnd {
    position: Vec2,
    vertex_index: u32,
    junction_kind: JunctionKind,
}

#[derive(Default)]
struct ThreadLocalData {
    path_finder: PathFinder,
    ends: Vec<PathFindingEnd>,
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
    ends: &mut Vec<PathFindingEnd>,
    is_root: bool,
    junction_kind: Option<JunctionKind>,
) -> u32 {
    let mut first = true;
    let mut prev_node: Option<(usize, PathNode)> = None;
    for (node_index, node) in path.iter_pruned() {
        if let Some((prev_node_index, prev_node)) = prev_node {
            let prev_vertex_index = vertices.len() as u32;
            vertices.push(Vertex {
                position: prev_node.position,
                kind: if first {
                    VertexKind::WireStart { is_root }
                } else {
                    VertexKind::Normal
                },
                connected_junctions: SmallVec::new(),
            });

            ends.push(PathFindingEnd {
                position: path.nodes()[prev_node_index].position,
                vertex_index: prev_vertex_index,
                junction_kind: JunctionKind::Corner,
            });
            for i in (prev_node_index + 1)..node_index {
                ends.push(PathFindingEnd {
                    position: path.nodes()[i].position,
                    vertex_index: prev_vertex_index,
                    junction_kind: JunctionKind::LineSegment,
                });
            }

            first = false;
        }

        prev_node = Some((node_index, node));
    }

    let (last_node_index, last_node) = prev_node.unwrap();
    let last_vertex_index = vertices.len() as u32;
    vertices.push(Vertex {
        position: last_node.position,
        kind: VertexKind::WireEnd { junction_kind },
        connected_junctions: SmallVec::new(),
    });

    ends.push(PathFindingEnd {
        position: path.nodes()[last_node_index].position,
        vertex_index: last_vertex_index,
        junction_kind: JunctionKind::Corner,
    });

    last_vertex_index
}

fn push_fallback_vertices(
    start: Vec2,
    end: Vec2,
    start_dirs: Directions,
    vertices: &mut Vec<Vertex>,
    ends: &mut Vec<PathFindingEnd>,
    is_root: bool,
    junction_kind: Option<JunctionKind>,
) -> u32 {
    let start_vertex_index = vertices.len() as u32;
    vertices.push(Vertex {
        position: start,
        kind: VertexKind::WireStart { is_root },
        connected_junctions: SmallVec::new(),
    });

    let middle = if start_dirs.intersects(Directions::X) {
        Vec2 {
            x: end.x,
            y: start.y,
        }
    } else {
        Vec2 {
            x: start.x,
            y: end.y,
        }
    };

    if (middle != start) && (middle != end) {
        vertices.push(Vertex {
            position: middle,
            kind: VertexKind::Normal,
            connected_junctions: SmallVec::new(),
        });
    }

    let end_vertex_index = vertices.len() as u32;
    vertices.push(Vertex {
        position: end,
        kind: VertexKind::WireEnd { junction_kind },
        connected_junctions: SmallVec::new(),
    });

    ends.push(PathFindingEnd {
        position: start,
        vertex_index: start_vertex_index,
        junction_kind: JunctionKind::Corner,
    });
    ends.push(PathFindingEnd {
        position: end,
        vertex_index: end_vertex_index,
        junction_kind: JunctionKind::Corner,
    });

    end_vertex_index
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
            push_vertices(&path, vertices, ends, true, None);
        }
        PathFindResult::NotFound => {
            debug!(
                "no path between ({}, {}) and ({}, {}) found, generating fallback wire",
                root_start_pos.x, root_start_pos.y, root_end_pos.x, root_end_pos.y
            );

            let root_start_node = &graph.nodes[graph.find_node(root_start_pos).unwrap()];
            push_fallback_vertices(
                root_start_pos,
                root_end_pos,
                root_start_node.legal_directions,
                vertices,
                ends,
                true,
                None,
            );
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
            let (last_vertex_index, junction_kind, junction_vertex_index) = match path_finder
                .find_path_multi(graph, endpoint_pos, ends.iter().map(|end| end.position))
            {
                PathFindResult::Found(path) => {
                    let junction_end = ends
                        .iter()
                        .find(|end| end.position == path.nodes().last().unwrap().position)
                        .unwrap();

                    let junction_kind = junction_end.junction_kind;
                    let junction_vertex_index = junction_end.vertex_index;

                    let last_vertex_index =
                        push_vertices(&path, vertices, ends, false, Some(junction_kind));

                    (last_vertex_index, junction_kind, junction_vertex_index)
                }
                PathFindResult::NotFound => {
                    debug!(
                        "no path between ({}, {}) and root net found, generating fallback wire",
                        endpoint_pos.x, endpoint_pos.y
                    );

                    let start_node = &graph.nodes[graph.find_node(endpoint_pos).unwrap()];
                    let junction_end = ends
                        .iter()
                        .min_by_key(|end| end.position.manhatten_distance_to(endpoint_pos))
                        .unwrap();

                    let junction_kind = junction_end.junction_kind;
                    let junction_vertex_index = junction_end.vertex_index;

                    let last_vertex_index = push_fallback_vertices(
                        endpoint_pos,
                        junction_end.position,
                        start_node.legal_directions,
                        vertices,
                        ends,
                        true,
                        Some(junction_kind),
                    );

                    (last_vertex_index, junction_kind, junction_vertex_index)
                }
                PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
                    result = Err(RoutingError::InvalidPoint);
                    return JCF::Exit;
                }
            };

            vertices[junction_vertex_index as usize]
                .connected_junctions
                .push(Junction {
                    vertex_index: last_vertex_index,
                    kind: junction_kind,
                });

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
