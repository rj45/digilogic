use crate::graph::{Graph, NodeIndex, INVALID_NODE_INDEX};
use bevy_log::{debug, error, info_span};
use digilogic_core::transform::*;
use digilogic_core::{fixed, Fixed, HashMap, HashSet};
use std::cmp::Reverse;

type PriorityQueue<I, P> = priority_queue::PriorityQueue<I, P, ahash::RandomState>;

#[derive(Debug, Clone)]
pub enum PathFindResult {
    Found(Path),
    NotFound,
    InvalidStartPoint,
    InvalidEndPoint,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PathNodeKind {
    Normal,
    Start,
    End,
    Waypoint,
}

#[derive(Debug, Clone, Copy)]
pub struct PathNode {
    pub position: Vec2,
    pub kind: PathNodeKind,
    pub bend_direction: Option<Direction>,
}

#[derive(Default, Debug, Clone)]
#[repr(transparent)]
pub struct Path {
    nodes: Vec<PathNode>,
}

impl Path {
    #[inline]
    pub fn nodes(&self) -> &[PathNode] {
        &self.nodes
    }

    pub fn iter_pruned(&self) -> impl Iterator<Item = (usize, PathNode)> + '_ {
        let mut prev_dir: Option<Direction> = None;
        self.nodes
            .iter()
            .enumerate()
            .filter_map(move |(index, &node)| {
                let include = if node.kind == PathNodeKind::Normal {
                    match (node.bend_direction, prev_dir) {
                        (Some(dir), Some(prev_dir)) => dir != prev_dir,
                        _ => true,
                    }
                } else {
                    true
                };

                prev_dir = node.bend_direction;

                if include {
                    Some((index, node))
                } else {
                    None
                }
            })
    }
}

#[derive(Default)]
pub(crate) struct PathFinder {
    end_indices: HashSet<NodeIndex>,
    g_score: HashMap<NodeIndex, Fixed>,
    predecessor: HashMap<NodeIndex, NodeIndex>,
    open_queue: PriorityQueue<NodeIndex, Reverse<Fixed>>,
}

impl PathFinder {
    #[cfg(debug_assertions)]
    #[tracing::instrument(skip_all)]
    fn assert_data_is_valid(&self, graph: &Graph) {
        for (&node_index, &pred_index) in &self.predecessor {
            assert_ne!(node_index, INVALID_NODE_INDEX);
            assert_ne!(pred_index, INVALID_NODE_INDEX);

            let node = &graph.nodes[node_index];
            let pred = &graph.nodes[pred_index];

            let node_to_pred_dir = node.neighbors.find(pred_index);
            let pred_to_node_dir = pred.neighbors.find(node_index);

            assert!(node_to_pred_dir.is_some());
            assert!(pred_to_node_dir.is_some());
            assert_eq!(
                node_to_pred_dir.unwrap(),
                pred_to_node_dir.unwrap().opposite()
            );
        }
    }

    #[cfg(not(debug_assertions))]
    fn assert_data_is_valid(&self, _graph: &Graph) {}

    fn build_path(
        &mut self,
        path: &mut Path,
        graph: &Graph,
        start_index: NodeIndex,
        end_index: NodeIndex,
    ) {
        // If there was a previous path segment, don't duplicate the joining point.
        if !path.nodes.is_empty() {
            let prev_end = path.nodes.pop();
            assert!(
                matches!(
                    prev_end,
                    Some(PathNode {
                        kind: PathNodeKind::End,
                        bend_direction: None,
                        ..
                    }),
                ),
                "invalid end node",
            );
        }

        let insert_index = path.nodes.len();
        path.nodes.push(PathNode {
            position: graph.nodes[end_index].position,
            kind: PathNodeKind::End,
            bend_direction: None,
        });

        if end_index == start_index {
            return;
        }

        let mut current_index = end_index;
        loop {
            let pred_index = *self.predecessor.get(&current_index).expect("invalid path");
            let pred = &graph.nodes[pred_index];

            let dir = pred
                .neighbors
                .find(current_index)
                .expect("invalid predecessor");

            if pred_index == start_index {
                let kind = if insert_index == 0 {
                    PathNodeKind::Start
                } else {
                    PathNodeKind::Waypoint
                };

                path.nodes.insert(
                    insert_index,
                    PathNode {
                        position: pred.position,
                        kind,
                        bend_direction: Some(dir),
                    },
                );

                break;
            } else {
                path.nodes.insert(
                    insert_index,
                    PathNode {
                        position: pred.position,
                        kind: PathNodeKind::Normal,
                        bend_direction: Some(dir),
                    },
                );

                current_index = pred_index;
            }
        }
    }

    fn length_in_direction(&self, graph: &Graph, mut node: NodeIndex, dir: Direction) -> Fixed {
        let mut len = fixed!(0);
        while let Some(&pred) = self.predecessor.get(&node) {
            if graph.nodes[node].neighbors[dir] == pred {
                let node_pos = graph.nodes[node].position;
                let pred_pos = graph.nodes[pred].position;
                len += node_pos.manhatten_distance_to(pred_pos);

                node = pred;
            } else {
                break;
            }
        }
        len
    }

