mod circuitfile;

use aery::prelude::*;
use anyhow::{anyhow, Result};
use bevy_ecs::prelude::*;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::transform::*;
use std::collections::{HashMap, HashSet, VecDeque};
use std::fs::File;
use std::io::{BufReader, Write};
use std::path::Path;

struct PosEntry {
    port: Option<Entity>,
    endpoint: Option<Entity>,
    wires: Vec<[Vec2; 2]>,
}

pub fn load_digital(
    commands: &mut Commands,
    filename: &Path,
    symbols: &SymbolRegistry,
) -> Result<Entity> {
    println!("Loading digital circuit {}", filename.display());

    let basedir = filename.parent().ok_or(anyhow!(
        "error getting parent directory of {}",
        filename.display(),
    ))?;

    let file = File::open(filename)?;
    let reader = BufReader::new(file);
    let circuit = serde_xml_rs::from_reader(reader)?;
    translate_circuit(commands, &circuit, symbols, basedir)
}

fn translate_circuit(
    commands: &mut Commands,
    circuit: &circuitfile::Circuit,
    symbols: &SymbolRegistry,
    basedir: &Path,
) -> Result<Entity> {
    File::create("test.json")?
        .write_all(serde_json::to_string_pretty(circuit).unwrap().as_bytes())?;

    let mut pos_map = HashMap::<Vec2, PosEntry>::default();

    let circuit_id = commands
        .spawn(CircuitBundle {
            ..Default::default()
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
                endpoint: None,
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
                        endpoint: None,
                        wires: vec![ends],
                    },
                );
            }
        }
    }

    let mut visited = HashSet::<Vec2>::default();
    let mut todo = VecDeque::<Vec2>::default();
    let mut endpoints = HashMap::<Vec2, Entity>::default();
    let mut junctions = HashSet::<Vec2>::default();

    // do a "flood fill" to find all connected ports and assign them to nets
    for (pos, _) in pos_map.iter() {
        if visited.contains(pos) {
            continue;
        }

        let net_id = commands
            .spawn(NetBundle {
                net: Net,
                name: Default::default(),
                bit_width: BitWidth(1), // TODO: Get bit width from somewhere
            })
            .set::<Child>(circuit_id)
            .id();

        todo.clear();
        todo.push_back(*pos);
        while let Some(pos) = todo.pop_front() {
            visited.insert(pos);

            if let Some(pos_entry) = pos_map.get(&pos) {
                if let Some(port) = pos_entry.port {
                    // Connect port to net
                    let endpoint_id = commands
                        .spawn(EndpointBundle {
                            endpoint: Endpoint,
                            transform: TransformBundle {
                                transform: Transform {
                                    translation: pos,
                                    ..Default::default()
                                },
                                ..Default::default()
                            },
                            visibility: Default::default(),
                            bounds: Default::default(),
                        })
                        .insert(PortID(port))
                        .set::<Child>(net_id)
                        .id();
                    endpoints.insert(pos, endpoint_id);
                } else if pos_entry.wires.len() > 2 {
                    // junction here, save it for later
                    junctions.insert(pos);
                }

                for wire in pos_entry.wires.iter() {
                    for end in wire.iter() {
                        if !visited.contains(end) {
                            todo.push_back(*end);
                        }
                    }
                }
            }
        }
    }

    // TODO: I feel like this should not be necessary somehow. I should be able to
    // alter the endpoint field from within the previous loop, but it won't let me.
    for (pos, endpoint_id) in endpoints.iter() {
        pos_map.get_mut(pos).unwrap().endpoint = Some(*endpoint_id);
    }

    Ok(())
}
