mod netlist;

use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_log::info;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::symbol::PortInfo;
use digilogic_core::symbol::SymbolRegistry;

use digilogic_core::transform::Directions;
use digilogic_core::transform::InheritTransform;
use digilogic_core::transform::Transform;
use digilogic_core::transform::TransformBundle;
use digilogic_core::transform::Vec2;
use digilogic_core::visibility::VisibilityBundle;
use digilogic_core::Fixed;
use digilogic_core::HashMap;
use digilogic_core::HashSet;
use digilogic_core::SharedStr;
use digilogic_layout::{Graph, Node, NodeEntity};
use petgraph::graph::NodeIndex;
use std::path::Path;

struct NetBit {
    ports: Vec<PortInfo>,
}

#[derive(Debug, Default)]
struct MetaGraph {
    graph: Graph,
    entity_ids: HashMap<NodeEntity, NodeIndex>,
}

pub fn load_yosys(
    commands: &mut Commands,
    filename: &Path,
    symbols: &SymbolRegistry,
) -> anyhow::Result<Entity> {
    info!("loading Yosys circuit {}", filename.display());

    let netlist = netlist::Netlist::load(filename)?;
    translate_netlist(commands, &netlist, symbols)
}

fn translate_netlist(
    commands: &mut Commands,
    netlist: &netlist::Netlist,
    symbols: &SymbolRegistry,
) -> anyhow::Result<Entity> {
    let mut bit_map = HashMap::new();
    let modules = &netlist.modules;
    let mut top_id: Option<Entity> = None;

    for (name, module) in modules.iter() {
        let circuit_id = commands
            .spawn(CircuitBundle {
                ..Default::default()
            })
            .insert(Name(name.clone()))
            .id();

        let mut graph = MetaGraph::default();

        for (name, port) in module.ports.iter() {
            translate_module_port(name, port, &mut bit_map, commands, circuit_id, symbols)?;
        }

        for (name, cell) in module.cells.iter() {
            translate_cell(name, cell, &mut bit_map, commands, circuit_id, symbols)?;
        }

        for (name, net_info) in module.net_names.iter() {
            translate_net(
                name,
                net_info,
                &mut bit_map,
                commands,
                circuit_id,
                &mut graph,
            )?;
        }

        layout_circuit(commands, &mut graph)?;

        // TODO: the top module is not necessarily the last one... need to figure out
        // how to determine the top module
        if top_id.is_none() {
            top_id = Some(circuit_id);
        }
    }

    Ok(top_id.unwrap())
}

fn translate_module_port(
    name: &SharedStr,
    port: &netlist::Port,
    bit_map: &mut HashMap<usize, NetBit>,
    commands: &mut Commands,
    circuit_id: Entity,
    symbols: &SymbolRegistry,
) -> anyhow::Result<()> {
    let mut symbol_builder = match (port.direction) {
        netlist::PortDirection::Input => symbols.get(SymbolKind::In),
        netlist::PortDirection::Output => symbols.get(SymbolKind::Out),
        netlist::PortDirection::InOut => todo!(),
    };
    symbol_builder
        .name(name.clone())
        .build(commands, circuit_id);
    if let Some(port_info) = symbol_builder.ports().first() {
        for signal in port.bits.iter() {
            if let netlist::Signal::Net(bit) = signal {
                let net_bit = bit_map.entry(*bit).or_insert(NetBit { ports: Vec::new() });
                net_bit.ports.push(port_info.clone());
            } else {
                // figure out how to hard code a bit to a specific value
                todo!();
            }
        }
    }

    Ok(())
}

