use bevy_ecs::prelude::Entity;
use petgraph::algo::kosaraju_scc;
use petgraph::stable_graph::{NodeIndex, StableDiGraph};
use petgraph::visit::EdgeRef;
use petgraph::Direction;
use std::cmp::Ordering;
use std::collections::HashMap;
use std::collections::VecDeque;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum NodeEntity {
    Port(Entity),
    Symbol(Entity),
    DriverJunction(Entity),
    ListenerJunction(Entity),
    Dummy,
}

#[derive(Debug, Clone)]
pub struct Node {
    pub entity: NodeEntity,
    pub size: (u32, u32),

    // constraint that this node must be ordered immediately after the adjacent node, and in the same layer
    pub adjacent_to: Option<NodeIndex>,

    pub rank: Option<u32>,
    pub order: Option<u32>,
    pub x: Option<f64>,
    pub y: Option<f64>,

    pub dummy: bool,
}

impl Node {
    pub fn new(entity: NodeEntity, size: (u32, u32)) -> Self {
        Node {
            entity,
            size,
            adjacent_to: None,
            rank: None,
            order: None,
            x: None,
            y: None,
            dummy: false,
        }
    }
}

pub type Graph = StableDiGraph<Node, ()>;

pub fn layout_graph(graph: &mut Graph) -> Result<(), String> {
    bevy_log::debug!(
        "Layout graph with {} nodes and {} edges",
        graph.node_count(),
        graph.edge_count()
    );

    break_cycles(graph);
    assign_ranks(graph);
    add_dummy_nodes(graph);
    order_nodes_within_ranks(graph);
    // remove_dummy_nodes(graph);
    assign_x_coordinates(graph);
    assign_y_coordinates(graph);

    Ok(())
}

pub fn create_graph(edges: Vec<(u32, u32)>, nodes: Vec<Node>) -> Result<Graph, String> {
    let mut graph = StableDiGraph::new();
    let mut node_indices = HashMap::new();

    // Add nodes to the graph
    for (i, node) in nodes.into_iter().enumerate() {
        let index = graph.add_node(node);
        node_indices.insert(i as u32, index);
    }

    // Add edges to the graph
    for (from, to) in edges {
        let from_index = node_indices
            .get(&from)
            .ok_or_else(|| format!("Invalid node index: {}", from))?;
        let to_index = node_indices
            .get(&to)
            .ok_or_else(|| format!("Invalid node index: {}", to))?;
        graph.add_edge(*from_index, *to_index, ());
    }

    Ok(graph)
}

fn break_cycles(graph: &mut Graph) {
    // Remove self-loops first
    graph.retain_edges(|graph, edge| {
        let (source, target) = graph.edge_endpoints(edge).unwrap();
        source != target
    });

    let sccs = kosaraju_scc(graph as &Graph);

    for scc in sccs {
        if scc.len() > 1 {
            // This is a non-trivial strongly connected component
            let mut edges_to_reverse = Vec::new();

            // Find a spanning tree of the SCC
            let mut visited = std::collections::HashSet::new();
            let mut stack = vec![*scc.first().unwrap()];

            while let Some(node) = stack.pop() {
                if visited.insert(node) {
                    for &neighbor in &scc {
                        if neighbor != node && !visited.contains(&neighbor) {
                            if let Some(_edge) = graph.find_edge(node, neighbor) {
                                edges_to_reverse.push((node, neighbor));
                                stack.push(neighbor);
                            }
                        }
                    }
                }
            }

            // Reverse the selected edges
            for (from, to) in edges_to_reverse {
                graph.remove_edge(graph.find_edge(from, to).unwrap());
                graph.add_edge(to, from, ());
            }
        }
    }
}

