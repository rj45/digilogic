use super::{Canvas, Egui, MenuSet, OpenWindows, ViewportBundle};
use bevy_derive::{Deref, DerefMut};
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::Read;
use bevy_ecs::system::SystemParam;
use bevy_reflect::Reflect;
use digilogic_core::components::{Circuit, CircuitID, Name, Viewport};
use digilogic_core::resources::Project;
use digilogic_core::SharedStr;
use egui::*;
use egui_dock::*;
use egui_wgpu::RenderState;

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Reflect)]
enum EditState {
    #[default]
    NotEditing,
    BeginEditing,
    Editing,
}

fn show_editable_name(
    ui: &mut Ui,
    edit_state: &mut EditState,
    buffer: &mut String,
    name: &mut SharedStr,
) -> bool {
    match *edit_state {
        EditState::NotEditing => {
            let response = ui.selectable_label(false, name.as_str());
            if response.double_clicked() {
                *edit_state = EditState::BeginEditing;
            }

            response.clicked()
        }
        EditState::BeginEditing => {
            buffer.clear();
            buffer.push_str(name.as_str());
            ui.text_edit_singleline(buffer).request_focus();
            *edit_state = EditState::Editing;

            false
        }
        EditState::Editing => {
            if ui.text_edit_singleline(buffer).lost_focus() {
                if ui.input(|i| i.key_pressed(Key::Enter)) && (buffer.as_str() != name.as_str()) {
                    *name = buffer.as_str().into();
                }
                *edit_state = EditState::NotEditing;
            }

            false
        }
    }
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Deref, DerefMut, Reflect, Component)]
#[repr(transparent)]
struct NameEditState(EditState);

fn inject_name_edit_state(trigger: Trigger<OnAdd, Circuit>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert(NameEditState::default());
}

#[derive(SystemParam)]
struct ViewportSpawner<'w, 's> {
    commands: Commands<'w, 's>,
    dock_state: NonSendMut<'w, DockState<Entity>>,
    viewports: Query<'w, 's, (Entity, Read<CircuitID>), With<Viewport>>,
}

impl ViewportSpawner<'_, '_> {
    fn focus_or_spawn_viewport(&mut self, circuit: CircuitID, render_state: &RenderState) {
        for (viewport, &viewport_circuit) in self.viewports.iter() {
            if viewport_circuit == circuit {
                let index = self
                    .dock_state
                    .find_tab(&viewport)
                    .expect("viewport without tab");
                self.dock_state.set_active_tab(index);
                return;
            }
        }

        let viewport = self
            .commands
            .spawn(ViewportBundle {
                viewport: Viewport,
                circuit,
                pan_zoom: Default::default(),
                scene: Default::default(),
                canvas: Canvas::create(render_state),
            })
            .id();

        self.dock_state
            .main_surface_mut()
            .push_to_first_leaf(viewport);
    }
}

fn update_explorer(
    egui: Res<Egui>,
    open_windows: Res<OpenWindows>,
    mut project: Option<ResMut<Project>>,
    mut project_name_edit_state: Local<EditState>,
    mut circuits: Query<(Entity, &mut Name, &mut NameEditState), With<Circuit>>,
    mut edit_buffer: Local<String>,
    mut viewport_spawner: ViewportSpawner,
) {
    SidePanel::left("explorer_panel")
        .resizable(true)
        .show(&egui.context, |ui| {
            ui.add_enabled_ui(!open_windows.any(), |ui| {
                if let Some(project) = project.as_deref_mut() {
                    collapsing_header::CollapsingState::load_with_default_open(
                        ui.ctx(),
                        "project_header".into(),
                        true,
                    )
                    .show_header(ui, |ui| {
                        show_editable_name(
                            ui,
                            &mut project_name_edit_state,
                            &mut edit_buffer,
                            &mut project.name,
                        );
                    })
                    .body(|ui| {
                        for (circuit_id, mut circuit_name, mut circuit_name_edit_state) in
                            circuits.iter_mut()
                        {
                            if project
                                .root_circuit
                                .is_some_and(|root_id| root_id.0 == circuit_id)
                            {
                                // TODO: visually mark root circuit
                            }

                            let clicked = show_editable_name(
                                ui,
                                &mut circuit_name_edit_state,
                                &mut edit_buffer,
                                &mut circuit_name.0,
                            );

                            if clicked {
                                viewport_spawner.focus_or_spawn_viewport(
                                    CircuitID(circuit_id),
                                    &egui.render_state,
                                );
                            }
                        }
                    });
                } else {
                    *project_name_edit_state = EditState::NotEditing;
                }
            });

            ui.allocate_space(ui.available_size_before_wrap());
        });
}

#[derive(SystemSet, Debug, Clone, PartialEq, Eq, Hash)]
pub struct ExplorerSet;

#[derive(Debug, Default)]
pub struct ExplorerPlugin;

impl bevy_app::Plugin for ExplorerPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<EditState>()
            .register_type::<NameEditState>();
        app.observe(inject_name_edit_state);
        app.configure_sets(bevy_app::Update, ExplorerSet.after(MenuSet));
        app.add_systems(bevy_app::Update, update_explorer.in_set(ExplorerSet));
    }
}
