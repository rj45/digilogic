#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod ui;

#[derive(serde::Serialize, serde::Deserialize, bevy_ecs::system::Resource)]
struct AppState {
    dark_mode: bool,
}

impl Default for AppState {
    fn default() -> Self {
        Self { dark_mode: true }
    }
}

struct App {
    state: Option<AppState>,
    world: Option<bevy_ecs::world::World>,
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
        use digilogic_core::Plugin;

        let world = self.world.get_or_insert_with(|| {
            let mut world = bevy_ecs::world::World::new();
            world.insert_resource(self.state.take().unwrap());
            UiPlugin::new(context, frame).build(&mut world, &mut self.schedule);
            world
        });

        self.schedule.run(world);
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
