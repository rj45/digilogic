mod canvas;
use canvas::*;

mod draw;
use draw::*;

use crate::{AppState, FileDialogEvent};
use bevy_ecs::prelude::*;
use egui::*;
use egui_wgpu::RenderState;

#[derive(Resource)]
struct Egui {
    context: Context,
    render_state: RenderState,
}

impl Egui {
    fn new(context: &Context, render_state: &RenderState) -> Self {
        Self {
            context: context.clone(),
            render_state: render_state.clone(),
        }
    }
}

fn update_main_menu(
    egui: &Egui,
    ui: &mut Ui,
    app_state: &mut AppState,
    file_dialog_events: &mut EventWriter<FileDialogEvent>,
) {
    menu::bar(ui, |ui| {
        ui.menu_button("File", |ui| {
            if ui.button("Open").clicked() {
                file_dialog_events.send(FileDialogEvent::Open);
            }

            if ui.button("Save").clicked() {
                file_dialog_events.send(FileDialogEvent::Save);
            }

            ui.separator();

            #[cfg(not(target_arch = "wasm32"))]
            if ui.button("Quit").clicked() {
                egui.context.send_viewport_cmd(ViewportCommand::Close);
            }
        });
        ui.add_space(16.0);

        ui.with_layout(Layout::top_down(Align::RIGHT), |ui| {
            widgets::global_dark_light_mode_switch(ui);
            app_state.dark_mode = egui.context.style().visuals.dark_mode;
        });
    });
}

fn update_canvas(egui: &Egui, ui: &mut Ui, canvas: &mut Canvas, scene: &Scene) {
    let canvas_size = ui.available_size();
    let canvas_width = (canvas_size.x.floor() as u32).max(1);
    let canvas_height = (canvas_size.y.floor() as u32).max(1);

    canvas.resize(&egui.render_state, canvas_width, canvas_height);
    canvas.render(&egui.render_state, &scene.0, ui.visuals().extreme_bg_color);

    Image::new((canvas.texture_id(), canvas_size)).ui(ui);
}

fn update(
    egui: Res<Egui>,
    mut canvas: NonSendMut<Canvas>,
    scene: Res<Scene>,
    mut app_state: ResMut<AppState>,
    mut file_dialog_events: EventWriter<FileDialogEvent>,
) {
    TopBottomPanel::top("top_panel").show(&egui.context, |ui| {
        update_main_menu(&egui, ui, &mut app_state, &mut file_dialog_events);
    });

    TopBottomPanel::bottom("bottom_panel").show(&egui.context, |ui| {
        ui.with_layout(Layout::bottom_up(Align::RIGHT), |ui| {
            warn_if_debug_build(ui);
        });
    });

    CentralPanel::default().show(&egui.context, |ui| {
        update_canvas(&egui, ui, &mut canvas, &scene);
    });
}

#[cfg(feature = "inspector")]
pub fn inspect(world: &mut World) {
    let Some(egui) = world.get_resource::<Egui>() else {
        return;
    };
    let context = egui.context.clone();

    Window::new("Inspector").show(&context, |ui| {
        ScrollArea::both().show(ui, |ui| {
            CollapsingHeader::new("Entities")
                .default_open(true)
                .show(ui, |ui| {
                    bevy_inspector_egui::bevy_inspector::ui_for_world_entities(world, ui);
                });
            CollapsingHeader::new("Resources").show(ui, |ui| {
                bevy_inspector_egui::bevy_inspector::ui_for_resources(world, ui);
            });
            ui.allocate_space(ui.available_size());
        });
    });
}

pub struct UiPlugin {
    context: Context,
    render_state: RenderState,
}

impl UiPlugin {
    pub fn new(context: &Context, render_state: &RenderState) -> Self {
        Self {
            context: context.clone(),
            render_state: render_state.clone(),
        }
    }
}

impl bevy_app::Plugin for UiPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.insert_non_send_resource(Canvas::create(&self.render_state));
        app.insert_resource(Egui::new(&self.context, &self.render_state));
        app.insert_resource(Scene::default());
        app.insert_resource(SymbolSVGs(Vec::new()));
        app.add_systems(bevy_app::Startup, init_symbol_shapes);
        app.add_systems(bevy_app::Update, draw);
        app.add_systems(bevy_app::Update, update.after(draw));

        #[cfg(feature = "inspector")]
        {
            // Crashes if these types are not registered
            app.register_type::<std::path::PathBuf>();
            app.register_type::<std::time::Instant>();

            app.add_plugins(bevy_inspector_egui::DefaultInspectorConfigPlugin);
            app.add_systems(bevy_app::Last, inspect);
        }
    }
}
