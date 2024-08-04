mod circuitfile;

use anyhow::{anyhow, Result};
use bevy_ecs::prelude::*;
use digilogic_core::symbol::SymbolRegistry;
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

    Ok(Entity::PLACEHOLDER)
}