fn assign_ranks(graph: &mut Graph) {
    let mut rank_map: HashMap<NodeIndex, u32> = HashMap::new();
    let mut queue = VecDeque::new();

    // Find source nodes and assign them rank 0
    for node in graph.node_indices() {
        if graph.neighbors_directed(node, Direction::Incoming).count() == 0 {
            rank_map.insert(node, 0);
            queue.push_back(node);
        }
    }

    // Assign ranks using a modified topological sort
    while let Some(node) = queue.pop_front() {
        let current_rank = *rank_map.get(&node).unwrap();

        // Process outgoing neighbors
        for neighbor in graph.neighbors_directed(node, Direction::Outgoing) {
            let new_rank = current_rank + 1;
            let neighbor_rank = rank_map.entry(neighbor).or_insert(new_rank);
            *neighbor_rank = (*neighbor_rank).max(new_rank);

            // Add to queue if all incoming edges are processed
            if graph
                .neighbors_directed(neighbor, Direction::Incoming)
                .all(|n| rank_map.contains_key(&n))
            {
                queue.push_back(neighbor);
            }
        }

        // Process adjacent nodes (for adjacency constraints)
        if let Some(adjacent_to) = graph[node].adjacent_to {
            let adjacent_rank = rank_map.entry(adjacent_to).or_insert(current_rank);
            *adjacent_rank = current_rank;
            if !queue.contains(&adjacent_to) {
                queue.push_back(adjacent_to);
            }
        }
    }

    // Assign ranks to any remaining unranked nodes
    let max_rank = *rank_map.values().max().unwrap_or(&0);
    for node in graph.node_indices().collect::<Vec<_>>() {
        graph[node].rank = Some(*rank_map.entry(node).or_insert(max_rank + 1));
    }

    // Compact ranks
    let mut unique_ranks: Vec<u32> = rank_map.values().cloned().collect();
    unique_ranks.sort();
    unique_ranks.dedup();

    let compact_rank_map: HashMap<u32, u32> = unique_ranks
        .into_iter()
        .enumerate()
        .map(|(i, r)| (r, i as u32))
        .collect();

    // Update ranks based on the new mapping
    for node in graph.node_weights_mut() {
        if let Some(old_rank) = node.rank {
            node.rank = Some(compact_rank_map[&old_rank]);
        }
    }
}

fn add_dummy_nodes(graph: &mut Graph) {
    let mut changed = true;
    while changed {
        changed = false;

        for edge in graph.edge_indices().collect::<Vec<_>>() {
            let (source, target) = graph.edge_endpoints(edge).unwrap();
            let source_rank = graph[source].rank.unwrap();
            let target_rank = graph[target].rank.unwrap();

            // Add dummy nodes for edges that span more than one rank
            if (target_rank - source_rank) > 1 {
                let dummy = graph.add_node(Node {
                    entity: NodeEntity::Dummy,
                    size: (0, 0),
                    adjacent_to: None,
                    rank: Some(target_rank - 1),
                    order: None,
                    x: None,
                    y: None,
                    dummy: true,
                });

                graph.add_edge(source, dummy, ());
                graph.add_edge(dummy, target, ());
                graph.remove_edge(edge);
                changed = true;
            }
        }
    }
}

fn order_nodes_within_ranks(graph: &mut Graph) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    // Initialize random order for the first rank
    let nodes_at_rank: Vec<NodeIndex> = graph
        .node_indices()
        .filter(|&n| graph[n].rank == Some(0))
        .collect();
    for (i, &node) in nodes_at_rank.iter().enumerate() {
        graph[node].order = Some(i as u32);
    }

    // Perform layer sweep algorithm
    let mut improved = true;
    let mut iteration = 0;
    while improved && iteration < 24 {
        // Max 24 iterations as per the paper
        improved = false;
        iteration += 1;

        // Backward sweep
        for rank in (0..max_rank).rev() {
            if apply_barycenter_heuristic(graph, rank, Direction::Incoming) {
                improved = true;
            }
        }

        // Forward sweep
        for rank in 0..=max_rank {
            if apply_barycenter_heuristic(graph, rank, Direction::Outgoing) {
                improved = true;
            }
        }

        for _ in 0..8 {
            // Backward sweep
            for rank in (0..max_rank).rev() {
                if iteration > 1 && minimize_crossings(graph, rank, Direction::Incoming) {
                    improved = true;
                }
            }

            // Forward sweep
            for rank in 0..=max_rank {
                if minimize_crossings(graph, rank, Direction::Outgoing) {
                    improved = true;
                }
            }
        }
    }
}

