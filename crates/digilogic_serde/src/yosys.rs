mod netlist;

use aery::prelude::*;
use bevy_ecs::prelude::*;
use bevy_log::info;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::symbol::SymbolRegistry;

use digilogic_core::transform::InheritTransform;
use digilogic_core::visibility::VisibilityBundle;
use digilogic_core::HashMap;
use digilogic_core::HashSet;
use digilogic_core::SharedStr;
use std::path::Path;

struct NetBit {
    ports: Vec<Entity>,
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

        for (name, port) in module.ports.iter() {
            translate_module_port(name, port, &mut bit_map, commands, circuit_id, symbols)?;
        }

        for (name, cell) in module.cells.iter() {
            translate_cell(name, cell, &mut bit_map, commands, circuit_id, symbols)?;
        }

        for (name, net_info) in module.net_names.iter() {
            translate_net(name, net_info, &mut bit_map, commands, circuit_id)?;
        }

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
                net_bit.ports.push(port_info.id);
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
                    net_bit.ports.push(port_info.id);
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

    for signal in net_info.bits.iter() {
        if let netlist::Signal::Net(bit) = signal {
            if let Some(bit_info) = bit_map.get(bit) {
                for port in bit_info.ports.iter() {
                    ports.insert(*port);
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

    for port in ports.iter() {
        commands
            .spawn(EndpointBundle {
                endpoint: Endpoint,
                ..Default::default()
            })
            .insert(PortID(*port))
            .set::<Child>(net_id)
            .set::<InheritTransform>(*port);
    }

    Ok(())
}
