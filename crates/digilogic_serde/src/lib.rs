mod circuitfile;
use circuitfile::*;

use bevy_ecs::prelude::*;
use bevy_hierarchy::BuildChildren;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::transform::Transform;
use digilogic_core::transform::TransformBundle;
use digilogic_core::transform::Vec2i;
use std::collections::HashMap;

fn load_json(
    mut commands: Commands,
    mut ev_load: EventReader<LoadEvent>,
    mut ev_loaded: EventWriter<LoadedEvent>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in ev_load.read() {
        let result = CircuitFile::load(&ev.filename);
        match result {
            Ok(circuit) => {
                let circuit_id = translate_circuit(&mut commands, &circuit, &symbols).unwrap();
                ev_loaded.send(LoadedEvent {
                    filename: ev.filename.clone(),
                    circuit: CircuitID(circuit_id.clone()),
                });
            }
            Err(e) => {
                // TODO: instead of this, send an ErrorEvent
                eprintln!("Error loading circuit {:?}: {:?}", ev.filename, e);
            }
        }
    }
}

fn translate_circuit(
    commands: &mut Commands,
    circuit: &CircuitFile,
    symbols: &SymbolRegistry,
) -> anyhow::Result<Entity> {
    let mut id_map = HashMap::new();
    let modules = &circuit.modules;
    let mut top_id: Option<Entity> = None;

    for module in modules.iter() {
        let circuit_id = commands
            .spawn(CircuitBundle {
                ..Default::default()
            })
            .id();

        for symbol in module.symbols.iter() {
            translate_symbol(symbol, &mut id_map, commands, circuit_id, symbols)?;
        }

        for net in module.nets.iter() {
            translate_net(net, &mut id_map, commands, circuit_id)?;
        }
        if let None = top_id {
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
) -> Result<(), anyhow::Error> {
    let symbol_builder = if let Some(kind_name) = symbol.symbol_kind_name.as_ref() {
        symbols.get(kind_name)
    } else if let Some(_) = symbol.symbol_kind_id.as_ref() {
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
        .position(Vec2i {
            x: symbol.position[0] as i32,
            y: symbol.position[1] as i32,
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
) -> Result<(), anyhow::Error> {
    let net_id = commands
        .spawn(NetBundle {
            net: digilogic_core::components::Net {
                endpoints: Default::default(),
            },
            name: Name(net.name.clone()),
            bit_width: BitWidth(1),
        })
        .set_parent(circuit_id)
        .id();

    for subnet in net.subnets.iter() {
        translate_subnet(subnet, id_map, commands, net_id)?;
    }

    Ok(())
}

fn translate_subnet(
    subnet: &circuitfile::Subnet,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    net_id: Entity,
) -> Result<(), anyhow::Error> {
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
) -> Result<(), anyhow::Error> {
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

    let mut endpoint_ent = commands.spawn(EndpointBundle {
        transform: TransformBundle {
            transform: Transform {
                translation: Vec2i {
                    x: endpoint.position[0] as i32,
                    y: endpoint.position[1] as i32,
                },
                ..Default::default()
            },
            ..Default::default()
        },
        ..Default::default()
    });
    endpoint_ent.set_parent(net_id);
    if let Some(port_id) = port_id {
        endpoint_ent.insert(PortID(port_id));
    }
    let endpoint_id = endpoint_ent.id();

    for waypoint in endpoint.waypoints.iter() {
        translate_waypoint(waypoint, id_map, commands, endpoint_id)?;
    }
    Ok(())
}

fn translate_waypoint(
    waypoint: &circuitfile::Waypoint,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    endpoint_id: Entity,
) -> Result<(), anyhow::Error> {
    let waypoint_id = commands
        .spawn(WaypointBundle {
            transform: TransformBundle {
                transform: Transform {
                    translation: Vec2i {
                        x: waypoint.position[0] as i32,
                        y: waypoint.position[1] as i32,
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
            ..Default::default()
        })
        .set_parent(endpoint_id)
        .id();
    id_map.insert(waypoint.id.clone(), waypoint_id);
    Ok(())
}

#[derive(Default)]
pub struct LoadSavePlugin;

impl bevy_app::Plugin for LoadSavePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.add_systems(bevy_app::Update, load_json);
    }
}