fn translate_cell(
    name: &SharedStr,
    cell: &netlist::Cell,
    bit_map: &mut HashMap<usize, NetBit>,
    commands: &mut Commands,
    circuit_id: Entity,
    symbols: &SymbolRegistry,
) -> anyhow::Result<()> {
    let mut symbol_builder = match cell.cell_type {
        netlist::CellType::Not => symbols.get(SymbolKind::Not),
        netlist::CellType::Pos => todo!(),
        netlist::CellType::Neg => todo!(),
        netlist::CellType::ReduceAnd => todo!(),
        netlist::CellType::ReduceOr => todo!(),
        netlist::CellType::ReduceXor => todo!(),
        netlist::CellType::ReduceXnor => todo!(),
        netlist::CellType::ReduceBool => todo!(),
        netlist::CellType::LogicNot => todo!(),
        netlist::CellType::And => symbols.get(SymbolKind::And),
        netlist::CellType::Or => symbols.get(SymbolKind::Or),
        netlist::CellType::Xor => symbols.get(SymbolKind::Xor),
        netlist::CellType::Xnor => todo!(),
        netlist::CellType::Shl => todo!(),
        netlist::CellType::Sshl => todo!(),
        netlist::CellType::Shr => todo!(),
        netlist::CellType::Sshr => todo!(),
        netlist::CellType::LogicAnd => todo!(),
        netlist::CellType::LogicOr => todo!(),
        netlist::CellType::EqX => todo!(),
        netlist::CellType::NeX => todo!(),
        netlist::CellType::Pow => todo!(),
        netlist::CellType::Lt => todo!(),
        netlist::CellType::Le => todo!(),
        netlist::CellType::Eq => todo!(),
        netlist::CellType::Ne => todo!(),
        netlist::CellType::Ge => todo!(),
        netlist::CellType::Gt => todo!(),
        netlist::CellType::Add => todo!(),
        netlist::CellType::Sub => todo!(),
        netlist::CellType::Mul => todo!(),
        netlist::CellType::Div => todo!(),
        netlist::CellType::Mod => todo!(),
        netlist::CellType::DivFloor => todo!(),
        netlist::CellType::ModFloor => todo!(),
        netlist::CellType::Mux => todo!(),
        netlist::CellType::Pmux => todo!(),
        netlist::CellType::TriBuf => todo!(),
        netlist::CellType::Sr => todo!(),
        netlist::CellType::Dff => todo!(),
        netlist::CellType::Dffe => todo!(),
        netlist::CellType::Sdff => todo!(),
        netlist::CellType::Sdffe => todo!(),
        netlist::CellType::Sdffce => todo!(),
        netlist::CellType::Dlatch => todo!(),
        netlist::CellType::MemRdV2 => todo!(),
        netlist::CellType::MemWrV2 => todo!(),
        netlist::CellType::MemInitV2 => todo!(),
        netlist::CellType::MemV2 => todo!(),
        netlist::CellType::Unknown(_) => todo!(),
    };

    symbol_builder
        .name(name.clone())
        .build(commands, circuit_id);

    for (port_name, signals) in cell.connections.iter() {
        if let Some(port_info) = symbol_builder
            .ports()
            .iter()
            .find(|p| *p.name == *port_name)
        {
            for signal in signals.iter() {
                if let netlist::Signal::Net(bit) = signal {
                    let net_bit = bit_map.entry(*bit).or_insert(NetBit { ports: Vec::new() });
                    net_bit.ports.push(port_info.clone());
                } else {
                    // figure out how to hard code a bit to a specific value
                    todo!();
                }
            }
        } else {
            // port names do not match what yosys produces
            todo!();
        }
    }

    Ok(())
}