    #[tracing::instrument(skip_all, name = "find_path")]
    fn find_path_impl(&mut self, graph: &Graph, start_index: NodeIndex) -> PathFindResult {
        let mut path = Path::default();

        self.g_score.clear();
        self.predecessor.clear();
        self.open_queue.clear();

        self.g_score.insert(start_index, fixed!(0));
        self.open_queue.push(start_index, Reverse(fixed!(0)));

        'outer: {
            while let Some((current_index, _)) = self.open_queue.pop() {
                let current_node = &graph.nodes[current_index];

                let pred_index = self.predecessor.get(&current_index).copied();

                // Shortest path to one end found, construct it.
                if self.end_indices.contains(&current_index) {
                    self.assert_data_is_valid(graph);
                    self.build_path(&mut path, graph, start_index, current_index);
                    break 'outer;
                }

                let pred = pred_index.map(|pred_index| (pred_index, &graph.nodes[pred_index]));

                // Find which direction is straight ahead to apply weights.
                let straight_dir = pred.map(|(pred_index, pred_node)| {
                    let pred_to_current_dir = pred_node
                        .neighbors
                        .find(current_index)
                        .expect("invalid predecessor");

                    let current_to_pred_dir = current_node.neighbors.find(pred_index);
                    debug_assert_eq!(current_to_pred_dir, Some(pred_to_current_dir.opposite()));

                    pred_to_current_dir
                });

                let straight_length = if let Some(straight_dir) = straight_dir {
                    self.length_in_direction(graph, current_index, straight_dir.opposite())
                } else {
                    fixed!(0)
                };

                let corner_penalty = (straight_length / fixed!(100)).sqr().max(fixed!(50));

                for dir in Direction::ALL {
                    if Some(dir.opposite()) == straight_dir {
                        // The path came from here.
                        continue;
                    }

                    let neighbor_index = current_node.neighbors[dir];
                    if neighbor_index == INVALID_NODE_INDEX {
                        continue;
                    }

                    let neighbor_node = &graph.nodes[neighbor_index];
                    debug_assert_eq!(neighbor_node.neighbors[dir.opposite()], current_index);

                    // Calculate the new path length.
                    let new_g_score = self.g_score[&current_index]
                        + current_node
                            .position
                            .manhatten_distance_to(neighbor_node.position)
                        + if Some(dir) == straight_dir {
                            fixed!(0)
                        } else {
                            corner_penalty
                        };

                    // Check whether the new path length is shorter than the previous one.
                    let update = match self.g_score.get(&neighbor_index) {
                        Some(&g_score) => new_g_score < g_score,
                        None => true,
                    };

                    if update {
                        // Shorter path found, update it.
                        self.g_score.insert(neighbor_index, new_g_score);
                        self.predecessor.insert(neighbor_index, current_index);

                        // Calculate the new approximate total cost.
                        let new_f_score = new_g_score
                            + self
                                .end_indices
                                .iter()
                                .map(|&end_index| &graph.nodes[end_index])
                                .map(|end| {
                                    neighbor_node.position.manhatten_distance_to(end.position)
                                })
                                .min()
                                .expect("empty end point list");

                        self.open_queue.push(neighbor_index, Reverse(new_f_score));
                    }
                }
            }

            #[cfg(debug_assertions)]
            {
                use std::fmt::Write;

                let scope = info_span!("print_debug_log");
                let scope = scope.enter();

                let mut msg = String::new();
                write!(msg, "unable to find path to remaining waypoints [").unwrap();
                for (i, &end_index) in self.end_indices.iter().enumerate() {
                    if i > 0 {
                        write!(msg, ", ").unwrap();
                    }

                    let end_node = &graph.nodes[end_index];
                    write!(msg, "({}, {})", end_node.position.x, end_node.position.y).unwrap();
                }
                write!(msg, "]").unwrap();
                debug!("{}", msg);

                drop(scope);
            }
        }

        if !path.nodes.is_empty() {
            PathFindResult::Found(path)
        } else {
            PathFindResult::NotFound
        }
    }

    pub(crate) fn find_path(&mut self, graph: &Graph, start: Vec2, end: Vec2) -> PathFindResult {
        let Some(start_index) = graph.find_node(start) else {
            error!(
                "Start point ({}, {}) does not exist in the graph",
                start.x, start.y,
            );
            return PathFindResult::InvalidStartPoint;
        };

        let Some(end_index) = graph.find_node(end) else {
            error!(
                "End point ({}, {}) does not exist in the graph",
                end.x, end.y,
            );
            return PathFindResult::InvalidEndPoint;
        };

        if graph.nodes[end_index].neighbor_count() == 0 {
            return PathFindResult::NotFound;
        }

        self.end_indices.clear();
        self.end_indices.insert(end_index);

        self.find_path_impl(graph, start_index)
    }

    pub(crate) fn find_path_multi(
        &mut self,
        graph: &Graph,
        start: Vec2,
        ends: impl Iterator<Item = Vec2>,
    ) -> PathFindResult {
        let Some(start_index) = graph.find_node(start) else {
            error!(
                "Start point ({}, {}) does not exist in the graph",
                start.x, start.y,
            );
            return PathFindResult::InvalidStartPoint;
        };

        self.end_indices.clear();
        let mut total_neighbor_count = 0;
        for end in ends {
            let Some(end_index) = graph.find_node(end) else {
                error!(
                    "End point ({}, {}) does not exist in the graph",
                    end.x, end.y,
                );
                return PathFindResult::InvalidEndPoint;
            };

            let end_node = &graph.nodes[end_index];
            let neighbor_count = end_node.neighbor_count();

            if neighbor_count > 0 {
                if self.end_indices.insert(end_index) {
                    total_neighbor_count += neighbor_count;
                }
            } else {
                #[cfg(debug_assertions)]
                debug!("End point ({}, {}) unreachable, skipping", end.x, end.y);
            }
        }

        if total_neighbor_count == 0 {
            return PathFindResult::NotFound;
        }

        self.find_path_impl(graph, start_index)
    }
}
