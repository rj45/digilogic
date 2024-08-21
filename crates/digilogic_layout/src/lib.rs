use petgraph::algo::kosaraju_scc;
use petgraph::graph::{DiGraph, NodeIndex};
use petgraph::Direction;
use std::cmp::Ordering;
use std::collections::HashMap;
use std::collections::VecDeque;

#[derive(Debug, Clone)]
pub struct Node {
    pub size: (u32, u32),
    pub rank: Option<u32>,
    pub order: Option<u32>,
    pub x: Option<f64>,
    pub y: Option<f64>,
}

pub fn layout_graph(edges: Vec<(u32, u32)>, nodes: Vec<Node>) -> Result<DiGraph<Node, ()>, String> {
    let mut graph = create_graph(edges, nodes)?;

    break_cycles(&mut graph);
    assign_ranks(&mut graph);
    order_nodes_within_ranks(&mut graph);
    assign_x_coordinates(&mut graph);
    assign_y_coordinates(&mut graph);

    Ok(graph)
}

fn create_graph(edges: Vec<(u32, u32)>, nodes: Vec<Node>) -> Result<DiGraph<Node, ()>, String> {
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

fn break_cycles(graph: &mut DiGraph<Node, ()>) {
    // Remove self-loops first
    graph.retain_edges(|graph, edge| {
        let (source, target) = graph.edge_endpoints(edge).unwrap();
        source != target
    });

    let sccs = kosaraju_scc(graph as &DiGraph<Node, ()>);

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
                            if let Some(edge) = graph.find_edge(node, neighbor) {
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

fn assign_ranks(graph: &mut DiGraph<Node, ()>) {
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

    // Assign ranks to any remaining unranked nodes (should not happen if cycles are properly broken)
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
}

fn order_nodes_within_ranks(graph: &mut DiGraph<Node, ()>) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    for _ in 0..4 {
        // Perform a few iterations to improve the ordering
        for direction in &[Direction::Outgoing, Direction::Incoming] {
            for rank in 0..=max_rank {
                let nodes_at_rank: Vec<NodeIndex> = graph
                    .node_indices()
                    .filter(|&n| graph[n].rank == Some(rank))
                    .collect();

                let mut weighted_nodes: Vec<(NodeIndex, f64)> = nodes_at_rank
                    .iter()
                    .map(|&n| (n, calculate_median_position(graph, n, *direction)))
                    .collect();

                weighted_nodes.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(Ordering::Equal));

                // Assign orders, handling cases where all nodes have the same calculated position
                let mut current_order = 0;
                let mut prev_position = f64::NEG_INFINITY;
                for (node, position) in weighted_nodes {
                    if position > prev_position {
                        current_order = graph[node].order.unwrap_or(0);
                    }
                    graph[node].order = Some(current_order);
                    current_order += 1;
                    prev_position = position;
                }
            }
        }

        minimize_crossings(graph);
    }
}

fn calculate_median_position(
    graph: &DiGraph<Node, ()>,
    node: NodeIndex,
    direction: Direction,
) -> f64 {
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

fn minimize_crossings(graph: &mut DiGraph<Node, ()>) {
    let max_rank = graph
        .node_weights()
        .filter_map(|n| n.rank)
        .max()
        .unwrap_or(0);

    for rank in 0..max_rank {
        let mut improved = true;
        while improved {
            improved = false;
            let nodes_at_rank: Vec<NodeIndex> = graph
                .node_indices()
                .filter(|&n| graph[n].rank == Some(rank))
                .collect();

            for i in 0..nodes_at_rank.len() - 1 {
                let (n1, n2) = (nodes_at_rank[i], nodes_at_rank[i + 1]);
                if count_crossings(graph, n1, n2) > count_crossings(graph, n2, n1) {
                    let order1 = graph[n1].order.unwrap();
                    let order2 = graph[n2].order.unwrap();
                    graph[n1].order = Some(order2);
                    graph[n2].order = Some(order1);
                    improved = true;
                }
            }
        }
    }
}

// TODO: verify this is correct, seems sus
fn count_crossings(graph: &DiGraph<Node, ()>, left: NodeIndex, right: NodeIndex) -> usize {
    let left_edges: Vec<NodeIndex> = graph
        .neighbors_directed(left, Direction::Outgoing)
        .collect();
    let right_edges: Vec<NodeIndex> = graph
        .neighbors_directed(right, Direction::Outgoing)
        .collect();

    let mut crossings = 0;
    for &l in &left_edges {
        for &r in &right_edges {
            if (graph[l].order.unwrap_or(0) > graph[r].order.unwrap_or(0)) {
                crossings += 1;
            }
        }
    }
    crossings
}

fn assign_x_coordinates(graph: &mut DiGraph<Node, ()>) {
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

fn assign_y_coordinates(graph: &mut DiGraph<Node, ()>) {
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

    fn create_test_graph(edges: Vec<(u32, u32)>, node_count: usize) -> DiGraph<Node, ()> {
        let nodes = (0..node_count)
            .map(|_| Node {
                size: (50, 30),
                rank: None,
                order: None,
                x: None,
                y: None,
            })
            .collect();

        layout_graph(edges, nodes).unwrap()
    }

    fn check_acyclic(graph: &DiGraph<Node, ()>) -> bool {
        !petgraph::algo::is_cyclic_directed(graph)
    }

    fn check_ranks(graph: &DiGraph<Node, ()>) -> bool {
        graph.edge_indices().all(|e| {
            let (source, target) = graph.edge_endpoints(e).unwrap();
            graph[source].rank.unwrap() < graph[target].rank.unwrap()
        })
    }

    fn check_order_consistency(graph: &DiGraph<Node, ()>) -> bool {
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
                .all(|w| graph[w[0]].order.unwrap() < graph[w[1]].order.unwrap())
        })
    }

    fn check_x_coordinates(graph: &DiGraph<Node, ()>) -> bool {
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

    fn check_y_coordinates(graph: &DiGraph<Node, ()>) -> bool {
        graph
            .node_indices()
            .all(|n| graph[n].y.unwrap() == graph[n].rank.unwrap() as f64 * 100.0)
    }

    fn validate_layout(graph: &DiGraph<Node, ()>) -> bool {
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

        true
    }

    fn count_edge_crossings(graph: &DiGraph<Node, ()>) -> usize {
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
        graph: &DiGraph<Node, ()>,
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
}
