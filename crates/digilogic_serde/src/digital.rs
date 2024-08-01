mod circuitfile;
use serde_xml_rs::from_reader;
use std::fs::File;
use std::io::Write;
use std::path::Path;

use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;

pub(crate) fn load_digital(
    mut commands: Commands,
    mut ev_load: EventReader<LoadEvent>,
    mut ev_loaded: EventWriter<LoadedEvent>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in ev_load.read() {
        if let Some(ext) = (ev.filename).extension() {
            if ext != "dig" {
                continue;
            }
        }

        println!("Loading digital circuit {:?}", ev.filename);

        if let None = ev.filename.as_path().parent() {
            eprintln!("Error getting parent directory of {:?}", ev.filename);
            continue;
        }
        let basedir = ev.filename.as_path().parent().unwrap();

        if let Ok(file) = File::open(&ev.filename) {
            let result = from_reader(file);
            match result {
                Ok(circuit) => {
                    let circuit_id =
                        translate_circuit(&mut commands, &circuit, &symbols, basedir).unwrap();
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
        } else {
            // TODO: instead of this, send an ErrorEvent
            eprintln!("Error opening file {:?}", ev.filename);
        }
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
