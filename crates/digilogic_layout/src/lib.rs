use bevy_ecs::prelude::Entity;
use petgraph::algo::kosaraju_scc;
use petgraph::graph::{DiGraph, NodeIndex};
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
}

#[derive(Debug, Clone)]
pub struct Node {
    pub entity: NodeEntity,
    pub size: (u32, u32),

    // constraint that this node must be ordered immediately after this node, and in the same layer
    pub adjacent_to: Option<NodeIndex>,

    pub rank: Option<u32>,
    pub order: Option<u32>,
    pub x: Option<f64>,
    pub y: Option<f64>,
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
        }
    }
}

pub type Graph = DiGraph<Node, ()>;

pub fn layout_graph(graph: &mut Graph) -> Result<(), String> {
    bevy_log::debug!(
        "Layout graph with {} nodes and {} edges",
        graph.node_count(),
        graph.edge_count()
    );

    bevy_log::debug!("Breaking cycles");
    break_cycles(graph);

    bevy_log::debug!("Assigning ranks");
    assign_ranks(graph);

    bevy_log::debug!("Ordering nodes within ranks");
    order_nodes_within_ranks(graph);

    bevy_log::debug!("Assigning X coords");
    assign_x_coordinates(graph);

    bevy_log::debug!("Assigning Y coords");
    assign_y_coordinates(graph);

    Ok(())
}

pub fn create_graph(edges: Vec<(u32, u32)>, nodes: Vec<Node>) -> Result<Graph, String> {
    let mut graph = DiGraph::new();
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
    let mut in_degree = HashMap::new();
    let mut queue = VecDeque::new();

    // Initialize in-degree for each node and find source nodes
    for node in graph.node_indices() {
        let degree = graph.neighbors_directed(node, Direction::Incoming).count();
        in_degree.insert(node, degree);
        if degree == 0 {
            queue.push_back(node);
            graph[node].rank = Some(0);
        }
    }

    // Assign ranks using a topological sort
    while let Some(node) = queue.pop_front() {
        let current_rank = graph[node].rank.unwrap();

        let neighbors = graph
            .neighbors_directed(node, Direction::Outgoing)
            .collect::<Vec<_>>();
        for neighbor in neighbors.iter().copied() {
            let new_rank = current_rank + 1;
            graph[neighbor].rank = Some(graph[neighbor].rank.map_or(new_rank, |r| r.max(new_rank)));

            *in_degree.get_mut(&neighbor).unwrap() -= 1;
            if in_degree[&neighbor] == 0 {
                queue.push_back(neighbor);
            }
        }
    }

    // Assign ranks to any remaining unranked nodes
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);
    for node in graph.node_indices() {
        if graph[node].rank.is_none() {
            graph[node].rank = Some(max_rank + 1);
        }
    }

    println!("Initial rank assignment:");
    for node in graph.node_indices() {
        println!("Node {:?}: rank {:?}", node, graph[node].rank);
    }

    // Adjust ranks for adjacency constraints
    loop {
        let mut nodes_to_update = Vec::new();
        for node in graph.node_indices() {
            if let Some(adjacent_to) = graph[node].adjacent_to {
                if graph[node].rank != graph[adjacent_to].rank {
                    nodes_to_update.push((node, graph[adjacent_to].rank));
                }
            }
        }

        if nodes_to_update.is_empty() {
            break;
        }

        for (node, new_rank) in nodes_to_update {
            println!(
                "Updating node {:?} rank from {:?} to {:?}",
                node, graph[node].rank, new_rank
            );
            graph[node].rank = new_rank;
        }
    }

    println!("Final rank assignment:");
    for node in graph.node_indices() {
        println!("Node {:?}: rank {:?}", node, graph[node].rank);
    }

    // Compact ranks
    let mut rank_map = HashMap::new();
    let mut current_rank = 0;

    for rank in 0..=max_rank {
        if graph.node_indices().any(|n| graph[n].rank == Some(rank)) {
            rank_map.insert(rank, current_rank);
            current_rank += 1;
        }
    }

    // Update ranks based on the new mapping
    for node in graph.node_indices() {
        if let Some(old_rank) = graph[node].rank {
            graph[node].rank = Some(*rank_map.get(&old_rank).unwrap());
        }
    }
}

