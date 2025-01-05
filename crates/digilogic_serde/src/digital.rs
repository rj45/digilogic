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

fn translate_symbol(
    symbol: &circuitfile::VisualElement,
    commands: &mut Commands,
    circuit_id: Entity,
    pos_map: &mut HashMap<Vec2, PosEntry>,
    symbols: &SymbolRegistry,
) -> Result<(), anyhow::Error> {
    let mut symbol_builder = symbols.get(symbol.element_name.into());

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

    Ok(())
}
