use crate::graph::Graph;
use crate::path_finding::*;
use crate::{EndpointQuery, Junction, JunctionKind, Vertex, VertexKind, MIN_WIRE_SPACING};
use aery::operations::utils::RelationsItem;
use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_log::debug;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::{fixed, Fixed};
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
    let mut max_dist = Fixed::MIN;
    let mut max_pair: Option<(Entity, Entity)> = None;

    net_children
        .join::<Child>(endpoints)
        .for_each(|(a, transform_a, _)| {
            let pos_a = transform_a.translation;

            net_children
                .join::<Child>(endpoints)
                .for_each(|(b, transform_b, _)| {
                    if a != b {
                        let pos_b = transform_b.translation;

                        let dist_x = (pos_a.x - pos_b.x).abs();
                        let dist_y = (pos_a.y - pos_b.y).abs();
                        let dist = (dist_x - dist_y).abs();
                        if dist > max_dist {
                            max_dist = dist;
                            max_pair = Some((a, b));
                        }
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
) {
    let mut path_nodes = path.iter_pruned().peekable();

    {
        let (first_node_index, first_node) = path_nodes.next().expect("path too short");
        let &(next_node_index, _) = path_nodes.peek().expect("path too short");

        let vertex_index = vertices.len() as u32;
        ends.push(PathFindingEnd {
            position: first_node.position,
            vertex_index,
            junction_kind: JunctionKind::Corner,
        });

        let vertex_index = (vertices.len() + 2) as u32;
        ends.push(PathFindingEnd {
            position: first_node.position,
            vertex_index,
            junction_kind: JunctionKind::Corner,
        });

        for i in (first_node_index + 1)..next_node_index {
            ends.push(PathFindingEnd {
                position: path.nodes()[i].position,
                vertex_index,
                junction_kind: JunctionKind::LineSegment,
            });
        }

        vertices.push(Vertex {
            position: first_node.position,
            kind: VertexKind::WireStart { is_root },
            connected_junctions: SmallVec::new(),
        });

        const DUMMY_MAX_DIST: Vec2 = Vec2::splat(MIN_WIRE_SPACING);
        let dummy_dist =
            (path.nodes()[first_node_index + 1].position - first_node.position) * fixed!(0.5);
        let dummy_pos = first_node.position + dummy_dist.clamp(-DUMMY_MAX_DIST, DUMMY_MAX_DIST);
        vertices.push(Vertex {
            position: dummy_pos,
            kind: VertexKind::Dummy,
            connected_junctions: SmallVec::new(),
        });
        vertices.push(Vertex {
            position: dummy_pos,
            kind: VertexKind::Dummy,
            connected_junctions: SmallVec::new(),
        });
    }

    while let Some((node_index, node)) = path_nodes.next() {
        let vertex_index = vertices.len() as u32;

        ends.push(PathFindingEnd {
            position: node.position,
            vertex_index,
            junction_kind: JunctionKind::Corner,
        });

        let kind = if let Some(&(next_node_index, _)) = path_nodes.peek() {
            for i in (node_index + 1)..next_node_index {
                ends.push(PathFindingEnd {
                    position: path.nodes()[i].position,
                    vertex_index,
                    junction_kind: JunctionKind::LineSegment,
                });
            }

            VertexKind::Normal
        } else {
            VertexKind::WireEnd { junction_kind }
        };

        vertices.push(Vertex {
            position: node.position,
            kind,
            connected_junctions: SmallVec::new(),
        });
    }
}

fn push_fallback_vertices(
    start: Vec2,
    end: Vec2,
    start_dirs: Directions,
    vertices: &mut Vec<Vertex>,
    ends: &mut Vec<PathFindingEnd>,
    is_root: bool,
    junction_kind: Option<JunctionKind>,
) {
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
            let (junction_kind, junction_vertex_index) = match path_finder.find_path_multi(
                graph,
                endpoint_pos,
                ends.iter().map(|end| end.position),
            ) {
                PathFindResult::Found(path) => {
                    let junction_end = ends
                        .iter()
                        .find(|end| end.position == path.nodes().last().unwrap().position)
                        .unwrap();

                    let junction_kind = junction_end.junction_kind;
                    let junction_vertex_index = junction_end.vertex_index;

                    push_vertices(&path, vertices, ends, false, Some(junction_kind));

                    (junction_kind, junction_vertex_index)
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

                    push_fallback_vertices(
                        endpoint_pos,
                        junction_end.position,
                        start_node.legal_directions,
                        vertices,
                        ends,
                        true,
                        Some(junction_kind),
                    );

                    (junction_kind, junction_vertex_index)
                }
                PathFindResult::InvalidStartPoint | PathFindResult::InvalidEndPoint => {
                    result = Err(RoutingError::InvalidPoint);
                    return JCF::Exit;
                }
            };

            let last_vertex_index = (vertices.len() - 1) as u32;
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