fn order_nodes_within_ranks(graph: &mut Graph) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    // Perform a few iterations to improve the ordering
    for pass in 0..4 {
        bevy_log::debug!("Ordering pass {}", pass);

        for direction in &[Direction::Outgoing, Direction::Incoming] {
            for rank in 0..=max_rank {
                let nodes_at_rank: Vec<NodeIndex> = graph
                    .node_indices()
                    .filter(|&n| graph[n].rank == Some(rank))
                    .collect();

                // Collapse chains of adjacent nodes
                let mut chains: Vec<Vec<NodeIndex>> = Vec::new();
                for &node in &nodes_at_rank {
                    if !chains.iter().any(|chain| chain.contains(&node)) {
                        let chain = get_adjacent_chain(graph, node);
                        chains.push(chain);
                    }
                }

                // Calculate median position for each chain
                let mut weighted_chains: Vec<(Vec<NodeIndex>, f64)> = chains
                    .into_iter()
                    .map(|chain| {
                        let median = chain
                            .iter()
                            .map(|&n| calculate_median_position(graph, n, *direction))
                            .sum::<f64>()
                            / chain.len() as f64;
                        (chain, median)
                    })
                    .collect();

                // Sort chains
                weighted_chains.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(Ordering::Equal));

                // Assign orders, handling cases where all nodes have the same calculated position, as well as adjacent constraints
                let mut current_order = 0;
                for (chain, _) in weighted_chains {
                    for &node in &chain {
                        graph[node].order = Some(current_order);
                        current_order += 1;
                    }
                }
            }
        }

        minimize_crossings(graph);
    }
}

fn calculate_median_position(graph: &Graph, node: NodeIndex, direction: Direction) -> f64 {
    let adjacent_nodes: Vec<NodeIndex> = graph.neighbors_directed(node, direction).collect();

    let positions: Vec<f64> = adjacent_nodes
        .iter()
        .filter_map(|&n| graph[n].order.map(|o| o as f64))
        .collect();

    if positions.is_empty() {
        return -1.0; // Use -1 for nodes without adjacent nodes in the given direction
    }

    let len = positions.len();
    if len % 2 == 1 {
        positions[len / 2]
    } else {
        let mid = len / 2;
        (positions[mid - 1] + positions[mid]) / 2.0
    }
}

fn minimize_crossings(graph: &mut Graph) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    let mut exchanged = VecDeque::new();

    for rank in 0..max_rank {
        let mut improved = true;
        let mut tries = 1000; // fail safe in case the exchanged history is too short
        while improved && tries > 0 {
            improved = false;
            tries -= 1;

            let nodes_at_rank: Vec<NodeIndex> = graph
                .node_indices()
                .filter(|&n| graph[n].rank == Some(rank))
                .collect();

            for i in 0..nodes_at_rank.len() - 1 {
                let (n1, n2) = (nodes_at_rank[i], nodes_at_rank[i + 1]);

                // Check if these nodes can be exchanged
                if !can_exchange(graph, n1, n2) {
                    continue;
                }

                let chain1 = get_adjacent_chain(graph, n1);
                let chain2 = get_adjacent_chain(graph, n2);

                // Only consider exchanging if the end of chain1 is adjacent to the start of chain2
                if graph[*chain1.last().unwrap()].order.unwrap() + 1
                    != graph[*chain2.first().unwrap()].order.unwrap()
                {
                    continue;
                }

                if count_crossings_for_chains(graph, &chain1, &chain2)
                    > count_crossings_for_chains(graph, &chain2, &chain1)
                {
                    if exchanged.iter().any(|&(a, b)| {
                        chain2.contains(&a) && chain1.contains(&b)
                            || chain1.contains(&a) && chain2.contains(&b)
                    }) {
                        // don't exchange chains that were already recently exchanged
                        continue;
                    }

                    exchange_chains(graph, &chain1, &chain2);

                    improved = true;
                    exchanged.push_back((n1, n2));
                    if exchanged.len() > 6 {
                        exchanged.pop_front();
                    }
                }
            }
        }
    }
}

fn can_exchange(graph: &Graph, n1: NodeIndex, n2: NodeIndex) -> bool {
    // Nodes can be exchanged if they are not adjacent to each other
    graph[n1].adjacent_to != Some(n2) && graph[n2].adjacent_to != Some(n1)
}

fn get_adjacent_chain(graph: &Graph, start: NodeIndex) -> Vec<NodeIndex> {
    let mut chain = vec![start];
    let mut current = start;

    // Follow the chain forward
    while let Some(next) = graph[current].adjacent_to {
        chain.push(next);
        current = next;
    }

    // Follow the chain backward
    current = start;
    while let Some(prev) = graph
        .node_indices()
        .find(|&n| graph[n].adjacent_to == Some(current))
    {
        chain.insert(0, prev);
        current = prev;
    }

    chain
}

fn exchange_chains(graph: &mut Graph, chain1: &[NodeIndex], chain2: &[NodeIndex]) {
    let start_order = chain1
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

    // Assign new orders to chain1, continuing from where chain2 ended
    for (i, &node) in chain1.iter().enumerate() {
        graph[node].order = Some(start_order + (chain2.len() + i) as u32);
    }

    // Adjust orders of nodes between the two chains if necessary
    let end_order = start_order + (chain1.len() + chain2.len() - 1) as u32;
    for node in graph.node_indices() {
        let order = graph[node].order.unwrap();
        if order > end_order {
            graph[node].order =
                Some(order + (chain1.len() as i32 - chain2.len() as i32).unsigned_abs());
        }
    }
}

