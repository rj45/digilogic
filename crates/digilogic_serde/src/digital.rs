mod circuitfile;

use aery::prelude::*;
use anyhow::{bail, Result};
use bevy_ecs::prelude::*;
use bevy_log::info;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::transform::*;
use digilogic_core::visibility::VisibilityBundle;
use digilogic_core::Fixed;
use digilogic_core::{fixed, HashMap, HashSet};
use std::cell::Cell;
use std::fs::File;
use std::io::{BufReader, Write};
use std::num::NonZeroU8;
use std::path::Path;

struct PosEntry {
    port: Option<Entity>,
    endpoint: Cell<Option<Entity>>,
    wires: Vec<[Vec2; 2]>,
}

pub fn load_digital(
    commands: &mut Commands,
    filename: &Path,
    symbols: &SymbolRegistry,
) -> Result<Entity> {
    info!("loading Digital circuit {}", filename.display());

    let Some(basedir) = filename.parent() else {
        bail!("error getting parent directory of {}", filename.display(),);
    };

    let Some(name) = filename.file_stem() else {
        bail!("error getting file name of {}", filename.display(),);
    };

    let file = File::open(filename)?;
    let reader = BufReader::new(file);
    let circuit = serde_xml_rs::from_reader(reader)?;

    translate_circuit(
        commands,
        &circuit,
        symbols,
        basedir,
        &name.to_string_lossy(),
    )
}

fn translate_circuit(
    commands: &mut Commands,
    circuit: &circuitfile::Circuit,
    symbols: &SymbolRegistry,
    basedir: &Path,
    name: &str,
) -> Result<Entity> {
    File::create("test.json")?
        .write_all(serde_json::to_string_pretty(circuit).unwrap().as_bytes())?;

    let mut pos_map = HashMap::<Vec2, PosEntry>::default();

    let circuit_id = commands
        .spawn(CircuitBundle {
            circuit: Circuit,
            name: Name(name.into()),
        })
        .id();

    for symbol in circuit.visual_elements.visual_element.iter() {
        translate_symbol(symbol, commands, circuit_id, &mut pos_map, symbols)?;
    }

    translate_wires(commands, circuit, circuit_id, &mut pos_map)?;

    Ok(circuit_id)
}

// NOTE: Must be kept in sync with ElementName!
const KIND_MAP: [SymbolKind; 6] = [
    SymbolKind::And,
    SymbolKind::Or,
    SymbolKind::Xor,
    SymbolKind::Not,
    SymbolKind::In,
    SymbolKind::Out,
];

fn translate_symbol(
    symbol: &circuitfile::VisualElement,
    commands: &mut Commands,
    circuit_id: Entity,
    pos_map: &mut HashMap<Vec2, PosEntry>,
    symbols: &SymbolRegistry,
) -> Result<(), anyhow::Error> {
    let mut symbol_builder = symbols.get(KIND_MAP[symbol.element_name as usize]);

    let pos = Vec2 {
        x: symbol.pos.x.try_into()?,
        y: symbol.pos.y.try_into()?,
    };

    symbol_builder.position(pos).build(commands, circuit_id);

    for port in symbol_builder.ports().iter() {
        pos_map.insert(
            pos + port.position,
            PosEntry {
                port: Some(port.id),
                endpoint: Cell::new(None),
                wires: vec![],
            },
        );
    }

    Ok(())
}

