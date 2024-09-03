#![cfg_attr(
    all(not(debug_assertions), not(feature = "trace")),
    windows_subsystem = "windows"
)]

mod ui;

use bevy_ecs::prelude::*;
use bevy_reflect::Reflect;
use bevy_state::prelude::*;
use bevy_state::prelude::{OnEnter, OnExit};
use bevy_time::{Time, Virtual};
use digilogic_core::SharedStr;
use digilogic_routing::RoutingConfig;
use serde::{Deserialize, Serialize};

const ROUTING_CONFIG_KEY: &str = "routing";

#[cfg(debug_assertions)]
const LOG_LEVEL: bevy_log::Level = bevy_log::Level::DEBUG;
#[cfg(not(debug_assertions))]
const LOG_LEVEL: bevy_log::Level = bevy_log::Level::INFO;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect, Serialize, Deserialize)]
enum Backend {
    #[cfg(not(target_arch = "wasm32"))]
    Builtin,
    External,
}

impl Backend {
    const ALL: &[Self] = &[
        #[cfg(not(target_arch = "wasm32"))]
        Self::Builtin,
        Self::External,
    ];
}

#[cfg(not(target_arch = "wasm32"))]
impl Default for Backend {
    fn default() -> Self {
        Self::Builtin
    }
}

#[cfg(target_arch = "wasm32")]
impl Default for Backend {
    fn default() -> Self {
        Self::External
    }
}

#[derive(Serialize, Deserialize, Resource, Reflect)]
#[reflect(Resource)]
struct AppSettings {
    dark_mode: bool,
    show_bounding_boxes: bool,
    show_routing_graph: bool,
    show_root_wires: bool,
    backend: Backend,
    builtin_backend_engine: native_main::SimulationEngine,
    external_backend_addr: (SharedStr, u16),
}

const DEFAULT_LOCAL_SERVER_ADDR: (SharedStr, u16) = (
    SharedStr::new_static("127.0.0.1"),
    digilogic_netcode::DEFAULT_PORT,
);

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            dark_mode: true,
            show_bounding_boxes: false,
            show_routing_graph: false,
            show_root_wires: false,
            backend: Backend::default(),
            builtin_backend_engine: native_main::SimulationEngine::default(),
            external_backend_addr: DEFAULT_LOCAL_SERVER_ADDR,
        }
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, States)]
enum AppState {
    #[default]
    Normal,
    Simulating,
}

#[derive(Event)]
enum FileDialogEvent {
    OpenProject,
    SaveProject,

    AddCircuit,
    ImportCircuit,
    SaveCircuit,
}

#[repr(transparent)]
struct App(bevy_app::App);

fn pause_time(mut time: ResMut<Time<Virtual>>) {
    time.pause();
}

fn resume_time(mut time: ResMut<Time<Virtual>>) {
    time.unpause();
}

impl App {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let context = &cc.egui_ctx;
        let render_state = cc.wgpu_render_state.as_ref().unwrap();

        let app_state: AppSettings = cc
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
            bevy_time::TimePlugin,
            bevy_state::app::StatesPlugin,
            bevy_log::LogPlugin {
                level: LOG_LEVEL,
                ..Default::default()
            },
        ));

        app.register_type::<AppSettings>();
        app.init_state::<AppState>();
        app.insert_resource(app_state);
        app.add_event::<FileDialogEvent>();

        // Setup virtual time to only advance while simulating.
        let mut virt_time = app.world_mut().get_resource_mut::<Time<Virtual>>().unwrap();
        virt_time.pause();
        virt_time.set_max_delta(std::time::Duration::MAX);
        app.add_systems(OnExit(AppState::Simulating), pause_time);
        app.add_systems(OnEnter(AppState::Simulating), resume_time);

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
            digilogic_netcode::ClientPlugin,
            digilogic_ux::UxPlugin,
            ui::UiPlugin::new(context, render_state),
        ));

        Self(app)
    }
}

fn handle_exit_events(world: &mut World, context: &egui::Context) {
    type AppExitEvents = Events<bevy_app::AppExit>;

    let mut exit_events = world.get_resource_mut::<AppExitEvents>().unwrap();
    let close_requested = context.input(|i| i.viewport().close_requested());

    if !exit_events.is_empty() && !close_requested {
        context.send_viewport_cmd(egui::ViewportCommand::Close);
    } else if close_requested {
        exit_events.send(bevy_app::AppExit::Success);
    }
}

trait FileDialogExt {
    fn add_project_filters(self) -> Self;
    fn add_circuit_filters(self) -> Self;
    fn add_import_filters(self) -> Self;
}

