mod circuitfile;
use serde_xml_rs::from_reader;
use std::fs::File;

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
        if !(ev.filename).ends_with(".dig") {
            continue;
        }
        if let Ok(file) = File::open(&ev.filename) {
            let result = from_reader(file);
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
) -> anyhow::Result<Entity> {
    Ok(Entity::PLACEHOLDER)
}