fn translate_net(
    name: &SharedStr,
    net_info: &netlist::NetNameOpts,
    bit_map: &mut HashMap<usize, NetBit>,
    commands: &mut Commands,
    circuit_id: Entity,
    graph: &mut MetaGraph,
) -> anyhow::Result<()> {
    let net_id = commands
        .spawn(NetBundle {
            net: Net,
            name: Name(name.clone()),
            bit_width: BitWidth(net_info.bits.len() as u8),
            visibility: VisibilityBundle::default(),
        })
        .set::<Child>(circuit_id)
        .id();

    let mut ports = HashSet::new();
    let mut port_infos = Vec::new();

    for signal in net_info.bits.iter() {
        if let netlist::Signal::Net(bit) = signal {
            if let Some(bit_info) = bit_map.get(bit) {
                for port in bit_info.ports.iter() {
                    if !ports.contains(&port.id) {
                        port_infos.push(port.clone());
                    }
                    ports.insert(port.id);
                }
            } else {
                // bit not found in the bit map
                todo!();
            }
        } else {
            // figure out how to hard code a bit to a specific value
            todo!();
        }
    }

    // check all bits of the net have the same port list
    for signal in net_info.bits.iter() {
        if let netlist::Signal::Net(bit) = signal {
            if ports.len() != bit_map.get(bit).unwrap().ports.len() {
                return Err(anyhow::anyhow!(
                    "net {} has different port lists for its bits",
                    name
                ));
            }
        }
    }

    let mut listeners: Vec<PortInfo> = Vec::new();
    let mut drivers: Vec<PortInfo> = Vec::new();

    for port in port_infos.iter() {
        commands
            .spawn(EndpointBundle {
                endpoint: Endpoint,
                ..Default::default()
            })
            .insert(PortID(port.id))
            .set::<Child>(net_id)
            .set::<InheritTransform>(port.id);

        if !graph
            .entity_ids
            .contains_key(&NodeEntity::Symbol(port.symbol))
        {
            let ne = NodeEntity::Symbol(port.symbol);
            let id = graph.graph.add_node(Node::new(ne, (80, 80)));
            graph.entity_ids.insert(ne, id);
        }
        if !graph.entity_ids.contains_key(&NodeEntity::Port(port.id)) {
            let ne = NodeEntity::Port(port.id);
            let id = graph.graph.add_node(Node::new(ne, (5, 5)));
            graph.entity_ids.insert(ne, id);
        }

        match port.direction {
            Directions::NEG_X => {
                listeners.push(port.clone());
            }
            Directions::POS_X => {
                drivers.push(port.clone());
            }
            _ => {
                // figure out how to handle in/out/top/bottom ports
                todo!();
            }
        }
    }

    let driver_junction = if drivers.len() > 1 {
        let junction = NodeEntity::DriverJunction(net_id);
        let junction_id = graph.graph.add_node(Node::new(junction, (3, 3)));
        graph.entity_ids.insert(junction, junction_id);
        Some(junction)
    } else {
        None
    };

    let listener_junction = if listeners.len() > 1 {
        let junction = NodeEntity::ListenerJunction(net_id);
        let junction_id = graph.graph.add_node(Node::new(junction, (3, 3)));
        graph.entity_ids.insert(junction, junction_id);
        Some(junction)
    } else {
        None
    };

    // add a single edge between the junctions if they both exist
    if let (Some(driver_junction), Some(listener_junction)) = (driver_junction, listener_junction) {
        graph.graph.add_edge(
            *graph.entity_ids.get(&driver_junction).unwrap(),
            *graph.entity_ids.get(&listener_junction).unwrap(),
            (),
        );
    }

    // Set up a graph from each driver's symbol to the driver's port, then optionally to the
    // driver junction, then optionally to the listener junction, then to each listener's port,
    // then to the listener's symbol.
    for driver in drivers.iter() {
        let driver_symbol_id = graph
            .entity_ids
            .get(&NodeEntity::Symbol(driver.symbol))
            .unwrap();
        let mut driver_node = NodeEntity::Port(driver.id);
        let mut driver_id = graph.entity_ids.get(&driver_node).unwrap();

        // driver's symbol -> driver's port
        graph.graph.add_edge(*driver_symbol_id, *driver_id, ());

        if let Some(junction) = driver_junction {
            let junction_id = graph.entity_ids.get(&junction).unwrap();
            // driver's port -> driver junction
            graph.graph.add_edge(*driver_id, *junction_id, ());
            driver_id = junction_id;
            driver_node = junction;
        }

        if let Some(junction) = listener_junction {
            let junction_id = graph.entity_ids.get(&junction).unwrap();
            if driver_junction.is_none() {
                // only add an edge here if there is no driver junction
                // driver port -> listener junction
                graph.graph.add_edge(*driver_id, *junction_id, ());
            }
            driver_node = junction;
            driver_id = junction_id;
        }

        for listener in listeners.iter() {
            let listener_node = NodeEntity::Port(listener.id);
            let listener_id = graph.entity_ids.get(&listener_node).unwrap();
            // listener's port / listener junction / driver junction -> listener's port
            graph.graph.add_edge(*driver_id, *listener_id, ());

            let listener_symbol_node = NodeEntity::Symbol(listener.symbol);
            let listener_symbol_id = graph.entity_ids.get(&listener_symbol_node).unwrap();
            // listener's port -> listener's symbol
            graph.graph.add_edge(*listener_id, *listener_symbol_id, ());
        }
    }

    Ok(())
}

fn layout_circuit(commands: &mut Commands, graph: &mut MetaGraph) -> anyhow::Result<()> {
    // std::fs::write(
    //     "graph.dot",
    //     format!(
    //         "{:?}",
    //         petgraph::dot::Dot::with_config(&graph.graph, &[petgraph::dot::Config::EdgeNoLabel])
    //     ),
    // )?;

    digilogic_layout::layout_graph(&mut graph.graph);

    let mut max_x: f64 = 0.;
    let mut max_y: f64 = 0.;

    for node in graph.graph.node_weights() {
        let x = node.x.unwrap();
        let y = node.y.unwrap();
        if x > max_x {
            max_x = x;
        }
        if y > max_y {
            max_y = y;
        }
    }

    for node in graph.graph.node_weights() {
        // swap x and y
        let y = node.x.unwrap();
        let x = node.y.unwrap();
        match node.entity {
            NodeEntity::Port(_port_id) => {
                // ignore
            }
            NodeEntity::Symbol(symbol_id) => {
                let transform = TransformBundle {
                    transform: Transform {
                        translation: Vec2 {
                            x: Fixed::try_from(x * 1.).unwrap(),
                            y: Fixed::try_from(y * 1.).unwrap(),
                        },
                        ..Default::default()
                    },
                    ..Default::default()
                };
                commands.entity(symbol_id).insert(transform);
            }
            NodeEntity::DriverJunction(_net_id) => {
                // TODO: place waypoints?
            }
            NodeEntity::ListenerJunction(_net_id) => {
                // TODO: place waypoints?
            }
        }
    }

    Ok(())
}
