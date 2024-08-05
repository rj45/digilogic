mod canvas;
use canvas::*;

mod draw;
use draw::*;

use crate::{AppState, FileDialogEvent};
use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_ecs::system::SystemParam;
use bevy_hierarchy::prelude::*;
use bevy_reflect::Reflect;
use digilogic_core::components::{Circuit, CircuitID, Name, Viewport};
use digilogic_core::events::{LoadedEvent, UnloadedEvent};
use egui::*;
use egui_dock::*;
use egui_wgpu::RenderState;

const MIN_LINEAR_ZOOM: f32 = 0.0;
const MAX_LINEAR_ZOOM: f32 = 1.0;
const MIN_ZOOM: f32 = 0.125;
const MAX_ZOOM: f32 = 8.0;

#[inline]
fn zoom_to_linear(zoom: f32) -> f32 {
    let b = (MAX_ZOOM / MIN_ZOOM).ln() / (MAX_LINEAR_ZOOM - MIN_LINEAR_ZOOM);
    let a = MIN_ZOOM * (-b * MIN_LINEAR_ZOOM).exp();

    ((zoom * zoom) / a).ln() / b
}

#[inline]
fn linear_to_zoom(linear: f32) -> f32 {
    let b = (MAX_ZOOM / MIN_ZOOM).ln() / (MAX_LINEAR_ZOOM - MIN_LINEAR_ZOOM);
    let a = MIN_ZOOM * (-b * MIN_LINEAR_ZOOM).exp();

    (a * (b * linear).exp()).sqrt()
}

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

#[derive(Debug, Clone, Copy, Component, Reflect)]
#[repr(transparent)]
struct ViewportCount(u16);

#[derive(Debug, Clone, Copy, Component)]
struct PanZoom {
    pan: Vec2,
    zoom: f32,
}

impl Default for PanZoom {
    #[inline]
    fn default() -> Self {
        Self {
            pan: Vec2::ZERO,
            zoom: 1.0,
        }
    }
}

#[derive(Default, Deref, DerefMut, Component)]
#[repr(transparent)]
struct Scene(vello::Scene);

#[derive(Bundle)]
struct ViewportBundle {
    viewport: Viewport,
    circuit: CircuitID,
    pan_zoom: PanZoom,
    scene: Scene,
    canvas: Canvas,
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
                ui.close_menu();
            }

            if ui.button("Save").clicked() {
                file_dialog_events.send(FileDialogEvent::Save);
                ui.close_menu();
            }

            ui.separator();

            #[cfg(not(target_arch = "wasm32"))]
            if ui.button("Quit").clicked() {
                egui.context.send_viewport_cmd(ViewportCommand::Close);
            }
        });
        ui.add_space(16.0);

        ui.with_layout(Layout::top_down(Align::RIGHT), |ui| {
            egui::widgets::global_dark_light_mode_switch(ui);
            app_state.dark_mode = egui.context.style().visuals.dark_mode;
        });
    });
}

fn update_menu(
    egui: Res<Egui>,
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
}

#[allow(clippy::too_many_arguments)] // TODO: fixme
fn update_viewport(
    egui: &Egui,
    ui: &mut Ui,
    renderer: &mut CanvasRenderer,
    pan_zoom: &mut PanZoom,
    scene: &Scene,
    canvas: &mut Canvas,
    input_events: &mut EventWriter<crate::ux::InputEvent>,
    viewport: Entity,
) {
    TopBottomPanel::bottom("status_bar")
        .show_separator_line(false)
        .show_inside(ui, |ui| {
            ui.label(format!("{:.0}%", pan_zoom.zoom * pan_zoom.zoom * 100.0));
        });

    CentralPanel::default().show_inside(ui, |ui| {
        let canvas_size = ui.available_size();
        let canvas_width = (canvas_size.x.floor() as u32).max(1);
        let canvas_height = (canvas_size.y.floor() as u32).max(1);

        canvas.resize(&egui.render_state, canvas_width, canvas_height);
        canvas.render(
            renderer,
            &egui.render_state,
            &scene.0,
            ui.visuals().extreme_bg_color,
        );

        let response = Image::new((canvas.texture_id(), canvas_size))
            .ui(ui)
            .interact(Sense::click_and_drag());

        if response.dragged_by(PointerButton::Middle) {
            pan_zoom.pan += response.drag_delta() / pan_zoom.zoom;
        }

        if let Some(mouse_pos) = response.hover_pos() {
            let old_mouse_world_pos =
                (mouse_pos - response.rect.left_top()) / pan_zoom.zoom - pan_zoom.pan;

            let linear = zoom_to_linear(pan_zoom.zoom);
            let linear_delta = ui.input(|state| state.smooth_scroll_delta.y) / 600.0;
            let linear = (linear + linear_delta).clamp(MIN_LINEAR_ZOOM, MAX_LINEAR_ZOOM);
            pan_zoom.zoom = linear_to_zoom(linear);

            let new_mouse_world_pos =
                (mouse_pos - response.rect.left_top()) / pan_zoom.zoom - pan_zoom.pan;

            pan_zoom.pan += new_mouse_world_pos - old_mouse_world_pos;

            // note: this will only happen if the mouse is hovering the viewport
            forward_hover_events(ui, input_events, new_mouse_world_pos, viewport);
        }
    });
}

