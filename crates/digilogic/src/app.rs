use digilogic_core::Plugin;
use egui::*;

#[derive(serde::Serialize, serde::Deserialize, bevy_ecs::system::Resource)]
pub struct AppState {
    pub dark_mode: bool,
}

impl Default for AppState {
    fn default() -> Self {
        Self { dark_mode: true }
    }
}

pub struct App {
    state: Option<AppState>,
    world: Option<bevy_ecs::world::World>,
    schedule: bevy_ecs::schedule::Schedule,
}

impl App {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let state: AppState = cc
            .storage
            .and_then(|storage| eframe::get_value(storage, eframe::APP_KEY))
            .unwrap_or_default();

        let visuals = if state.dark_mode {
            Visuals::dark()
        } else {
            Visuals::light()
        };
        cc.egui_ctx.set_visuals(visuals);

        Self {
            state: Some(state),
            world: None,
            schedule: bevy_ecs::schedule::Schedule::default(),
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

    fn update(&mut self, context: &Context, frame: &mut eframe::Frame) {
        use crate::ui::UiPlugin;

        let world = self.world.get_or_insert_with(|| {
            let mut world = bevy_ecs::world::World::new();
            world.insert_resource(self.state.take().unwrap());
            UiPlugin::new(context, frame).build(&mut world, &mut self.schedule);
            world
        });

        self.schedule.run(world);
    }
}
