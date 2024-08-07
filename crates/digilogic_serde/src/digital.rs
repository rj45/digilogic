mod circuitfile;

use anyhow::{anyhow, Result};
use bevy_ecs::prelude::*;
use digilogic_core::bundles::CircuitBundle;
use digilogic_core::components::SymbolKind;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::transform::Vec2;
use std::fs::File;
use std::io::{BufReader, Write};
use std::path::Path;

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

    let circuit_id = commands
        .spawn(CircuitBundle {
            ..Default::default()
        })
        .id();

    for symbol in circuit.visual_elements.visual_element.iter() {
        translate_symbol(symbol, commands, circuit_id, symbols)?;
    }

    Ok(circuit_id)
}

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
    symbols: &SymbolRegistry,
) -> Result<(), anyhow::Error> {
    let mut symbol_builder = symbols.get(KIND_MAP[symbol.element_name as usize]);

    symbol_builder
        .position(Vec2 {
            x: symbol.pos.x.try_into()?,
            y: symbol.pos.y.try_into()?,
        })
        .build(commands, circuit_id);

    Ok(())
}