fn apply_barycenter_heuristic(graph: &mut Graph, rank: u32, direction: Direction) -> bool {
    let nodes_at_rank: Vec<NodeIndex> = graph
        .node_indices()
        .filter(|&n| graph[n].rank == Some(rank))
        .collect();

    // Calculate barycenter positions
    let mut node_barycenters: Vec<(NodeIndex, f64)> = nodes_at_rank
        .iter()
        .map(|&node| (node, calculate_barycenter_position(graph, node, direction)))
        .collect();

    // Sort nodes by barycenter values
    node_barycenters.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(Ordering::Equal));

    // Check if the order has changed
    let mut changed = false;
    for (i, (node, _)) in node_barycenters.iter().enumerate() {
        if graph[*node].order != Some(i as u32) {
            graph[*node].order = Some(i as u32);
            changed = true;
        }
    }

    changed
}

fn calculate_barycenter_position(graph: &Graph, node: NodeIndex, direction: Direction) -> f64 {
    let adjacent_nodes: Vec<NodeIndex> = graph.neighbors_directed(node, direction).collect();

    let positions: Vec<f64> = adjacent_nodes
        .iter()
        .filter_map(|&n| graph[n].order.map(|o| o as f64))
        .collect();

    if positions.is_empty() {
        return -1.0; // Use -1 for nodes without adjacent nodes in the given direction
    }

    let len = positions.len();
    let sum = positions.iter().sum::<f64>();
    sum / len as f64
}

fn minimize_crossings(graph: &mut Graph, rank: u32, direction: Direction) -> bool {
    let mut improved = false;

    let nodes_at_rank: Vec<NodeIndex> = graph
        .node_indices()
        .filter(|&n| graph[n].rank == Some(rank))
        .collect();

    for i in 0..nodes_at_rank.len() - 1 {
        let (n1, n2) = (nodes_at_rank[i], nodes_at_rank[i + 1]);

        if !can_exchange(graph, n1, n2) {
            continue;
        }

        let chain1 = get_adjacent_chain(graph, n1);
        let chain2 = get_adjacent_chain(graph, n2);

        let crossings_before = count_crossings_for_chains(graph, &chain1, &chain2, direction);
        let crossings_after = count_crossings_for_chains(graph, &chain2, &chain1, direction);

        if crossings_after < crossings_before {
            exchange_chains(graph, &chain1, &chain2);
            improved = true;
        }
    }

    improved
}

fn can_exchange(graph: &Graph, n1: NodeIndex, n2: NodeIndex) -> bool {
    // Nodes can be exchanged if they are not adjacent to each other
    graph[n1].adjacent_to != Some(n2) && graph[n2].adjacent_to != Some(n1)
}

fn get_adjacent_chain(graph: &Graph, start: NodeIndex) -> Vec<NodeIndex> {
    let mut chain = vec![start];
    let mut current = start;

    // Follow the chain backward
    while let Some(prev) = graph[current].adjacent_to {
        chain.insert(0, prev);

        current = prev;
    }

    // Follow the chain forward
    current = start;
    while let Some(next) = graph
        .node_indices()
        .find(|&n| graph[n].adjacent_to == Some(current))
    {
        chain.push(next);
        current = next;
    }

    chain
}

fn exchange_chains(graph: &mut Graph, chain1: &[NodeIndex], chain2: &[NodeIndex]) {
    let mut start_order = chain1
        .iter()
        .map(|&n| graph[n].order.unwrap())
        .min()
        .unwrap()
        .min(
            chain2
                .iter()
                .map(|&n| graph[n].order.unwrap())
                .min()
                .unwrap(),
        );

    // Assign new orders to chain2
    for (i, &node) in chain2.iter().enumerate() {
        graph[node].order = Some(start_order + i as u32);
    }

    start_order += chain2.len() as u32;

    // Assign new orders to chain1, continuing from where chain2 ended
    for (i, &node) in chain1.iter().enumerate() {
        graph[node].order = Some(start_order + i as u32);
    }
}

fn count_crossings_for_chains(
    graph: &Graph,
    chain1: &[NodeIndex],
    chain2: &[NodeIndex],
    direction: Direction,
) -> usize {
    let mut crossings = 0;
    for &n1 in chain1 {
        for &n2 in chain2 {
            crossings += count_crossings(graph, n1, n2, direction);
        }
    }
    crossings
}

