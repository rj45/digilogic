mod canvas;
use canvas::*;

mod draw;
use draw::*;

use crate::app::AppState;
use bevy_ecs::prelude::*;
use digilogic_core::Plugin;
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

fn update_main_menu(egui: &Egui, ui: &mut Ui, app_state: &mut AppState) {
    menu::bar(ui, |ui| {
        ui.menu_button("File", |ui| {
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
) {
    TopBottomPanel::top("top_panel").show(&egui.context, |ui| {
        update_main_menu(&egui, ui, &mut app_state);
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

pub struct UiPlugin<'a> {
    context: &'a Context,
    frame: &'a mut eframe::Frame,
}

impl<'a> UiPlugin<'a> {
    #[inline]
    pub fn new(context: &'a Context, frame: &'a mut eframe::Frame) -> Self {
        Self { context, frame }
    }
}

impl Plugin for UiPlugin<'_> {
    fn build(self, world: &mut World, schedule: &mut Schedule) {
        let render_state = self.frame.wgpu_render_state().unwrap();
        let canvas = Canvas::create(render_state);

        world.insert_non_send_resource(canvas);
        world.insert_resource(Egui::new(self.context, render_state));
        world.insert_resource(Scene::default());
        schedule.add_systems(draw);
        schedule.add_systems(update.after(draw));
    }
}