impl FileDialogExt for rfd::FileDialog {
    fn add_project_filters(self) -> Self {
        self.add_filter("Digilogic project", &["dlp"])
    }

    fn add_circuit_filters(self) -> Self {
        self.add_filter("Digilogic Circuit", &["dlc"])
    }

    fn add_import_filters(self) -> Self {
        self.add_filter("Digital Circuit", &["dig"])
            .add_filter("Yosys JSON", &["yosys", "json"])
    }
}

fn handle_file_dialog(world: &mut World, frame: &mut eframe::Frame) {
    type FileDialogEvents = Events<FileDialogEvent>;
    type ProjectLoadEvents = Events<digilogic_core::events::ProjectLoadEvent>;
    type CircuitLoadEvents = Events<digilogic_core::events::CircuitLoadEvent>;

    let mut file_dialog_events = world.get_resource_mut::<FileDialogEvents>().unwrap();
    let file_dialog_events: Vec<_> = file_dialog_events.drain().collect();

    for file_dialog_event in file_dialog_events {
        #[cfg(not(target_arch = "wasm32"))]
        {
            let dialog = rfd::FileDialog::new().set_parent(frame);

            match file_dialog_event {
                FileDialogEvent::OpenProject => {
                    if let Some(filename) = dialog.add_project_filters().pick_file() {
                        let mut load_events =
                            world.get_resource_mut::<ProjectLoadEvents>().unwrap();
                        load_events.send(digilogic_core::events::ProjectLoadEvent { filename });
                    }
                }
                FileDialogEvent::SaveProject => {
                    if let Some(filename) = dialog.add_project_filters().save_file() {
                        // TODO: save project file
                    }
                }
                FileDialogEvent::AddCircuit => {
                    if let Some(filename) = dialog.add_circuit_filters().pick_file() {
                        let mut load_events =
                            world.get_resource_mut::<CircuitLoadEvents>().unwrap();
                        load_events.send(digilogic_core::events::CircuitLoadEvent { filename });
                    }
                }
                FileDialogEvent::ImportCircuit => {
                    if let Some(filename) = dialog.add_import_filters().pick_file() {
                        let mut load_events =
                            world.get_resource_mut::<CircuitLoadEvents>().unwrap();
                        load_events.send(digilogic_core::events::CircuitLoadEvent { filename });
                    }
                }
                FileDialogEvent::SaveCircuit => {
                    if let Some(filename) = dialog.add_project_filters().save_file() {
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
        if let Some(app_state) = self.0.world().get_resource::<AppSettings>() {
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
                handle_exit_events(self.0.world_mut(), context);
                self.0.update();
                handle_file_dialog(self.0.world_mut(), frame);
            }
        }
    }
}

#[cfg(not(target_arch = "wasm32"))]
mod native_main {
    use clap::{Parser, Subcommand, ValueEnum};
    use serde::{Deserialize, Serialize};

    #[derive(
        Default,
        Debug,
        Clone,
        Copy,
        PartialEq,
        Eq,
        ValueEnum,
        bevy_reflect::Reflect,
        Serialize,
        Deserialize,
    )]
    pub enum SimulationEngine {
        #[default]
        /// 4-state lockstep simulation
        Gsim,
        /// 4-state lockstep simulation using GPU compute
        GsimCompute,
    }

    impl SimulationEngine {
        pub const ALL: &[Self] = &[Self::Gsim, Self::GsimCompute];
    }

    #[derive(Subcommand)]
    enum Commands {
        /// Starts a simulation server
        Server {
            /// The simulation engine to use
            #[arg(short, long)]
            engine: Option<SimulationEngine>,
            /// The port to listen on
            #[arg(short, long)]
            port: Option<u16>,
        },
    }

    #[derive(Parser)]
    #[command(name = "Digilogic", version, about, long_about = None, propagate_version = true)]
    struct Args {
        #[command(subcommand)]
        pub command: Option<Commands>,
    }

    fn run_gui() {
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
            Box::new(|cc| Ok(Box::new(crate::App::new(cc)))),
        )
        .unwrap();
    }

    pub fn run() {
        let args = Args::parse();
        match args.command {
            None => run_gui(),
            Some(Commands::Server { engine, port }) => match engine.unwrap_or_default() {
                SimulationEngine::Gsim => {
                    digilogic_netcode::run_server(port, digilogic_gsim::GsimServer::default())
                        .unwrap()
                }
                SimulationEngine::GsimCompute => todo!(),
            },
        }
    }
}

#[cfg(not(target_arch = "wasm32"))]
fn main() {
    native_main::run();
}

#[cfg(target_arch = "wasm32")]
fn main() {
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