fn forward_hover_events(
    ui: &mut Ui,
    input_events: &mut EventWriter<crate::ux::InputEvent>,
    world_mouse_pos: Vec2,
    viewport: Entity,
) {
    ui.input(|state| {
        for event in state.events.iter() {
            match event {
                egui::Event::PointerMoved(_) => {
                    input_events.send(crate::ux::InputEvent {
                        event: egui::Event::PointerMoved(
                            (world_mouse_pos.x, world_mouse_pos.y).into(),
                        ),
                        viewport,
                    });
                }
                egui::Event::PointerButton {
                    button,
                    pressed,
                    modifiers,
                    ..
                } => {
                    input_events.send(crate::ux::InputEvent {
                        event: egui::Event::PointerButton {
                            pos: (world_mouse_pos.x, world_mouse_pos.y).into(),
                            button: *button,
                            pressed: *pressed,
                            modifiers: *modifiers,
                        },
                        viewport,
                    });
                }
                // TODO: forward other events
                _ => {}
            }
        }
    });
}

#[derive(SystemParam)]
struct TabViewer<'w, 's> {
    commands: Commands<'w, 's>,
    egui: Res<'w, Egui>,
    renderer: NonSendMut<'w, CanvasRenderer>,
    unloaded_events: EventWriter<'w, UnloadedEvent>,
    viewports: Query<'w, 's, (Read<CircuitID>, Write<PanZoom>, Read<Scene>, Write<Canvas>)>,
    circuits: Query<'w, 's, (Option<Read<Name>>, Write<ViewportCount>)>,
    input_events: EventWriter<'w, crate::ux::InputEvent>,
}

impl egui_dock::TabViewer for TabViewer<'_, '_> {
    type Tab = Entity;

    fn title(&mut self, tab: &mut Self::Tab) -> WidgetText {
        if let Ok((&circuit, _, _, _)) = self.viewports.get(*tab) {
            if let Ok((Some(name), _)) = self.circuits.get(circuit.0) {
                return name.0.as_str().into();
            }
        }

        format!("{}", *tab).into()
    }

    fn ui(&mut self, ui: &mut Ui, tab: &mut Self::Tab) {
        if let Ok((_, mut pan_zoom, scene, mut canvas)) = self.viewports.get_mut(*tab) {
            update_viewport(
                &self.egui,
                ui,
                &mut self.renderer,
                &mut pan_zoom,
                scene,
                &mut canvas,
                &mut self.input_events,
                *tab,
            );
        }
    }

    fn id(&mut self, tab: &mut Self::Tab) -> Id {
        Id::new(*tab)
    }

    fn on_close(&mut self, tab: &mut Self::Tab) -> bool {
        self.commands.entity(*tab).despawn();

        if let Ok((&circuit, _, _, _)) = self.viewports.get(*tab) {
            if let Ok((_, mut viewport_count)) = self.circuits.get_mut(circuit.0) {
                viewport_count.0 -= 1;

                if viewport_count.0 == 0 {
                    // TODO: show confirmation prompt if circuit contains changes
                    self.commands.entity(circuit.0).despawn_recursive();
                    self.unloaded_events.send(UnloadedEvent { circuit });
                }
            }
        }

        true
    }

    fn scroll_bars(&self, _tab: &Self::Tab) -> [bool; 2] {
        [false; 2]
    }
}

fn update_tabs(mut dock_state: NonSendMut<DockState<Entity>>, mut tab_viewer: TabViewer) {
    let context = tab_viewer.egui.context.clone();

    CentralPanel::default().show(&context, |ui| {
        DockArea::new(&mut dock_state)
            .style(egui_dock::Style::from_egui(context.style().as_ref()))
            .show_inside(ui, &mut tab_viewer);
    });
}

fn add_tabs(
    mut commands: Commands,
    egui: Res<Egui>,
    mut dock_state: NonSendMut<DockState<Entity>>,
    mut loaded_events: EventReader<LoadedEvent>,
    mut circuits: Query<Option<&mut ViewportCount>, With<Circuit>>,
) {
    for loaded_event in loaded_events.read() {
        if let Ok(Some(mut viewport_count)) = circuits.get_mut(loaded_event.circuit.0) {
            viewport_count.0 += 1;
        } else {
            commands
                .entity(loaded_event.circuit.0)
                .insert(ViewportCount(1));
        }

        let viewport = commands
            .spawn(ViewportBundle {
                viewport: Viewport,
                circuit: loaded_event.circuit,
                pan_zoom: PanZoom::default(),
                scene: Scene::default(),
                canvas: Canvas::create(&egui.render_state),
            })
            .id();

        dock_state.main_surface_mut().push_to_first_leaf(viewport);
    }
}

#[cfg(feature = "inspector")]
fn inspect(world: &mut World) {
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
        app.insert_non_send_resource(DockState::<Entity>::new(Vec::new()));
        app.insert_non_send_resource(CanvasRenderer::new(&self.render_state));
        app.insert_resource(Egui::new(&self.context, &self.render_state));
        app.insert_resource(SymbolShapes(Vec::new()));
        app.register_type::<ViewportCount>()
            .register_type::<Viewport>();
        app.add_systems(bevy_app::Startup, init_symbol_shapes);
        app.add_systems(bevy_app::Update, (draw, update_menu, add_tabs));
        app.add_systems(bevy_app::Update, update_tabs.after(draw).after(update_menu));

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
