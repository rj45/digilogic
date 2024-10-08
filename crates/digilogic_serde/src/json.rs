mod circuitfile;
use circuitfile::*;

use aery::prelude::*;
use anyhow::{bail, Result};
use bevy_ecs::prelude::*;
use bevy_log::info;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::transform::*;
use digilogic_core::visibility::VisibilityBundle;
use digilogic_core::HashMap;
use std::num::NonZeroU8;
use std::path::Path;

pub fn load_json(
    commands: &mut Commands,
    filename: &Path,
    symbols: &SymbolRegistry,
) -> Result<Entity> {
    info!("loading Digilogic circuit {}", filename.display());

    let Some(name) = filename.file_stem() else {
        bail!("error getting file name of {}", filename.display(),);
    };

    let circuit = CircuitFile::load(filename)?;
    translate_circuit(commands, &circuit, symbols, &name.to_string_lossy())
}

fn translate_circuit(
    commands: &mut Commands,
    circuit: &CircuitFile,
    symbols: &SymbolRegistry,
    name: &str,
) -> Result<Entity> {
    let mut id_map = HashMap::new();
    let modules = &circuit.modules;
    let mut top_id: Option<Entity> = None;

    for module in modules.iter() {
        let circuit_id = commands
            .spawn(CircuitBundle {
                circuit: Circuit,
                name: Name(name.into()),
            })
            .id();

        for symbol in module.symbols.iter() {
            translate_symbol(symbol, &mut id_map, commands, circuit_id, symbols)?;
        }

        for net in module.nets.iter() {
            translate_net(net, &mut id_map, commands, circuit_id)?;
        }
        if top_id.is_none() {
            top_id = Some(circuit_id);
        }
    }

    Ok(top_id.unwrap())
}

// TODO: a context struct would reduce the number of arguments
fn translate_symbol(
    symbol: &circuitfile::Symbol,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    circuit_id: Entity,
    symbols: &SymbolRegistry,
) -> Result<()> {
    let symbol_builder = if let Some(kind_name) = symbol.symbol_kind_name.as_ref() {
        symbols.get_by_name(kind_name)
    } else if symbol.symbol_kind_id.is_some() {
        return Err(anyhow::anyhow!(
            "Symbol {} has SymbolKindID but it's not supported",
            symbol.id.0
        ));
    } else {
        return Err(anyhow::anyhow!("Symbol {} has no SymbolKind", symbol.id.0));
    };
    if symbol_builder.is_none() {
        return Err(anyhow::anyhow!(
            "Symbol {} has unknown SymbolKind {}",
            symbol.id.0,
            symbol
                .symbol_kind_name
                .as_ref()
                .unwrap_or(&symbol.symbol_kind_id.as_ref().unwrap().0)
        ));
    }
    let mut symbol_builder = symbol_builder.unwrap();
    symbol_builder
        .designator_number(symbol.number)
        .position(Vec2 {
            x: symbol.position[0],
            y: symbol.position[1],
        })
        .build(commands, circuit_id);
    for port in symbol_builder.ports().iter() {
        let symbol_name_pair = format!("{}:{}", symbol.id.0, port.name);
        id_map.insert(Id(symbol_name_pair.into()), port.id);
    }

    Ok(())
}

fn translate_net(
    net: &circuitfile::Net,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    circuit_id: Entity,
) -> Result<()> {
    let net_id = commands
        .spawn(NetBundle {
            net: Net,
            name: Name(net.name.clone()),
            bit_width: BitWidth(NonZeroU8::MIN),
            visibility: VisibilityBundle::default(),
        })
        .set::<Child>(circuit_id)
        .id();

    for subnet in net.subnets.iter() {
        translate_subnet(subnet, id_map, commands, net_id)?;
    }

    Ok(())
}

fn translate_subnet(
    subnet: &Subnet,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    net_id: Entity,
) -> Result<()> {
    for endpoint in subnet.endpoints.iter() {
        translate_endpoint(endpoint, id_map, commands, net_id)?;
    }
    Ok(())
}

fn translate_endpoint(
    endpoint: &circuitfile::Endpoint,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    net_id: Entity,
) -> Result<()> {
    let portref = &endpoint.portref;

    let port_id = if let Some(port_name) = portref.port_name.as_ref() {
        let port_name_pair = format!("{}:{}", portref.symbol.0, port_name);
        if let Some(id) = id_map.get(&Id(port_name_pair.into())) {
            Some(*id)
        } else {
            return Err(anyhow::anyhow!(
                "Endpoint {} references unknown port {} on {}",
                endpoint.id.0,
                port_name,
                portref.symbol.0
            ));
        }
    } else {
        None
    };

    let endpoint_id = commands
        .spawn(EndpointBundle {
            transform: TransformBundle {
                transform: Transform {
                    translation: Vec2 {
                        x: endpoint.position[0],
                        y: endpoint.position[1],
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
            ..Default::default()
        })
        .set::<Child>(net_id)
        .id();

    if let Some(port_id) = port_id {
        commands
            .entity(endpoint_id)
            .insert(PortID(port_id))
            .insert(Transform::default());

        // Remember to disconnect this when disconnecting from the port.
        commands
            .entity(endpoint_id)
            .set::<InheritTransform>(port_id);
        commands.entity(port_id).insert(NetID(net_id));
    }

    Ok(())
}