fn count_crossings_for_chains(graph: &Graph, chain1: &[NodeIndex], chain2: &[NodeIndex]) -> usize {
    let mut crossings = 0;
    for &n1 in chain1 {
        for &n2 in chain2 {
            crossings += count_crossings(graph, n1, n2);
        }
    }
    crossings
}

fn count_crossings(graph: &Graph, left: NodeIndex, right: NodeIndex) -> usize {
    let left_edges: Vec<NodeIndex> = graph
        .neighbors_directed(left, Direction::Outgoing)
        .collect();
    let right_edges: Vec<NodeIndex> = graph
        .neighbors_directed(right, Direction::Outgoing)
        .collect();

    let mut crossings = 0;
    for &l in &left_edges {
        for &r in &right_edges {
            if graph[l].order.unwrap_or(0) > graph[r].order.unwrap_or(0) {
                crossings += 1;
            }
        }
    }
    crossings
}

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
    let rank_separation = 100.0; // Vertical separation between ranks

    for rank in 0..=max_rank {
        let y = rank as f64 * rank_separation;
        let rank_nodes: Vec<NodeIndex> = graph
            .node_indices()
            .filter(|&n| graph[n].rank == Some(rank))
            .collect();
        for node in rank_nodes.iter().copied() {
            graph[node].y = Some(y);
        }
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
            let nodes_at_rank: Vec<NodeIndex> = graph
                .node_indices()
                .filter(|&n| graph[n].rank == Some(rank))
                .collect();

            nodes_at_rank
                .windows(2)
                .all(|w| graph[w[0]].x.unwrap() < graph[w[1]].x.unwrap())
        })
    }

    fn check_y_coordinates(graph: &Graph) -> bool {
        graph
            .node_indices()
            .all(|n| graph[n].y.unwrap() == graph[n].rank.unwrap() as f64 * 100.0)
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
            println!("Layout has {} edge crossings", edge_crossings);
            return false;
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
        let mut graph = DiGraph::new();
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));

        graph.add_edge(n0, n1, ());
        graph.add_edge(n1, n2, ());
        graph.add_edge(n2, n3, ());

        graph[n1].adjacent_to = Some(n0);
        graph[n2].adjacent_to = Some(n1);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraints
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n1].rank, graph[n2].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());
        assert_eq!(graph[n1].order.unwrap() + 1, graph[n2].order.unwrap());
    }

    #[test]
    fn test_adjacency_with_crossing() {
        let mut graph = DiGraph::new();
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));

        graph.add_edge(n0, n2, ());
        graph.add_edge(n1, n3, ());

        graph[n1].adjacent_to = Some(n0);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraint
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());

        // Check for edge crossings
        let crossings = count_edge_crossings(&graph);
        assert_eq!(
            crossings, 0,
            "Expected 0 crossings, but found {}",
            crossings
        );
    }

    #[test]
    fn test_multiple_adjacency_constraints() {
        let mut graph = DiGraph::new();
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n4 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));

        graph.add_edge(n0, n3, ());
        graph.add_edge(n1, n4, ());
        graph.add_edge(n2, n4, ());

        graph[n1].adjacent_to = Some(n0);
        graph[n2].adjacent_to = Some(n1);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraints
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n1].rank, graph[n2].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());
        assert_eq!(graph[n1].order.unwrap() + 1, graph[n2].order.unwrap());

        // Check for edge crossings
        let crossings = count_edge_crossings(&graph);
        assert_eq!(
            crossings, 0,
            "Expected 0 crossings, but found {}",
            crossings
        );
    }

    #[test]
    fn test_adjacency_with_complex_crossing() {
        let mut graph = DiGraph::new();
        let n0 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n1 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n2 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n3 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n4 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));
        let n5 = graph.add_node(Node::new(NodeEntity::Port(Entity::PLACEHOLDER), (50, 30)));

        graph.add_edge(n0, n3, ());
        graph.add_edge(n1, n4, ());
        graph.add_edge(n2, n5, ());

        graph[n1].adjacent_to = Some(n0);
        graph[n2].adjacent_to = Some(n1);

        layout_graph(&mut graph).unwrap();
        assert!(validate_layout(&graph));

        // Check adjacency constraints
        assert_eq!(graph[n0].rank, graph[n1].rank);
        assert_eq!(graph[n1].rank, graph[n2].rank);
        assert_eq!(graph[n0].order.unwrap() + 1, graph[n1].order.unwrap());
        assert_eq!(graph[n1].order.unwrap() + 1, graph[n2].order.unwrap());

        // Check for edge crossings
        let crossings = count_edge_crossings(&graph);
        assert_eq!(
            crossings, 0,
            "Expected 0 crossings, but found {}",
            crossings
        );
    }
}
