mod canvas;
use canvas::*;

mod draw;
use digilogic_ux::DragType;
use draw::*;

mod settings;
use settings::*;

mod explorer;
use explorer::*;

mod palette;
use palette::*;

use crate::{AppSettings, Backend, FileDialogEvent, DEFAULT_LOCAL_SERVER_ADDR};
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_ecs::system::SystemParam;
use bevy_reflect::Reflect;
use bevy_state::prelude::*;
use digilogic_core::components::{Circuit, CircuitID, Name, Viewport};
use digilogic_core::resources::Project;
use digilogic_core::states::{SimulationConnected, SimulationState};
use digilogic_core::{fixed, Fixed, SharedStr};
use egui::*;
use egui_dock::*;
use egui_wgpu::RenderState;
use std::sync::{Arc, Mutex, MutexGuard};
use vello::peniko::Font;

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

// Variant order corresponds to draw order
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Component, Reflect)]
#[repr(u8)]
enum Layer {
    Symbol,
    RoutingGraph,
    Wire,
    Port,
    BoundingBox,
}

#[derive(Default, Component)]
struct Scene {
    layers: [Mutex<vello::Scene>; 5],
    combined: vello::Scene,
}

impl Scene {
    #[inline]
    fn for_layer(&self, layer: Layer) -> MutexGuard<vello::Scene> {
        self.layers[layer as usize].lock().unwrap()
    }
}

#[derive(Bundle)]
struct ViewportBundle {
    viewport: Viewport,
    circuit: CircuitID,
    pan_zoom: PanZoom,
    scene: Scene,
    canvas: Canvas,
}

fn combine_scenes(
    app_state: Res<AppSettings>,
    mut viewports: Query<(&PanZoom, &mut Scene), With<Viewport>>,
) {
    for (pan_zoom, mut scene) in viewports.iter_mut() {
        let transform =
            vello::kurbo::Affine::translate((pan_zoom.pan.x as f64, pan_zoom.pan.y as f64))
                .then_scale(pan_zoom.zoom as f64);

        let scene = &mut *scene;
        scene.combined.reset();

        for (i, layer) in scene.layers.iter_mut().enumerate() {
            if i == (Layer::BoundingBox as usize) && !app_state.show_bounding_boxes {
                continue;
            }

            if i == (Layer::RoutingGraph as usize) && !app_state.show_routing_graph {
                continue;
            }

            let layer = layer.get_mut().unwrap();
            scene.combined.append(layer, Some(transform));
        }
    }
}

#[derive(Debug, Default, Resource, Reflect)]
#[reflect(Resource)]
struct OpenWindows {
    settings: bool,
}

impl OpenWindows {
    fn any(&self) -> bool {
        self.settings
    }
}

