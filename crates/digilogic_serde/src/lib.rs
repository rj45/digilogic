mod digital;
mod json;

use anyhow::{anyhow, bail, Result};
use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use digilogic_core::components::CircuitID;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;
use std::path::{Path, PathBuf};

type HashMap<K, V> = ahash::AHashMap<K, V>;

#[cfg(target_family = "unix")]
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[repr(transparent)]
struct FileId(u64);

#[cfg(not(target_family = "unix"))]
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[repr(transparent)]
struct FileId(PathBuf);

impl FileId {
    #[cfg(target_family = "unix")]
    fn for_path<P: AsRef<Path>>(path: P) -> std::io::Result<Self> {
        use std::os::unix::fs::MetadataExt;

        let metadata = std::fs::metadata(path)?;
        Ok(Self(metadata.ino()))
    }

    #[cfg(not(target_family = "unix"))]
    fn for_path<P: AsRef<Path>>(path: P) -> std::io::Result<Self> {
        path.as_ref().canonicalize().map(Self)
    }
}

#[derive(Debug, Default, Deref, DerefMut, Resource)]
#[repr(transparent)]
struct FileRegistry(HashMap<FileId, CircuitID>);

fn load_file(
    commands: &mut Commands,
    ev: &LoadEvent,
    registry: &mut FileRegistry,
    symbols: &SymbolRegistry,
) -> Result<LoadedEvent> {
    let file_id = FileId::for_path(&ev.filename)?;
    let circuit = if let Some(circuit) = registry.0.get(&file_id) {
        *circuit
    } else if let Some(ext) = ev.filename.extension() {
        let circuit = if ext == "dlc" {
            json::load_json(commands, &ev.filename, symbols)
        } else if ext == "dig" {
            digital::load_digital(commands, &ev.filename, symbols)
        } else {
            Err(anyhow!(
                "unsupported file extension '{}'",
                ext.to_string_lossy()
            ))
        }?;

        let circuit = CircuitID(circuit);
        registry.0.insert(file_id, circuit);
        circuit
    } else {
        bail!("file without extension is not supported");
    };

    Ok(LoadedEvent {
        filename: ev.filename.clone(),
        circuit,
    })
}

fn handle_load_events(
    mut commands: Commands,
    mut ev_load: EventReader<LoadEvent>,
    mut ev_loaded: EventWriter<LoadedEvent>,
    mut registry: ResMut<FileRegistry>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in ev_load.read() {
        match load_file(&mut commands, ev, &mut registry, &symbols) {
            Ok(ev) => {
                ev_loaded.send(ev);
            }
            Err(e) => {
                // TODO: instead of this, send an ErrorEvent
                eprintln!("Error loading circuit {}: {:?}", ev.filename.display(), e);
            }
        }
    }
}

fn handle_unloaded_events(
    mut registry: ResMut<FileRegistry>,
    mut ev_unloaded: EventReader<UnloadedEvent>,
) {
    for ev in ev_unloaded.read() {
        registry.retain(|_, v| *v != ev.circuit);
    }
}

#[derive(Default, Debug)]
pub struct LoadSavePlugin;

impl bevy_app::Plugin for LoadSavePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.init_resource::<FileRegistry>();
        app.add_systems(
            bevy_app::Update,
            (handle_load_events, handle_unloaded_events),
        );
    }
}
