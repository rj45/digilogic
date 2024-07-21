use crate::canvas::Canvas;
use egui::*;
use egui_wgpu::RenderState;

#[derive(serde::Serialize, serde::Deserialize)]
#[serde(default)]
pub struct App {
    dark_mode: bool,
    #[serde(skip)]
    canvas: Option<Canvas>,
    #[serde(skip)]
    scene: vello::Scene,
}

impl Default for App {
    fn default() -> Self {
        Self {
            dark_mode: true,
            canvas: None,
            scene: vello::Scene::new(),
        }
    }
}

impl App {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let this: Self = cc
            .storage
            .and_then(|storage| eframe::get_value(storage, eframe::APP_KEY))
            .unwrap_or_default();

        let visuals = if this.dark_mode {
            Visuals::dark()
        } else {
            Visuals::light()
        };
        cc.egui_ctx.set_visuals(visuals);

        this
    }

    fn update_main_menu(&mut self, ctx: &Context, ui: &mut Ui) {
        menu::bar(ui, |ui| {
            ui.menu_button("File", |ui| {
                #[cfg(not(target_arch = "wasm32"))]
                if ui.button("Quit").clicked() {
                    ctx.send_viewport_cmd(ViewportCommand::Close);
                }
            });
            ui.add_space(16.0);

            ui.with_layout(Layout::top_down(Align::RIGHT), |ui| {
                widgets::global_dark_light_mode_switch(ui);
                self.dark_mode = ctx.style().visuals.dark_mode;
            });
        });
    }

    fn update_canvas(&mut self, render_state: &RenderState, ui: &mut Ui) {
        let canvas_size = ui.available_size();
        let canvas_width = (canvas_size.x.floor() as u32).max(1);
        let canvas_height = (canvas_size.y.floor() as u32).max(1);

        let canvas = self
            .canvas
            .get_or_insert_with(|| Canvas::create(render_state, canvas_width, canvas_height));
        canvas.resize(render_state, canvas_width, canvas_height);

        self.scene.reset();
        // TODO: populate scene

        canvas.render(render_state, &self.scene, ui.visuals().extreme_bg_color);

        Image::new((canvas.texture_id(), canvas_size)).ui(ui);
    }
}

impl eframe::App for App {
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        eframe::set_value(storage, eframe::APP_KEY, self);
    }

    fn update(&mut self, ctx: &Context, frame: &mut eframe::Frame) {
        TopBottomPanel::top("top_panel").show(ctx, |ui| {
            self.update_main_menu(ctx, ui);
        });

        TopBottomPanel::bottom("bottom_panel").show(ctx, |ui| {
            ui.with_layout(Layout::bottom_up(Align::RIGHT), |ui| {
                warn_if_debug_build(ui);
            });
        });

        CentralPanel::default().show(ctx, |ui| {
            let render_state = frame.wgpu_render_state().unwrap();
            self.update_canvas(render_state, ui);
        });
    }
}
