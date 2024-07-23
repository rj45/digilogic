#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod ui;

use bevy_ecs::event::Event;
use bevy_ecs::system::Resource;
use bevy_ecs::world::World;
use digilogic_serde::load_json;
use egui::load;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Resource)]
struct AppState {
    dark_mode: bool,
}

impl Default for AppState {
    fn default() -> Self {
        Self { dark_mode: true }
    }
}

#[derive(Event)]
enum FileDialogEvent {
    Open,
    Save,
}

struct App {
    state: Option<AppState>,
    world: Option<World>,
    schedule: bevy_ecs::schedule::Schedule,
}

impl App {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let state: AppState = cc
            .storage
            .and_then(|storage| eframe::get_value(storage, eframe::APP_KEY))
            .unwrap_or_default();

        let visuals = if state.dark_mode {
            egui::Visuals::dark()
        } else {
            egui::Visuals::light()
        };
        cc.egui_ctx.set_visuals(visuals);

        Self {
            state: Some(state),
            world: None,
            schedule: Default::default(),
        }
    }
}

fn handle_file_dialog(world: &mut World, frame: &mut eframe::Frame) {
    type FileDialogEvents = bevy_ecs::event::Events<FileDialogEvent>;
    type LoadEvents = bevy_ecs::event::Events<digilogic_core::events::LoadEvent>;
    let file_dialog_event: Option<FileDialogEvent>;
    {
        let mut file_dialog_events = world.get_resource_mut::<FileDialogEvents>().unwrap();
        let mut file_dialog_events = file_dialog_events.drain();
        file_dialog_event = file_dialog_events.next();

        assert!(
            file_dialog_events.next().is_none(),
            "multiple file dialog events in one frame",
        );
    }

    if let Some(file_dialog_event) = file_dialog_event {
        #[cfg(not(target_arch = "wasm32"))]
        {
            let dialog = rfd::FileDialog::new().add_filter("Digilogic Circuit", &["dlc"]);
            #[cfg(any(target_os = "macos", target_os = "windows", target_os = "linux"))]
            let dialog = dialog.set_parent(frame);

            match file_dialog_event {
                FileDialogEvent::Open => {
                    if let Some(file) = dialog.pick_file() {
                        let mut load_events = world.get_resource_mut::<LoadEvents>().unwrap();
                        load_events.send(digilogic_core::events::LoadEvent { filename: file });
                    }
                }
                FileDialogEvent::Save => {
                    if let Some(file) = dialog.save_file() {
                        // TODO: save circuit file
                    }
                }
            }
        }

        #[cfg(target_arch = "wasm32")]
        {
            todo!();
        }
    }
}

impl eframe::App for App {
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        let state = if let Some(world) = self.world.as_ref() {
            world.get_resource::<AppState>().unwrap()
        } else {
            self.state.as_ref().unwrap()
        };

        eframe::set_value(storage, eframe::APP_KEY, state);
    }

    fn update(&mut self, context: &egui::Context, frame: &mut eframe::Frame) {
        use crate::ui::UiPlugin;
        use bevy_ecs::event::*;
        use bevy_hierarchy::HierarchyEvent;
        use digilogic_core::Plugin;

        let world = self.world.get_or_insert_with(|| {
            let mut world = World::new();

            world.insert_resource(self.state.take().unwrap());
            self.schedule.add_systems((event_update_system, load_json));
            EventRegistry::register_event::<HierarchyEvent>(&mut world);
            EventRegistry::register_event::<FileDialogEvent>(&mut world);
            EventRegistry::register_event::<digilogic_core::events::LoadEvent>(&mut world);
            EventRegistry::register_event::<digilogic_core::events::LoadedEvent>(&mut world);
            UiPlugin::new(context, frame).build(&mut world, &mut self.schedule);

            world
        });

        self.schedule.run(world);
        handle_file_dialog(world, frame);
    }
}

#[cfg(not(target_arch = "wasm32"))]
fn main() -> eframe::Result {
    env_logger::init();

    let native_options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1024.0, 768.0])
            .with_min_inner_size([640.0, 480.0])
            .with_icon(
                eframe::icon_data::from_png_bytes(include_bytes!("../assets/icon-256.png"))
                    .expect("Failed to load icon"),
            ),
        ..Default::default()
    };

    eframe::run_native(
        "digilogic",
        native_options,
        Box::new(|cc| Ok(Box::new(App::new(cc)))),
    )
}

#[cfg(target_arch = "wasm32")]
fn main() {
    eframe::WebLogger::init(log::LevelFilter::Debug).ok();

    let web_options = eframe::WebOptions::default();

    wasm_bindgen_futures::spawn_local(async {
        let start_result = eframe::WebRunner::new()
            .start(
                "egui-host",
                web_options,
                Box::new(|cc| Ok(Box::new(App::new(cc)))),
            )
            .await;

        let loading_text = web_sys::window()
            .and_then(|w| w.document())
            .and_then(|d| d.get_element_by_id("loading_text"));
        if let Some(loading_text) = loading_text {
            match start_result {
                Ok(_) => {
                    loading_text.remove();
                }
                Err(e) => {
                    loading_text.set_inner_html(
                        "<p> The app has crashed. See the developer console for details. </p>",
                    );
                    panic!("Failed to start eframe: {e:?}");
                }
            }
        }
    });
}