fn translate_wires(
    commands: &mut Commands,
    circuit: &circuitfile::Circuit,
    circuit_id: Entity,
    pos_map: &mut HashMap<Vec2, PosEntry>,
) -> Result<(), anyhow::Error> {
    // at this point, pos_map contains only the ports.
    // add the wire ends to pos_map also.
    for wire in circuit.wires.wire.iter() {
        let ends = [
            Vec2 {
                x: wire.p1.x.try_into()?,
                y: wire.p1.y.try_into()?,
            },
            Vec2 {
                x: wire.p2.x.try_into()?,
                y: wire.p2.y.try_into()?,
            },
        ];

        for end in ends.iter() {
            if let Some(pos_entry) = pos_map.get_mut(end) {
                pos_entry.wires.push(ends);
            } else {
                pos_map.insert(
                    *end,
                    PosEntry {
                        port: None,
                        endpoint: Cell::new(None),
                        wires: vec![ends],
                    },
                );
            }
        }
    }

    let mut visited = HashSet::<Vec2>::default();
    let mut todo = Vec::<Vec2>::default();
    let mut junctions = HashSet::<(Entity, Vec2)>::default();
    let mut net_endpoints = HashMap::<Entity, Vec<Vec2>>::default();

    // do a "flood fill" to find all connected ports and assign them to nets
    for pos in pos_map.keys() {
        if visited.contains(pos) {
            continue;
        }

        let net_id = commands
            .spawn(NetBundle {
                net: Net,
                name: Default::default(),
                bit_width: BitWidth(NonZeroU8::MIN), // TODO: Get bit width from somewhere
                visibility: VisibilityBundle::default(),
            })
            .set::<Child>(circuit_id)
            .id();

        todo.clear();
        todo.push(*pos);
        while let Some(pos) = todo.pop() {
            visited.insert(pos);

            if let Some(pos_entry) = pos_map.get(&pos) {
                if let Some(port) = pos_entry.port {
                    // Connect port to net
                    let endpoint_id = commands
                        .spawn(EndpointBundle {
                            bounds: BoundingBoxBundle {
                                bounding_box: BoundingBox::from_half_size(fixed!(2.5), fixed!(2.5)),
                                ..Default::default()
                            },
                            ..Default::default()
                        })
                        .insert(PortID(port))
                        .set::<Child>(net_id)
                        // Remember to disconnect this when disconnecting from the port.
                        .set::<InheritTransform>(port)
                        .id();

                    commands.entity(port).insert(NetID(net_id));

                    pos_entry.endpoint.set(Some(endpoint_id));
                    if let Some(endpoints) = net_endpoints.get_mut(&net_id) {
                        endpoints.push(pos);
                    } else {
                        net_endpoints.insert(net_id, vec![pos]);
                    }
                } else if pos_entry.wires.len() > 2 {
                    // junction here, save it for later
                    junctions.insert((net_id, pos));
                }

                for wire in pos_entry.wires.iter() {
                    for end in wire.iter() {
                        if !visited.contains(end) {
                            todo.push(*end);
                        }
                    }
                }
            }
        }
    }

    // now that we have all the endpoints, we can attach waypoints to them near junctions
    let mut endpoints = Vec::<(Entity, Vec2)>::default();
    visited.clear();
    for (net_id, junction_pos) in junctions.iter() {
        let pos_entry = pos_map.get(junction_pos).unwrap();
        for wire in pos_entry.wires.iter() {
            let other_end = if wire[0] == *junction_pos {
                wire[1]
            } else {
                wire[0]
            };

            // waypoint is positioned at the midpoint of the wire segment going
            // into the junction
            let waypoint_pos = Vec2 {
                x: (junction_pos.x + other_end.x) / fixed!(2),
                y: (junction_pos.y + other_end.y) / fixed!(2),
            };

            // flood fill from the other end to find all connected endpoints
            endpoints.clear();
            todo.clear();
            todo.push(other_end);
            while let Some(pos) = todo.pop() {
                visited.insert(pos);

                if let Some(pos_entry) = pos_map.get(&pos) {
                    for wire in pos_entry.wires.iter() {
                        for end in wire.iter() {
                            if !visited.contains(end) {
                                todo.push(*end);
                            }
                        }
                    }
                    if let Some(endpoint) = pos_entry.endpoint.get() {
                        endpoints.push((endpoint, pos));
                    }
                }
            }

            let mut best_endpoint = None;
            let mut best_distance = Fixed::MAX;
            if endpoints.len() == 1 {
                // only one endpoint the waypoint could be attached to, so attach it
                best_endpoint = Some(endpoints[0].0);
            } else {
                // multiple endpoints, so do some sleuthing to figure out which one to attach to

                // find the two endpoints that are furthest apart, those will be the root wire endpoints
                let nets_endpoints = net_endpoints.get(net_id).unwrap();
                let mut furthest_endpoints = None;
                let mut furthest_distance = fixed!(0);
                for endpoint_pos1 in nets_endpoints.iter() {
                    for endpoint_pos2 in nets_endpoints.iter() {
                        let distance = endpoint_pos1.manhatten_distance_to(*endpoint_pos2);
                        if distance > furthest_distance {
                            furthest_distance = distance;
                            furthest_endpoints = Some((*endpoint_pos1, *endpoint_pos2));
                        }
                    }
                }

                // check if the root wire endpoints are in the list of endpoints and pick the closest one
                for (endpoint, endpoint_pos) in endpoints.iter() {
                    if *endpoint_pos == furthest_endpoints.unwrap().0
                        || *endpoint_pos == furthest_endpoints.unwrap().1
                    {
                        let distance = waypoint_pos.manhatten_distance_to(*endpoint_pos);
                        if distance < best_distance {
                            best_distance = distance;
                            best_endpoint = Some(*endpoint);
                        }
                    }
                }

                if best_endpoint.is_none() {
                    // no root endpoints found, just attach to the closest endpoint
                    for (endpoint, endpoint_pos) in endpoints.iter() {
                        let distance = waypoint_pos.manhatten_distance_to(*endpoint_pos);
                        if distance < best_distance {
                            best_distance = distance;
                            best_endpoint = Some(*endpoint);
                        }
                    }
                }
            }
            if let Some(best_endpoint) = best_endpoint {
                commands
                    .spawn(WaypointBundle {
                        waypoint: Waypoint,
                        transform: TransformBundle {
                            transform: Transform {
                                translation: waypoint_pos,
                                ..Default::default()
                            },
                            ..Default::default()
                        },
                        bounds: BoundingBoxBundle {
                            bounding_box: BoundingBox::from_half_size(fixed!(2.5), fixed!(2.5)),
                            ..Default::default()
                        },
                        ..Default::default()
                    })
                    .set::<Child>(best_endpoint);
            }
        }
    }

    Ok(())
}