// TODO: separate responsibilities
#[allow(clippy::too_many_arguments)]
fn update_menu(
    mut commands: Commands,
    egui: Res<Egui>,
    mut settings: ResMut<AppSettings>,
    mut routing_config: ResMut<digilogic_routing::RoutingConfig>,
    mut file_dialog_events: EventWriter<FileDialogEvent>,
    mut open_windows: ResMut<OpenWindows>,
    project: Option<Res<Project>>,
    circuits: Query<Entity, With<Circuit>>,
) {
    TopBottomPanel::top("menu_panel").show(&egui.context, |ui| {
        ui.add_enabled_ui(!open_windows.any(), |ui| {
            menu::bar(ui, |ui| {
                ui.menu_button("File", |ui| {
                    if ui.button("New Project").clicked() {
                        if project.is_some() {
                            // TODO: check for unsaved changes

                            for circuit in circuits.iter() {
                                commands.entity(circuit).despawn();
                            }
                        }

                        commands.insert_resource(Project {
                            name: SharedStr::new_static("Unnamed Project"),
                            file_path: None,
                            root_circuit: None,
                        });
                        ui.close_menu();
                    }

                    if ui.button("Open Project").clicked() {
                        file_dialog_events.send(FileDialogEvent::OpenProject);
                        ui.close_menu();
                    }

                    if ui.button("Save Project").clicked() {
                        file_dialog_events.send(FileDialogEvent::SaveProject);
                        ui.close_menu();
                    }

                    ui.separator();

                    ui.add_enabled_ui(project.is_some(), |ui| {
                        if ui.button("New Circuit").clicked() {
                            // TODO
                            ui.close_menu();
                        }

                        if ui.button("Add Circuit").clicked() {
                            file_dialog_events.send(FileDialogEvent::AddCircuit);
                            ui.close_menu();
                        }

                        if ui.button("Import Circuit").clicked() {
                            file_dialog_events.send(FileDialogEvent::ImportCircuit);
                            ui.close_menu();
                        }

                        if ui.button("Save Circuit").clicked() {
                            file_dialog_events.send(FileDialogEvent::SaveCircuit);
                            ui.close_menu();
                        }
                    });

                    ui.separator();

                    #[cfg(not(target_arch = "wasm32"))]
                    if ui.button("Quit").clicked() {
                        egui.context.send_viewport_cmd(ViewportCommand::Close);
                    }
                });
                ui.add_space(8.0);

                ui.menu_button("View", |ui| {
                    ui.menu_button("Debug", |ui| {
                        ui.checkbox(&mut settings.show_bounding_boxes, "Bounding boxes");
                        ui.checkbox(&mut settings.show_routing_graph, "Routing graph");
                        ui.checkbox(&mut settings.show_root_wires, "Root wires");
                    });

                    ui.separator();

                    if ui.button("Settings").clicked() {
                        open_windows.settings = true;
                        ui.close_menu();
                    }
                });
                ui.add_space(8.0);

                ui.menu_button("Routing", |ui| {
                    let mut prune_graph = routing_config.prune_graph;
                    ui.checkbox(&mut prune_graph, "Prune graph");

                    // Don't trigger change detection if nothing changed.
                    if prune_graph != routing_config.prune_graph {
                        routing_config.prune_graph = prune_graph;
                    }
                });
                ui.add_space(8.0);

                ui.with_layout(Layout::top_down(Align::RIGHT), |ui| {
                    global_dark_light_mode_switch(ui);
                    settings.dark_mode = egui.context.style().visuals.dark_mode;
                });
            });
        });
    });
}

fn update_tool_bar(
    mut commands: Commands,
    egui: Res<Egui>,
    settings: Res<AppSettings>,
    open_windows: Res<OpenWindows>,
    project: Option<Res<Project>>,
    simulation_state: Res<State<SimulationState>>,
) {
    let root_circuit_exists = project.and_then(|project| project.root_circuit).is_some();

    TopBottomPanel::top("tool_bar_panel").show(&egui.context, |ui| {
        ui.add_enabled_ui(!open_windows.any() && root_circuit_exists, |ui| {
            menu::bar(ui, |ui| {
                match simulation_state.is_connected() {
                    false => {
                        if ui.button("Start").clicked() {
                            match settings.backend {
                                #[cfg(not(target_arch = "wasm32"))]
                                Backend::Builtin => {
                                    //let executable = std::env::current_exe().unwrap();
                                    //std::process::Command::new(executable)
                                    //    .arg("server")
                                    //    .spawn()
                                    //    .unwrap();

                                    commands.trigger(digilogic_netcode::Connect {
                                        server_addr: DEFAULT_LOCAL_SERVER_ADDR,
                                    });
                                }
                                Backend::External => {
                                    commands.trigger(digilogic_netcode::Connect {
                                        server_addr: settings.external_backend_addr.clone(),
                                    });
                                }
                            }
                        }
                    }
                    true => {
                        if ui.button("Stop").clicked() {
                            commands.trigger(digilogic_netcode::Disconnect);
                        }
                    }
                }

                ui.add_enabled_ui(**simulation_state == SimulationState::ActiveIdle, |ui| {
                    if ui.button("Step").clicked() {
                        // TODO
                    }
                });

                match **simulation_state {
                    SimulationState::ActiveIdle => {
                        if ui.button("Run").clicked() {
                            // TODO
                        }
                    }
                    SimulationState::ActiveRunning => {
                        if ui.button("Pause").clicked() {
                            // TODO
                        }
                    }
                    _ => {
                        ui.add_enabled_ui(false, |ui| ui.button("Run"));
                    }
                }
            });
        });
    });
}

