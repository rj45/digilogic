use super::{Egui, OpenWindows};
use crate::{AppSettings, Backend};
use bevy_ecs::prelude::*;
use egui::*;
use egui_dock::*;

macro_rules! def_pages {
    ($($name:ident),* $(,)?) => {
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        enum Page {
            $($name,)*
        }

        impl Page {
            const ALL: &[Self] = &[
                $(Self::$name,)*
            ];
        }
    };
}

def_pages! {
    Appearance,
    Simulator,
}

impl Backend {
    const fn text(self) -> &'static str {
        match self {
            #[cfg(not(target_arch = "wasm32"))]
            Self::Builtin => "Builtin",
            Self::External => "External",
        }
    }
}

#[cfg(not(target_arch = "wasm32"))]
impl crate::native_main::SimulationEngine {
    const fn text(self) -> &'static str {
        match self {
            Self::Gsim => "Gsim",
            Self::GsimCompute => "Gsim Compute",
        }
    }
}

fn update_simulator_settings(ui: &mut Ui, settings: &mut AppSettings) {
    ui.horizontal(|ui| {
        ui.label("Backend");
        ComboBox::from_id_salt("backend_selector")
            .selected_text(settings.backend.text())
            .show_ui(ui, |ui| {
                for &backend in Backend::ALL {
                    ui.selectable_value(&mut settings.backend, backend, backend.text());
                }
            });
    });

    ui.separator();

    match settings.backend {
        #[cfg(not(target_arch = "wasm32"))]
        Backend::Builtin => {
            ui.horizontal(|ui| {
                ui.label("Engine");
                ComboBox::from_id_salt("builting_engine_selector")
                    .selected_text(settings.builtin_backend_engine.text())
                    .show_ui(ui, |ui| {
                        for &engine in crate::native_main::SimulationEngine::ALL {
                            ui.selectable_value(
                                &mut settings.builtin_backend_engine,
                                engine,
                                engine.text(),
                            );
                        }
                    });
            });
        }
        Backend::External => {
            ui.horizontal(|ui| {
                ui.label("Server address");
                let mut host_str = settings.external_backend_addr.0.to_string();
                ui.text_edit_singleline(&mut host_str);
                if host_str.as_str() != settings.external_backend_addr.0.as_str() {
                    settings.external_backend_addr.0 = host_str.into();
                }
            });

            ui.horizontal(|ui| {
                ui.label("Server port");
                let mut port_str = settings.external_backend_addr.1.to_string();
                ui.text_edit_singleline(&mut port_str);
                if let Ok(port) = port_str.parse::<u16>() {
                    settings.external_backend_addr.1 = port;
                }
            });
        }
    }
}

struct TabViewer<'a> {
    context: &'a Context,
    settings: &'a mut AppSettings,
}

impl egui_dock::TabViewer for TabViewer<'_> {
    type Tab = Page;

    fn title(&mut self, tab: &mut Self::Tab) -> WidgetText {
        match *tab {
            Page::Appearance => "Appearance".into(),
            Page::Simulator => "Simulator".into(),
        }
    }

    fn ui(&mut self, ui: &mut Ui, tab: &mut Self::Tab) {
        match *tab {
            Page::Appearance => {
                let theme = if self.settings.dark_mode {
                    Theme::Dark
                } else {
                    Theme::Light
                };
                self.context.style_ui(ui, theme)
            }
            Page::Simulator => update_simulator_settings(ui, self.settings),
        }
    }
}

fn update_settings_window(
    egui: Res<Egui>,
    mut dock_state: NonSendMut<DockState<Page>>,
    mut settings: ResMut<AppSettings>,
    mut open_windows: ResMut<OpenWindows>,
) {
    let mut tab_viewer = TabViewer {
        context: &egui.context,
        settings: &mut settings,
    };

    Window::new("Settings")
        .open(&mut open_windows.settings)
        .collapsible(false)
        .show(&egui.context, |ui| {
            DockArea::new(&mut dock_state)
                .id("settings_dock_area".into())
                .show_close_buttons(false)
                .tab_context_menus(false)
                .draggable_tabs(false)
                .show_window_close_buttons(false)
                .show_window_collapse_buttons(false)
                .style(egui_dock::Style::from_egui(egui.context.style().as_ref()))
                .show_inside(ui, &mut tab_viewer);
        });
}

#[derive(Debug, Default)]
pub struct SettingsPlugin;

impl bevy_app::Plugin for SettingsPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.insert_non_send_resource(DockState::new(Page::ALL.to_vec()));
        app.add_systems(bevy_app::Update, update_settings_window);
    }
}
