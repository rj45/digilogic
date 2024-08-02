mod circuitfile;
use serde_xml_rs::from_reader;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::path::PathBuf;

use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;

pub(crate) fn load_digital(
    commands: &mut Commands,
    filename: PathBuf,
    ev_loaded: &mut EventWriter<LoadedEvent>,
    symbols: &SymbolRegistry,
) {
    println!("Loading digital circuit {}", filename.display());

    if let None = filename.as_path().parent() {
        eprintln!("Error getting parent directory of {}", filename.display());
        return;
    }
    let basedir = filename.as_path().parent().unwrap();

    if let Ok(file) = File::open(&filename) {
        let result = from_reader(file);
        match result {
            Ok(circuit) => {
                let circuit_id = translate_circuit(commands, &circuit, symbols, basedir).unwrap();
                ev_loaded.send(LoadedEvent {
                    filename: filename,
                    circuit: CircuitID(circuit_id.clone()),
                });
            }
            Err(e) => {
                // TODO: instead of this, send an ErrorEvent
                eprintln!("Error loading circuit {}: {:?}", filename.display(), e);
            }
        }
    } else {
        // TODO: instead of this, send an ErrorEvent
        eprintln!("Error opening file {}", filename.display());
    }
}

fn translate_circuit(
    commands: &mut Commands,
    circuit: &circuitfile::Circuit,
    symbols: &SymbolRegistry,
    basedir: &Path,
) -> anyhow::Result<Entity> {
    File::create("test.json")?
        .write_all(serde_json::to_string_pretty(circuit).unwrap().as_bytes())?;

    Ok(Entity::PLACEHOLDER)
}