fn update_status_bar(egui: Res<Egui>, open_windows: Res<OpenWindows>) {
    TopBottomPanel::bottom("status_bar_panel").show(&egui.context, |ui| {
        ui.add_enabled_ui(!open_windows.any(), |ui| {
            ui.with_layout(Layout::bottom_up(Align::RIGHT), |ui| {
                warn_if_debug_build(ui);
            });
        });
    });
}

fn update_viewport(
    egui: &Egui,
    ui: &mut Ui,
    renderer: &mut CanvasRenderer,
    (&circuit, mut pan_zoom, scene, mut canvas): (&CircuitID, Mut<PanZoom>, &Scene, Mut<Canvas>),
    commands: &mut Commands,
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
            &scene.combined,
            vello::peniko::Color::rgb8(6, 6, 6),
        );

        let response = Image::new((canvas.texture_id(), canvas_size))
            .ui(ui)
            .interact(Sense::click_and_drag());

        if response.dragged_by(PointerButton::Middle) {
            let zoom = pan_zoom.zoom;
            pan_zoom.pan += response.drag_delta() / zoom;
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
            forward_hover_events(
                ui,
                response,
                commands,
                viewport,
                circuit,
                new_mouse_world_pos,
            );
        }
    });
}

fn forward_hover_events(
    ui: &mut Ui,
    response: Response,
    commands: &mut Commands,
    viewport: Entity,
    circuit: CircuitID,
    world_mouse_pos: Vec2,
) {
    let pos = digilogic_core::transform::Vec2 {
        x: Fixed::try_from_f32(world_mouse_pos.x).unwrap(),
        y: Fixed::try_from_f32(world_mouse_pos.y).unwrap(),
    };
    let modifiers = ui.input(|state| digilogic_ux::Modifiers {
        alt: state.modifiers.alt,
        ctrl: state.modifiers.ctrl,
        shift: state.modifiers.shift,
        mac_cmd: state.modifiers.mac_cmd,
        command: state.modifiers.command,
    });

    if response.hovered() {
        commands.trigger_targets(
            digilogic_ux::HoverEvent {
                viewport,
                circuit,
                pos,
                modifiers,
            },
            viewport,
        );
    }

    for (egui_button, ux_button) in [
        (PointerButton::Primary, digilogic_ux::PointerButton::Primary),
        (
            PointerButton::Secondary,
            digilogic_ux::PointerButton::Secondary,
        ),
    ] {
        if response.clicked_by(egui_button) {
            commands.trigger_targets(
                digilogic_ux::ClickEvent {
                    viewport,
                    circuit,
                    pos,
                    button: ux_button,
                    modifiers,
                },
                viewport,
            );
        }

        let drag_type = match (
            response.drag_started_by(egui_button),
            response.dragged_by(egui_button),
            response.drag_stopped_by(egui_button),
        ) {
            (_, _, true) => DragType::End,
            (true, _, false) => DragType::Start,
            (false, true, false) => DragType::Dragging,
            _ => continue,
        };

        let delta = response.drag_delta();
        let delta = digilogic_core::transform::Vec2 {
            x: Fixed::try_from_f32(delta.x).unwrap(),
            y: Fixed::try_from_f32(delta.y).unwrap(),
        };

        commands.trigger_targets(
            digilogic_ux::DragEvent {
                drag_type,
                viewport,
                circuit,
                pos,
                delta,
                button: ux_button,
                modifiers,
            },
            viewport,
        );
    }
}

type ViewportQuery<'w, 's> =
    Query<'w, 's, (Read<CircuitID>, Write<PanZoom>, Read<Scene>, Write<Canvas>), With<Viewport>>;