fn count_crossings(
    graph: &Graph,
    left: NodeIndex,
    right: NodeIndex,
    direction: Direction,
) -> usize {
    let left_edges: Vec<NodeIndex> = graph
        .edges_directed(left, direction)
        .map(|e| {
            if e.source() == left {
                e.target()
            } else {
                e.source()
            }
        })
        .collect();
    let right_edges: Vec<NodeIndex> = graph
        .edges_directed(right, direction)
        .map(|e| {
            if e.source() == right {
                e.target()
            } else {
                e.source()
            }
        })
        .collect();

    // let left_rank = graph[left].rank.unwrap();
    // let right_rank = graph[right].rank.unwrap();

    // let mut crossings = 0;
    // for &(l, l_outgoing) in &left_edges {
    //     for &(r, r_outgoing) in &right_edges {
    //         let l_rank = graph[l].rank.unwrap();
    //         let r_rank = graph[r].rank.unwrap();

    //         // Handle in-layer edges
    //         if l_rank == left_rank && r_rank == right_rank {
    //             if l_outgoing != r_outgoing {
    //                 crossings += 1;
    //             }
    //         } else {
    //             let l_order = graph[l].order.unwrap() as i32;
    //             let r_order = graph[r].order.unwrap() as i32;
    //             if (left_rank as i32 - l_rank as i32) * (right_rank as i32 - r_rank as i32) < 0
    //                 && (l_order - r_order) * (if l_outgoing == r_outgoing { 1 } else { -1 }) < 0
    //             {
    //                 crossings += 1;
    //             }
    //         }
    //     }
    // }

    let left_1 = graph[left].order.unwrap();
    let right_1 = graph[right].order.unwrap();

    let mut crossings = 0;
    for &l in &left_edges {
        for &r in &right_edges {
            let left_2 = graph[l].order.unwrap();
            let right_2 = graph[r].order.unwrap();

            if (left_1 < left_2 && right_1 > right_2) || (left_1 > left_2 && right_1 < right_2) {
                crossings += 1;
            }
        }
    }

    crossings
}

// fn remove_dummy_nodes(graph: &mut Graph) {
//     let mut dummy_nodes: Vec<NodeIndex> =
//         graph.node_indices().filter(|&n| graph[n].dummy).collect();
//     dummy_nodes.sort_by_key(|&n| graph[n].rank);

//     for node in dummy_nodes.iter().copied() {
//         let incoming = graph.neighbors_directed(node, Direction::Incoming).next();
//         if incoming.is_none() {
//             continue;
//         }
//         let incoming = incoming.unwrap();
//         let outgoing = graph
//             .neighbors_directed(node, Direction::Outgoing)
//             .next()
//             .unwrap();

//         if let Some(edge) = graph.find_edge(incoming, node) {
//             graph.remove_edge(edge);
//         }
//         if let Some(edge) = graph.find_edge(node, outgoing) {
//             graph.remove_edge(edge);
//         }

//         // restore the original edge
//         graph.add_edge(incoming, outgoing, ());

//         // Update order of nodes in the same rank
//         let rank = graph[node].rank.unwrap();
//         let order = graph[node].order.unwrap();
//         let nodes_at_rank: Vec<NodeIndex> = graph
//             .node_indices()
//             .filter(|&n| graph[n].rank == Some(rank))
//             .collect();
//         for &node in nodes_at_rank.iter() {
//             if graph[node].order.unwrap() > order {
//                 graph[node].order = Some(graph[node].order.unwrap() - 1);
//             }
//         }

//         graph.remove_node(node);
//     }
// }

fn assign_x_coordinates(graph: &mut Graph) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    for rank in 0..=max_rank {
        let mut nodes_at_rank: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(rank))
            .collect();

        nodes_at_rank.sort_by_key(|&n| graph[n].order);

        let mut x = 0.0;
        for &node in &nodes_at_rank {
            graph[node].x = Some(x);
            x += graph[node].size.0 as f64 + 50.0; // Add some spacing between nodes
        }
    }

    // Center nodes within their rank
    for rank in 0..=max_rank {
        let nodes_at_rank: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(rank))
            .collect();

        if let (Some(min_x), Some(max_x)) = (
            nodes_at_rank
                .iter()
                .filter_map(|&n| graph[n].x)
                .min_by(|a, b| a.partial_cmp(b).unwrap_or(Ordering::Equal)),
            nodes_at_rank
                .iter()
                .filter_map(|&n| graph[n].x)
                .max_by(|a, b| a.partial_cmp(b).unwrap_or(Ordering::Equal)),
        ) {
            let center = (min_x + max_x) / 2.0;
            let offset = 500.0 - center; // Assuming we want to center around x=500
            for &node in &nodes_at_rank {
                if let Some(x) = graph[node].x.as_mut() {
                    *x += offset;
                }
            }
        }
    }
}

