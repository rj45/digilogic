#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod ui;
mod ux;

use bevy_ecs::prelude::*;
use bevy_reflect::Reflect;
use digilogic_routing::RoutingConfig;
use serde::{Deserialize, Serialize};

const ROUTING_CONFIG_KEY: &str = "routing";

#[derive(Serialize, Deserialize, Resource, Reflect)]
#[reflect(Resource)]
struct AppState {
    dark_mode: bool,
    show_bounding_boxes: bool,
    show_routing_graph: bool,
    show_root_wires: bool,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            dark_mode: true,
            show_bounding_boxes: false,
            show_routing_graph: false,
            show_root_wires: false,
        }
    }
}

#[derive(Event)]
enum FileDialogEvent {
    Open,
    Save,
}

#[repr(transparent)]
struct App(bevy_app::App);

impl App {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let context = &cc.egui_ctx;
        let render_state = cc.wgpu_render_state.as_ref().unwrap();

        let app_state: AppState = cc
            .storage
            .and_then(|storage| eframe::get_value(storage, eframe::APP_KEY))
            .unwrap_or_default();

        let visuals = if app_state.dark_mode {
            egui::Visuals::dark()
        } else {
            egui::Visuals::light()
        };
        context.set_visuals(visuals);

        let mut app = bevy_app::App::default();

        // Bevy plugins
        app.add_plugins((
            bevy_core::TaskPoolPlugin::default(),
            bevy_core::TypeRegistrationPlugin,
            bevy_core::FrameCountPlugin,
            bevy_log::LogPlugin::default(),
        ));

        app.register_type::<AppState>();
        app.insert_resource(app_state);
        app.add_event::<FileDialogEvent>();

        // TODO: find a way to have plugins register what they want to save and restore.
        if let Some(routing_config) = cc
            .storage
            .and_then(|storage| eframe::get_value::<RoutingConfig>(storage, ROUTING_CONFIG_KEY))
        {
            app.insert_resource(routing_config);
        }

        // Digilogic plugins
        app.add_plugins((
            digilogic_core::CorePlugin,
            digilogic_serde::LoadSavePlugin,
            digilogic_routing::RoutingPlugin,
            ux::UxPlugin::default(),
            ui::UiPlugin::new(context, render_state),
        ));

        Self(app)
    }
}

fn handle_file_dialog(world: &mut World, frame: &mut eframe::Frame) {
    type FileDialogEvents = Events<FileDialogEvent>;
    type LoadEvents = Events<digilogic_core::events::LoadEvent>;

    let file_dialog_event = {
        let mut file_dialog_events = world.get_resource_mut::<FileDialogEvents>().unwrap();
        let mut file_dialog_events = file_dialog_events.drain();
        let file_dialog_event = file_dialog_events.next();

        assert!(
            file_dialog_events.next().is_none(),
            "multiple file dialog events in one frame",
        );

        file_dialog_event
    };

    if let Some(file_dialog_event) = file_dialog_event {
        #[cfg(not(target_arch = "wasm32"))]
        {
            let dialog = rfd::FileDialog::new()
                .add_filter("Digilogic Circuit", &["dlc"])
                .add_filter("Digital Circuit", &["dig"]);
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
        if let Some(app_state) = self.0.world().get_resource::<AppState>() {
            eframe::set_value(storage, eframe::APP_KEY, app_state);
        }

        // TODO: find a way to have plugins register what they want to save and restore.
        if let Some(routing_config) = self.0.world().get_resource::<RoutingConfig>() {
            eframe::set_value(storage, ROUTING_CONFIG_KEY, routing_config);
        }
    }

    fn update(&mut self, context: &egui::Context, frame: &mut eframe::Frame) {
        match self.0.plugins_state() {
            bevy_app::PluginsState::Adding => {
                context.request_repaint_after_secs(0.01);
            }
            bevy_app::PluginsState::Ready => {
                self.0.finish();
                context.request_repaint();
            }
            bevy_app::PluginsState::Finished => {
                self.0.cleanup();
                context.request_repaint();
            }
            bevy_app::PluginsState::Cleaned => {
                self.0.update();
                handle_file_dialog(self.0.world_mut(), frame);
            }
        }
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
