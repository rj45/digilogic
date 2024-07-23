use std::collections::HashMap;

use bevy_ecs::prelude::*;
use bevy_hierarchy::BuildChildren;
use digilogic_core::bundles::*;
use digilogic_core::components::*;
use digilogic_core::events::{LoadEvent, LoadedEvent};

mod circuitfile;
use circuitfile::*;

pub fn load_json(
    mut commands: Commands,
    symbol_kinds_q: Query<(EntityRef, &Name), With<SymbolKind>>,
    mut ev_load: EventReader<LoadEvent>,
    // mut ev_loaded: EventWriter<LoadedEvent>,
) {
    for ev in ev_load.read() {
        let result = CircuitFile::load(&ev.filename);
        match result {
            Ok(circuit) => {
                let circuit_id =
                    translate_circuit(&mut commands, &symbol_kinds_q, &circuit).unwrap();
                // ev_loaded.send(LoadedEvent {
                //     filename: ev.filename.clone(),
                //     circuit: CircuitID(circuit_id.clone()),
                // });
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
    symbol_kinds_q: &Query<(EntityRef, &Name), With<SymbolKind>>,
    circuit: &CircuitFile,
) -> anyhow::Result<Entity> {
    let mut id_map = HashMap::new();
    let modules = &circuit.modules;

    for module in modules.iter() {
        let circuit_symbol_kind_id = commands
            .spawn(SymbolKindBundle {
                marker: SymbolKind,
                visible: Default::default(),
                name: Name(module.name.clone()),
                size: Default::default(),
                designator_prefix: DesignatorPrefix(module.prefix.clone()),
            })
            .id();
        id_map.insert(module.symbol_kind.clone(), circuit_symbol_kind_id);

        let circuit_id = commands
            .spawn(CircuitBundle {
                marker: Circuit,
                symbol_kind: SymbolKindID(circuit_symbol_kind_id),
            })
            .id();
        id_map.insert(module.id.clone(), circuit_id);

        for symbol in module.symbols.iter() {
            let symbol_kind_id;
            if let Some(kind_name) = symbol.symbol_kind_name.as_ref() {
                // find a symbol kind by the same name
                let (kind, _) = symbol_kinds_q
                    .iter()
                    .filter(|(_, name)| name.0 == *kind_name)
                    .next()
                    .ok_or_else(|| anyhow::anyhow!("SymbolKind {} not found", kind_name))?;
                symbol_kind_id = kind.id();
            } else if let Some(kind_id) = symbol.symbol_kind_id.as_ref() {
                symbol_kind_id = id_map.get(&kind_id).unwrap().clone();
            } else {
                return Err(anyhow::anyhow!("Symbol {} has no SymbolKind", symbol.id.0));
            }
            let (kind, _) = symbol_kinds_q.get(symbol_kind_id).unwrap();
            let name = kind.get::<Name>().unwrap().0.clone();
            let designator_prefix = kind.get::<DesignatorPrefix>().unwrap().0.clone();
            let shape = kind.get::<Shape>().unwrap().0;
            let size = kind.get::<Size>().unwrap();

            let symbol_id = commands
                .spawn(SymbolBundle {
                    marker: Symbol,
                    visible: Visible {
                        shape: Shape(shape),
                        ..Default::default()
                    },
                    name: Name(name),
                    designator_prefix: DesignatorPrefix(designator_prefix),
                    designator_number: DesignatorNumber(symbol.number),
                    rotation: Default::default(),
                    size: Size {
                        width: size.width,
                        height: size.height,
                    },
                    symbol_kind: SymbolKindID(circuit_symbol_kind_id),
                })
                .set_parent(circuit_id)
                .id();
            id_map.insert(symbol.id.clone(), symbol_id);
        }
    }

    Ok(id_map.get(&modules[0].id).unwrap().clone())
}
