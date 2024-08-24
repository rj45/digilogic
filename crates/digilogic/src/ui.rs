mod canvas;
use canvas::*;

mod draw;
use draw::*;

use crate::{AppSettings, AppState, FileDialogEvent};
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_ecs::system::SystemParam;
use bevy_reflect::Reflect;
use digilogic_core::components::{Circuit, CircuitID, Name, Viewport};
use digilogic_core::events::{LoadedEvent, UnloadedEvent};
use egui::*;
use egui_dock::*;
use egui_wgpu::RenderState;
use std::sync::{Mutex, MutexGuard};

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

fn update_main_menu(
    egui: &Egui,
    ui: &mut Ui,
    app_state: &mut AppSettings,
    mut routing_config: ResMut<digilogic_routing::RoutingConfig>,
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
        ui.add_space(8.0);

        ui.menu_button("View", |ui| {
            ui.checkbox(&mut app_state.show_bounding_boxes, "Bounding boxes");
            ui.checkbox(&mut app_state.show_routing_graph, "Routing graph");
            ui.checkbox(&mut app_state.show_root_wires, "Root wires");
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
            app_state.dark_mode = egui.context.style().visuals.dark_mode;
        });
    });
}

fn update_menu(
    egui: Res<Egui>,
    mut app_state: ResMut<AppSettings>,
    routing_config: ResMut<digilogic_routing::RoutingConfig>,
    mut file_dialog_events: EventWriter<FileDialogEvent>,
) {
    TopBottomPanel::top("top_panel").show(&egui.context, |ui| {
        update_main_menu(
            &egui,
            ui,
            &mut app_state,
            routing_config,
            &mut file_dialog_events,
        );
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
            forward_hover_events(ui, commands, new_mouse_world_pos, viewport);
        }
    });
}

fn forward_hover_events(
    ui: &mut Ui,
    commands: &mut Commands,
    world_mouse_pos: Vec2,
    viewport: Entity,
) {
    ui.input(|state| {
        for event in state.events.iter() {
            match event {
                egui::Event::PointerMoved(_) => {
                    commands.trigger_targets(
                        crate::ux::PointerMovedEvent((world_mouse_pos.x, world_mouse_pos.y).into()),
                        viewport,
                    );
                }
                egui::Event::PointerButton {
                    button,
                    pressed,
                    modifiers,
                    ..
                } => {
                    commands.trigger_targets(
                        crate::ux::PointerButtonEvent {
                            pos: (world_mouse_pos.x, world_mouse_pos.y).into(),
                            button: *button,
                            pressed: *pressed,
                            modifiers: *modifiers,
                        },
                        viewport,
                    );
                }
                // TODO: forward other events
                _ => {}
            }
        }
    });
}

#[allow(clippy::type_complexity)]
#[derive(SystemParam)]
struct TabViewer<'w, 's> {
    commands: Commands<'w, 's>,
    egui: Res<'w, Egui>,
    renderer: NonSendMut<'w, CanvasRenderer>,
    unloaded_events: EventWriter<'w, UnloadedEvent>,
    viewports: Query<'w, 's, (Read<CircuitID>, Write<PanZoom>, Read<Scene>, Write<Canvas>)>,
    circuits: Query<'w, 's, (Option<Read<Name>>, Write<ViewportCount>)>,
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
                &mut self.commands,
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
                    self.commands.entity(circuit.0).despawn();
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

impl bevy_app::Plugin for UiPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.insert_non_send_resource(DockState::<Entity>::new(Vec::new()));
        app.insert_non_send_resource(CanvasRenderer::new(&self.render_state));
        app.insert_resource(Egui::new(&self.context, &self.render_state));
        app.insert_resource(SymbolShapes(Vec::new()));
        app.register_type::<ViewportCount>()
            .register_type::<Viewport>();

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
        app.add_systems(bevy_app::Update, (update_menu, add_tabs));
        app.add_systems(
            bevy_app::Update,
            update_tabs.after(combine_scenes).after(update_menu),
        );

        app.add_systems(
            bevy_app::PostUpdate,
            repaint.run_if(bevy_state::condition::in_state(AppState::Simulating)),
        );

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