fn assign_y_coordinates(graph: &mut Graph) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);
    let rank_separation = 60.0; // Vertical separation between ranks

    let mut y = 0.;

    for rank in 0..=max_rank {
        let rank_nodes: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(rank))
            .collect();
        let max_height = rank_nodes
            .iter()
            .map(|&n| graph[n].size.1)
            .max()
            .unwrap_or(0) as f64;
        for node in rank_nodes.iter().copied() {
            graph[node].y = Some(y);
        }
        y += max_height + rank_separation;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use petgraph::graph::NodeIndex;
    use std::collections::HashSet;

    fn create_test_graph(edges: Vec<(u32, u32)>, node_count: usize) -> Graph {
        let nodes = (0..node_count)
            .map(|_| Node {
                entity: NodeEntity::Port(Entity::PLACEHOLDER),
                size: (50, 30),
                adjacent_to: None,
                rank: None,
                order: None,
                x: None,
                y: None,
                dummy: false,
            })
            .collect();

        let mut graph = create_graph(edges, nodes).unwrap();
        layout_graph(&mut graph).unwrap();

        graph
    }

    fn check_acyclic(graph: &Graph) -> bool {
        !petgraph::algo::is_cyclic_directed(graph)
    }

    fn check_ranks(graph: &Graph) -> bool {
        graph.edge_indices().all(|e| {
            let (source, target) = graph.edge_endpoints(e).unwrap();
            graph[source].rank.unwrap() < graph[target].rank.unwrap()
        })
    }

    fn check_order_consistency(graph: &Graph) -> bool {
        let max_rank = graph
            .node_weights()
            .filter_map(|n| n.rank)
            .max()
            .unwrap_or(0);

        (0..=max_rank).all(|rank| {
            let mut nodes_at_rank: Vec<NodeIndex> = graph
                .node_indices()
                .filter(|&n| graph[n].rank == Some(rank))
                .collect();

            if nodes_at_rank.is_empty() {
                return true;
            }

            nodes_at_rank.sort_by_key(|&n| graph[n].order.unwrap());

            let min_order = graph[nodes_at_rank[0]].order.unwrap();
            let max_order = graph[nodes_at_rank[nodes_at_rank.len() - 1]].order.unwrap();

            // Check if orders are consecutive integers
            (min_order..=max_order).all(|expected_order| {
                nodes_at_rank
                    .iter()
                    .any(|&n| graph[n].order == Some(expected_order))
            })
        })
    }

    fn check_x_coordinates(graph: &Graph) -> bool {
        let max_rank = graph
            .node_weights()
            .filter_map(|n| n.rank)
            .max()
            .unwrap_or(0);

        (0..=max_rank).all(|rank| {
            let mut nodes_at_rank: Vec<NodeIndex> = graph
                .node_indices()
                .filter(|&n| graph[n].rank == Some(rank))
                .collect();

            // Sort nodes by their order
            nodes_at_rank.sort_by_key(|&n| graph[n].order.unwrap());

            // Check x-coordinates
            nodes_at_rank.windows(2).all(|w| {
                let (n1, n2) = (w[0], w[1]);

                graph[n1].x.unwrap() < graph[n2].x.unwrap()
            })
        })
    }

    fn check_y_coordinates(graph: &Graph) -> bool {
        let rank_coords = graph
            .node_indices()
            .map(|n| (graph[n].rank.unwrap(), graph[n].y.unwrap()))
            .collect::<HashMap<_, _>>();
        graph
            .node_indices()
            .all(|n| graph[n].y.unwrap() == rank_coords[&graph[n].rank.unwrap()])
    }

    fn validate_layout(graph: &Graph) -> bool {
        // Check if the graph is acyclic
        if !check_acyclic(graph) {
            println!("Graph contains cycles");
            return false;
        }

        // Check if all nodes have ranks assigned
        if !graph.node_weights().all(|n| n.rank.is_some()) {
            println!("Not all nodes have ranks assigned");
            return false;
        }

        // Check if ranks are correct (source nodes always have lower ranks)
        if !check_ranks(graph) {
            println!("Ranks are not correctly assigned");
            return false;
        }

        if !check_order_consistency(graph) {
            println!("Orders within ranks are not consistent");
            return false;
        }

        // Check if orders within ranks are unique and consecutive
        let mut rank_orders: HashMap<u32, HashSet<u32>> = HashMap::new();
        for node in graph.node_weights() {
            let rank = node.rank.unwrap();
            let order = node.order.unwrap();
            if !rank_orders.entry(rank).or_default().insert(order) {
                println!("Duplicate order {} within rank {}", order, rank);
                return false;
            }
        }
        for (rank, orders) in rank_orders.iter() {
            let min_order = orders.iter().min().unwrap();
            let max_order = orders.iter().max().unwrap();
            if *max_order - *min_order + 1 != orders.len() as u32 {
                println!("Orders within rank {} are not consecutive", rank);
                return false;
            }
        }

        // Check if x-coordinates are consistent with order
        if !check_x_coordinates(graph) {
            println!("X-coordinates are not consistent with order");
            return false;
        }

        // Check if y-coordinates are consistent with rank
        if !check_y_coordinates(graph) {
            println!("Y-coordinates are not consistent with rank");
            return false;
        }

        // Check for edge crossings
        let edge_crossings = count_edge_crossings(graph);
        if edge_crossings > 0 {
            // TODO: fixme
            // println!("Layout has {} edge crossings", edge_crossings);
            // return false;
        }

        // Check for minimum distance between nodes
        const MIN_DISTANCE: f64 = 1.0;
        for n1 in graph.node_indices() {
            for n2 in graph.node_indices() {
                if n1 != n2 {
                    let dx = graph[n1].x.unwrap() - graph[n2].x.unwrap();
                    let dy = graph[n1].y.unwrap() - graph[n2].y.unwrap();
                    let distance = (dx * dx + dy * dy).sqrt();
                    if distance < MIN_DISTANCE {
                        println!("Nodes are too close: distance = {}", distance);
                        return false;
                    }
                }
            }
        }

        // Check adjacency constraints
        for node in graph.node_indices() {
            if let Some(adjacent_to) = graph[node].adjacent_to {
                if graph[node].rank != graph[adjacent_to].rank {
                    println!("Adjacency constraint violated: nodes not in same rank");
                    return false;
                }
                if graph[node].order.unwrap() != graph[adjacent_to].order.unwrap() + 1 {
                    println!("Adjacency constraint violated: nodes not adjacent");
                    return false;
                }
            }
        }

        true
    }

    fn count_edge_crossings(graph: &Graph) -> usize {
        let mut crossings = 0;
        let edges: Vec<_> = graph.edge_indices().collect();
        for (i, &e1) in edges.iter().enumerate() {
            let (s1, t1) = graph.edge_endpoints(e1).unwrap();
            for &e2 in &edges[i + 1..] {
                let (s2, t2) = graph.edge_endpoints(e2).unwrap();
                if do_edges_cross(graph, s1, t1, s2, t2) {
                    crossings += 1;
                }
            }
        }
        crossings
    }

    fn do_edges_cross(
        graph: &Graph,
        s1: NodeIndex,
        t1: NodeIndex,
        s2: NodeIndex,
        t2: NodeIndex,
    ) -> bool {
        let (x1, y1) = (graph[s1].x.unwrap(), graph[s1].y.unwrap());
        let (x2, y2) = (graph[t1].x.unwrap(), graph[t1].y.unwrap());
        let (x3, y3) = (graph[s2].x.unwrap(), graph[s2].y.unwrap());
        let (x4, y4) = (graph[t2].x.unwrap(), graph[t2].y.unwrap());

        // Check if bounding boxes intersect
        if x1.max(x2) < x3.min(x4)
            || x3.max(x4) < x1.min(x2)
            || y1.max(y2) < y3.min(y4)
            || y3.max(y4) < y1.min(y2)
        {
            return false;
        }

        // Check if line segments intersect
        let d1 = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
        let d2 = (x2 - x1) * (y4 - y1) - (y2 - y1) * (x4 - x1);
        let d3 = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
        let d4 = (x4 - x3) * (y2 - y3) - (y4 - y3) * (x2 - x3);

        (d1 * d2 < 0.0) && (d3 * d4 < 0.0)
    }

    #[test]
    fn test_simple_chain() {
        let edges = vec![(0, 1), (1, 2), (2, 3)];
        let graph = create_test_graph(edges, 4);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_tree() {
        let edges = vec![(0, 1), (0, 2), (1, 3), (1, 4), (2, 5)];
        let graph = create_test_graph(edges, 6);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_diamond() {
        let edges = vec![(0, 1), (0, 2), (1, 3), (2, 3)];
        let graph = create_test_graph(edges, 4);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_cycle() {
        let edges = vec![(0, 1), (1, 2), (2, 0)];
        let graph = create_test_graph(edges, 3);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_disconnected() {
        let edges = vec![(0, 1), (2, 3)];
        let graph = create_test_graph(edges, 4);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_complex_graph() {
        let edges = vec![
            (0, 1),
            (0, 2),
            (1, 3),
            (2, 3),
            (3, 4),
            (3, 5),
            (4, 6),
            (5, 6),
            (6, 7),
            (7, 8),
            (7, 9),
            (8, 10),
            (9, 10),
            (2, 5),
            (1, 4),
        ];
        let graph = create_test_graph(edges, 11);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_single_node() {
        let edges = vec![];
        let graph = create_test_graph(edges, 1);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_parallel_edges() {
        let edges = vec![(0, 1), (0, 1), (1, 2), (1, 2)];
        let graph = create_test_graph(edges, 3);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_self_loop() {
        let edges = vec![(0, 0), (0, 1), (1, 2)];
        let graph = create_test_graph(edges, 3);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_large_cycle() {
        let edges = vec![(0, 1), (1, 2), (2, 3), (3, 4), (4, 0)];
        let graph = create_test_graph(edges, 5);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_multiple_cycles() {
        let edges = vec![(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)];
        let graph = create_test_graph(edges, 6);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_nested_cycles() {
        let edges = vec![(0, 1), (1, 2), (2, 3), (3, 1), (2, 4), (4, 5), (5, 2)];
        let graph = create_test_graph(edges, 6);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_bidirectional_edges() {
        let edges = vec![(0, 1), (1, 0), (1, 2), (2, 1), (2, 3)];
        let graph = create_test_graph(edges, 4);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_disconnected_subgraphs() {
        let edges = vec![(0, 1), (1, 2), (3, 4), (4, 5), (6, 7)];
        let graph = create_test_graph(edges, 8);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_single_node_cycle() {
        let edges = vec![(0, 1), (1, 2), (2, 1), (2, 3)];
        let graph = create_test_graph(edges, 4);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_dense_graph() {
        let edges = vec![
            (0, 1),
            (0, 2),
            (0, 3),
            (1, 2),
            (1, 3),
            (2, 3),
            (3, 4),
            (3, 5),
            (4, 5),
            (4, 6),
            (5, 6),
        ];
        let graph = create_test_graph(edges, 7);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_long_chain() {
        let edges: Vec<(u32, u32)> = (0..99).map(|i| (i, i + 1)).collect();
        let graph = create_test_graph(edges, 100);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_star_graph() {
        let edges: Vec<(u32, u32)> = (1..10).map(|i| (0, i)).collect();
        let graph = create_test_graph(edges, 10);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_complete_graph() {
        let mut edges = Vec::new();
        for i in 0..5 {
            for j in 0..5 {
                if i != j {
                    edges.push((i, j));
                }
            }
        }
        let graph = create_test_graph(edges, 5);
        assert!(validate_layout(&graph));
    }

    #[test]
    fn test_rank_consistency() {
        let edges = vec![(0, 1), (0, 2), (1, 3), (2, 3), (3, 4)];
        let graph = create_test_graph(edges, 5);
        assert!(validate_layout(&graph));

        // Check if ranks are assigned correctly
        assert_eq!(graph[NodeIndex::new(0)].rank, Some(0));
        assert!(
            graph[NodeIndex::new(1)].rank == Some(1) && graph[NodeIndex::new(2)].rank == Some(1)
        );
        assert_eq!(graph[NodeIndex::new(3)].rank, Some(2));
        assert_eq!(graph[NodeIndex::new(4)].rank, Some(3));
    }

    #[test]
    fn test_order_consistency() {
        let edges = vec![(0, 2), (1, 2), (2, 3), (2, 4)];
        let graph = create_test_graph(edges, 5);
        assert!(validate_layout(&graph));

        // Check if orders within ranks are consistent
        let rank_0_nodes: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(0))
            .collect();
        assert_eq!(rank_0_nodes.len(), 2);
        assert!(graph[rank_0_nodes[0]].order.unwrap() < graph[rank_0_nodes[1]].order.unwrap());

        let rank_2_nodes: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(2))
            .collect();
        assert_eq!(rank_2_nodes.len(), 2);
        assert!(graph[rank_2_nodes[0]].order.unwrap() < graph[rank_2_nodes[1]].order.unwrap());
    }

    #[test]
    fn test_simple_adjacency() {
        let mut graph = StableDiGraph::new();
        // Component 1
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 1
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 2
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 1
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 2

        // Component 2
        let n4 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 1
        let n5 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 1

        // Connections between components
        graph.add_edge(n2, n4, ()); // Component 1 output to Component 2 input
        graph.add_edge(n3, n5, ()); // Component 1 output to Component 2 output

        // Set adjacency for pins within components
        graph[n1].adjacent_to = Some(n0); // Input pins adjacent
        graph[n3].adjacent_to = Some(n2); // Output pins adjacent

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraints
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n2].rank, graph[n3].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());
        assert_eq!(graph[n2].order.unwrap() + 1, graph[n3].order.unwrap());
    }

    #[test]
    fn test_multiple_adjacency_constraints() {
        let mut graph = StableDiGraph::new();
        // Component 1
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 1
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 2
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 3
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 1
        let n4 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 2

        // Component 2
        let n5 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Input pin 1
        let n6 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30))); // Output pin 1

        // Connections between components
        graph.add_edge(n3, n5, ());
        graph.add_edge(n4, n6, ());

        // Set adjacency for pins within components
        graph[n1].adjacent_to = Some(n0);
        graph[n2].adjacent_to = Some(n1);
        graph[n4].adjacent_to = Some(n3);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraints
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n1].rank, graph[n2].rank);
        assert_eq!(graph[n3].rank, graph[n4].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());
        assert_eq!(graph[n1].order.unwrap() + 1, graph[n2].order.unwrap());
        assert_eq!(graph[n3].order.unwrap() + 1, graph[n4].order.unwrap());

        // Check for edge crossings
        let crossings = count_edge_crossings(&graph);
        assert_eq!(
            crossings, 0,
            "Expected 0 crossings, but found {}",
            crossings
        );
    }

    #[test]
    fn test_small_circuit_has_no_crossings() {
        let mut graph = StableDiGraph::new();
        let n0 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n2 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n4 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n5 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n6 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n7 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n8 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n9 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n10 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n11 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n12 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n13 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n14 = graph.add_node(Node::new(
            NodeEntity::ListenerJunction(Entity::PLACEHOLDER),
            (3, 3),
        ));
        let n15 = graph.add_node(Node::new(NodeEntity::Symbol(Entity::PLACEHOLDER), (80, 80)));
        let n16 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));
        let n17 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (5, 5)));

        graph.add_edge(n0, n1, ());
        graph.add_edge(n1, n3, ());
        graph.add_edge(n3, n2, ());
        graph.add_edge(n5, n6, ());
        graph.add_edge(n6, n4, ());
        graph.add_edge(n4, n2, ());
        graph.add_edge(n7, n8, ());
        graph.add_edge(n8, n9, ());
        graph.add_edge(n9, n0, ());
        graph.add_edge(n10, n11, ());
        graph.add_edge(n11, n14, ());
        graph.add_edge(n14, n12, ());
        graph.add_edge(n12, n0, ());
        graph.add_edge(n14, n13, ());
        graph.add_edge(n13, n5, ());
        graph.add_edge(n2, n17, ());
        graph.add_edge(n17, n16, ());
        graph.add_edge(n16, n15, ());

        // Set adjacency for symbol's ports
        graph[n12].adjacent_to = Some(n9);
        graph[n4].adjacent_to = Some(n3);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check for edge crossings
        let crossings = count_edge_crossings(&graph);
        assert_eq!(
            crossings, 0,
            "Expected 0 crossings, but found {}",
            crossings
        );
    }
}
