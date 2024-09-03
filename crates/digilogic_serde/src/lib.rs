mod digital;
mod json;
mod yosys;

use anyhow::{bail, Result};
use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use bevy_log::error;
use digilogic_core::components::{CircuitID, FilePath};
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;
use digilogic_core::HashMap;
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

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

fn load_circuit_file(
    commands: &mut Commands,
    filename: &Path,
    registry: &mut FileRegistry,
    symbols: &SymbolRegistry,
) -> Result<CircuitID> {
    let file_id = FileId::for_path(filename)?;

    if let Some(circuit) = registry.0.get(&file_id) {
        // Make sure the circuit is still loaded
        if commands.get_entity(circuit.0).is_some() {
            return Ok(*circuit);
        }
    }

    if let Some(ext) = filename.extension() {
        let circuit = if ext == "dlc" {
            json::load_json(commands, filename, symbols)?
        } else if ext == "dig" {
            digital::load_digital(commands, filename, symbols)?
        } else if ext == "yosys" {
            yosys::load_yosys(commands, filename, symbols)?
        } else if ext == "json" {
            yosys::load_yosys(commands, filename, symbols)
                .or_else(|_| json::load_json(commands, filename, symbols))?
        } else {
            bail!("unsupported file extension '{}'", ext.to_string_lossy());
        };

        commands
            .entity(circuit)
            .insert(FilePath(filename.to_owned()));

        let circuit = CircuitID(circuit);
        registry.0.insert(file_id, circuit);
        Ok(circuit)
    } else {
        bail!("file without extension is not supported");
    }
}

fn handle_circuit_load_events(
    mut commands: Commands,
    mut circuit_load_events: EventReader<CircuitLoadEvent>,
    mut circuit_loaded_events: EventWriter<CircuitLoadedEvent>,
    mut registry: ResMut<FileRegistry>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in circuit_load_events.read() {
        match load_circuit_file(&mut commands, &ev.filename, &mut registry, &symbols) {
            Ok(circuit) => {
                circuit_loaded_events.send(CircuitLoadedEvent { circuit });
            }
            Err(e) => {
                // TODO: instead of this, send an ErrorEvent
                error!("error loading circuit {}: {:?}", ev.filename.display(), e);
            }
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
struct Project {
    name: String,
    #[serde(default)]
    circuits: Vec<PathBuf>,
    #[serde(default)]
    root_circuit: Option<usize>,
}

fn load_project_file(
    commands: &mut Commands,
    filename: &Path,
    registry: &mut FileRegistry,
    symbols: &SymbolRegistry,
) -> Result<Vec<CircuitID>> {
    let ron = std::fs::read_to_string(filename)?;
    let project: Project = ron::Options::default()
        .with_default_extension(ron::extensions::Extensions::all())
        .from_str(&ron)?;

    let prev_dir = std::env::current_dir().ok();
    std::env::set_current_dir(filename.parent().unwrap())?;

    let circuits = project
        .circuits
        .iter()
        .map(|circuit_filename| load_circuit_file(commands, circuit_filename, registry, symbols))
        .collect::<Result<Vec<_>>>()?;

    if let Some(prev_dir) = prev_dir {
        std::env::set_current_dir(prev_dir)?;
    }

    commands.insert_resource(digilogic_core::resources::Project {
        name: project.name,
        file_path: Some(filename.to_owned()),
        root_circuit: project.root_circuit.and_then(|i| circuits.get(i).copied()),
    });

    Ok(circuits)
}

fn handle_project_load_events(
    mut commands: Commands,
    mut project_load_events: EventReader<ProjectLoadEvent>,
    mut project_loaded_events: EventWriter<ProjectLoadedEvent>,
    mut circuit_loaded_events: EventWriter<CircuitLoadedEvent>,
    mut registry: ResMut<FileRegistry>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in project_load_events.read() {
        match load_project_file(&mut commands, &ev.filename, &mut registry, &symbols) {
            Ok(circuits) => {
                for circuit in circuits {
                    circuit_loaded_events.send(CircuitLoadedEvent { circuit });
                }
                project_loaded_events.send(ProjectLoadedEvent);
            }
            Err(e) => {
                // TODO: instead of this, send an ErrorEvent
                error!("error loading project {}: {:?}", ev.filename.display(), e);
            }
        }
    }
}

#[derive(Default, Debug)]
pub struct LoadSavePlugin;

impl bevy_app::Plugin for LoadSavePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.init_resource::<FileRegistry>();
        app.add_systems(
            bevy_app::Update,
            (handle_circuit_load_events, handle_project_load_events),
        );
    }
}
