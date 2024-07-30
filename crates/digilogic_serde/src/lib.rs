mod circuitfile;
use circuitfile::*;

use bevy_ecs::prelude::*;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;
use std::collections::HashMap;

fn load_json(
    mut commands: Commands,
    mut ev_load: EventReader<LoadEvent>,
    mut ev_loaded: EventWriter<LoadedEvent>,
) {
    let symbols = SymbolRegistry::default();
    for ev in ev_load.read() {
        let result = CircuitFile::load(&ev.filename);
        match result {
            Ok(circuit) => {
                let circuit_id = translate_circuit(&mut commands, &circuit, &symbols).unwrap();
                ev_loaded.send(LoadedEvent {
                    filename: ev.filename.clone(),
                    circuit: CircuitID(circuit_id.clone()),
                });
                _ = circuit_id;
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

    for module in modules.iter() {
        let circuit_id = commands
            .spawn(CircuitBundle {
                ..Default::default()
            })
            .id();
        id_map.insert(module.id.clone(), circuit_id);

        for symbol in module.symbols.iter() {
            translate_symbol(symbol, &mut id_map, commands, circuit_id, symbols)?;
        }
    }

    Ok(id_map.get(&modules[0].id).unwrap().clone())
}

// TODO: a context struct would reduce the number of arguments
fn translate_symbol(
    symbol: &circuitfile::Symbol,
    id_map: &mut HashMap<Id, Entity>,
    commands: &mut Commands,
    circuit_id: Entity,
    symbols: &SymbolRegistry,
) -> Result<(), anyhow::Error> {
    let symbol_kind = if let Some(kind_name) = symbol.symbol_kind_name.as_ref() {
        symbols.get(kind_name)
    } else if let Some(_) = symbol.symbol_kind_id.as_ref() {
        return Err(anyhow::anyhow!(
            "Symbol {} has SymbolKindID but it's not supported",
            symbol.id.0
        ));
    } else {
        return Err(anyhow::anyhow!("Symbol {} has no SymbolKind", symbol.id.0));
    };
    if symbol_kind.is_none() {
        return Err(anyhow::anyhow!(
            "Symbol {} has unknown SymbolKind {}",
            symbol.id.0,
            symbol
                .symbol_kind_name
                .as_ref()
                .unwrap_or(&symbol.symbol_kind_id.as_ref().unwrap().0)
        ));
    }
    let symbol_kind = symbol_kind.unwrap();
    let symbol_id = symbol_kind.build(commands, circuit_id, symbol.number, BitWidth(1));
    id_map.insert(symbol.id.clone(), symbol_id);
    Ok(())
}

#[derive(Default)]
pub struct LoadSavePlugin;

impl bevy_app::Plugin for LoadSavePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.add_systems(bevy_app::Update, load_json);
    }
}