//#[allow(clippy::type_complexity)]
#[derive(SystemParam)]
struct TabViewer<'w, 's> {
    commands: Commands<'w, 's>,
    egui: Res<'w, Egui>,
    renderer: NonSendMut<'w, CanvasRenderer>,
    viewports: ViewportQuery<'w, 's>,
    circuits: Query<'w, 's, Read<Name>, With<Circuit>>,
    open_windows: Res<'w, OpenWindows>,
}

impl egui_dock::TabViewer for TabViewer<'_, '_> {
    type Tab = Entity;

    fn title(&mut self, tab: &mut Self::Tab) -> WidgetText {
        let (&circuit, _, _, _) = self.viewports.get(*tab).expect("invalid viewport ID");
        let name = self.circuits.get(circuit.0).expect("invalid circuit ID");
        name.0.as_str().into()
    }

    fn ui(&mut self, ui: &mut Ui, tab: &mut Self::Tab) {
        ui.add_enabled_ui(!self.open_windows.any(), |ui| {
            let viewport_item = self.viewports.get_mut(*tab).expect("invalid viewport ID");

            update_viewport(
                &self.egui,
                ui,
                &mut self.renderer,
                viewport_item,
                &mut self.commands,
                *tab,
            );
        });
    }

    fn id(&mut self, tab: &mut Self::Tab) -> Id {
        Id::new(*tab)
    }

    fn on_close(&mut self, tab: &mut Self::Tab) -> bool {
        self.commands.entity(*tab).despawn();
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
            .id("main_dock_area".into())
            .style(egui_dock::Style::from_egui(context.style().as_ref()))
            .show_inside(ui, &mut tab_viewer);
    });
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

fn repaint(egui: Res<Egui>) {
    egui.context.request_repaint_after_secs(1.0 / 30.0);
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

#[derive(SystemSet, Debug, Clone, PartialEq, Eq, Hash)]
struct DrawSet;

#[derive(SystemSet, Debug, Clone, PartialEq, Eq, Hash)]
struct MenuSet;

const FONT_BYTES: &[u8] = include_bytes!("../assets/ClearSans-Regular.ttf");

impl bevy_app::Plugin for UiPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.insert_non_send_resource(DockState::<Entity>::new(Vec::new()));
        app.insert_non_send_resource(CanvasRenderer::new(&self.render_state));
        app.insert_resource(Egui::new(&self.context, &self.render_state));
        app.insert_resource(SymbolShapes(Vec::new()));
        app.insert_resource(VelloFont(Font::new(
            vello::peniko::Blob::new(Arc::new(FONT_BYTES)),
            0,
        )));
        app.init_resource::<OpenWindows>();
        app.register_type::<Viewport>();

        app.add_systems(bevy_app::Startup, init_symbol_shapes);

        app.add_systems(
            bevy_app::Update,
            (draw_symbols, draw_ports, draw_wires).in_set(DrawSet),
        );
        app.add_systems(
            bevy_app::Update,
            draw_bounding_boxes
                .in_set(DrawSet)
                .run_if(|app_state: Res<AppSettings>| app_state.show_bounding_boxes),
        );
        app.add_systems(
            bevy_app::Update,
            draw_routing_graph
                .in_set(DrawSet)
                .run_if(|app_state: Res<AppSettings>| app_state.show_routing_graph),
        );
        app.add_systems(bevy_app::Update, combine_scenes.after(DrawSet));

        app.add_systems(
            bevy_app::Update,
            (update_menu, update_tool_bar, update_status_bar)
                .chain()
                .in_set(MenuSet),
        );

        app.add_systems(
            bevy_app::Update,
            update_tabs
                .after(combine_scenes)
                .after(MenuSet)
                .after(ExplorerSet),
        );

        app.add_systems(
            bevy_app::PostUpdate,
            repaint.run_if(in_state(SimulationConnected)),
        );

        app.add_plugins(SettingsPlugin)
            .add_plugins(ExplorerPlugin)
            .add_plugins(PalettePlugin);

        #[cfg(feature = "inspector")]
        {
            app.add_plugins(bevy_inspector_egui::DefaultInspectorConfigPlugin);
            app.add_systems(bevy_app::Last, inspect);
        }
    }
}
