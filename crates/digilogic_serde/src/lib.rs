mod digital;
mod json;

use bevy_ecs::prelude::*;
use digilogic_core::events::*;
use digilogic_core::symbol::SymbolRegistry;

fn load(
    mut commands: Commands,
    mut ev_load: EventReader<LoadEvent>,
    mut ev_loaded: EventWriter<LoadedEvent>,
    symbols: Res<SymbolRegistry>,
) {
    for ev in ev_load.read() {
        if let Some(ext) = (ev.filename).extension() {
            if ext == "dlc" {
                json::load_json(&mut commands, ev.filename.clone(), &mut ev_loaded, &symbols);
            } else if ext == "dig" {
                digital::load_digital(&mut commands, ev.filename.clone(), &mut ev_loaded, &symbols);
            }
        }
    }
}

#[derive(Default)]
pub struct LoadSavePlugin;

impl bevy_app::Plugin for LoadSavePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.add_systems(bevy_app::Update, load);
    }
}
