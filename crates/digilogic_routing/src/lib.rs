mod bit_grid;
mod fixup;
pub mod graph;
mod path_finding;
mod routing;
mod segment_tree;

use aery::edges::{EdgeInfo, Edges};
use aery::prelude::*;
use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_ecs::system::SystemParam;
use bevy_log::info_span;
use bevy_reflect::Reflect;
use bevy_tasks::prelude::*;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::{fixed, Fixed};
use serde::{Deserialize, Serialize};
use smallvec::SmallVec;
use tracing::Instrument;

const MIN_WIRE_SPACING: Fixed = fixed!(10);

#[derive(Default, Debug, Component, Reflect)]
#[component(storage = "SparseSet")]
struct GraphDirty;

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Reflect)]
pub enum VertexKind {
    #[default]
    Normal,
    Dummy,
    WireStart {
        is_root: bool,
    },
    WireEnd {
        junction_kind: Option<JunctionKind>,
    },
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Reflect)]
pub enum JunctionKind {
    #[default]
    LineSegment,
    Corner,
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Reflect)]
pub struct Junction {
    pub vertex_index: u32,
    pub kind: JunctionKind,
}

#[derive(Default, Debug, Reflect)]
pub struct Vertex {
    pub position: Vec2,
    pub kind: VertexKind,
    pub connected_junctions: SmallVec<[Junction; 2]>,
}

#[derive(Default, Debug, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct Vertices(Vec<Vertex>);

#[derive(Debug, Resource, Reflect, Serialize, Deserialize)]
#[reflect(Resource)]
pub struct RoutingConfig {
    pub prune_graph: bool,
}

impl Default for RoutingConfig {
    fn default() -> Self {
        Self { prune_graph: true }
    }
}

#[derive(SystemSet, Debug, Clone, PartialEq, Eq, Hash)]
pub struct RoutingSet;

#[derive(Debug, Event, Reflect)]
pub struct RoutingComplete {
    pub circuit: CircuitID,
}

type CircuitQuery<'w, 's> = Query<
    'w,
    's,
    (
        (Entity, Write<graph::Graph>, Edges<Child>),
        Relations<Child>,
    ),
    (With<Circuit>, With<GraphDirty>),
>;

type SymbolQuery<'w, 's> =
    Query<'w, 's, ((Entity, Read<AbsoluteBoundingBox>), Relations<Child>), With<Symbol>>;
type PortQuery<'w, 's> =
    Query<'w, 's, (Read<GlobalTransform>, Read<AbsoluteDirections>), With<Port>>;
type NetQuery<'w, 's> = Query<'w, 's, ((Entity, Write<Vertices>), Relations<Child>), With<Net>>;
type EndpointQuery<'w, 's> =
    Query<'w, 's, (Entity, Read<GlobalTransform>, Has<PortID>), With<Endpoint>>;

#[derive(SystemParam)]
struct CircuitTree<'w, 's> {
    symbols: SymbolQuery<'w, 's>,
    ports: PortQuery<'w, 's>,
    nets: NetQuery<'w, 's>,
    endpoints: EndpointQuery<'w, 's>,
}

fn route(
    mut commands: Commands,
    config: Res<RoutingConfig>,
    mut circuits: CircuitQuery,
    mut tree: CircuitTree,
    mut routing_complete_events: EventWriter<RoutingComplete>,
) {
    for ((circuit, mut graph, circuit_edges), circuit_children) in circuits.iter_mut() {
        commands.entity(circuit).remove::<GraphDirty>();
        graph.build(&circuit_children, &tree, config.prune_graph);

        ComputeTaskPool::get().scope(|scope| {
            for &child in circuit_edges.hosts() {
                let child = unsafe {
                    // SAFETY: `hosts()` never returns the same entity more than once.
                    tree.nets.get_unchecked(child)
                };

                if let Ok(((_, vertices), net_children)) = child {
                    scope.spawn({
                        let span = info_span!("route_net");

                        async {
                            let mut vertices = vertices;
                            let net_children = net_children;

                            routing::connect_net(
                                &graph,
                                &mut vertices.0,
                                &net_children,
                                &tree.endpoints,
                            )
                            .unwrap();
                        }
                        .instrument(span)
                    });
                }
            }
        });

        fixup::separate_wires(&circuit_children, &mut tree.nets);

        routing_complete_events.send(RoutingComplete {
            circuit: CircuitID(circuit),
        });
    }
}

fn inject_graph(trigger: Trigger<OnAdd, Circuit>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert((graph::Graph::default(), GraphDirty));
}

fn inject_vertices(trigger: Trigger<OnAdd, Net>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert(Vertices::default());
}

fn route_on_config_change(
    mut commands: Commands,
    config: Res<RoutingConfig>,
    circuits: Query<Entity, With<Circuit>>,
) {
    if config.is_changed() {
        for circuit in circuits.iter() {
            commands.entity(circuit).insert(GraphDirty);
        }
    }
}

#[allow(clippy::type_complexity)]
fn route_on_symbol_change(
    mut commands: Commands,
    circuits: Query<Entity, With<Circuit>>,
    symbols: Query<((), Relations<Child>), (With<Symbol>, Changed<GlobalTransform>)>,
) {
    for (_, edges) in symbols.iter() {
        edges.join::<Up<Child>>(&circuits).for_each(|circuit| {
            commands.entity(circuit).insert(GraphDirty);
        });
    }
}

#[allow(clippy::type_complexity)]
fn route_on_endpoint_change(
    mut commands: Commands,
    circuits: Query<Entity, With<Circuit>>,
    nets: Query<((), Relations<Child>), With<Net>>,
    endpoints: Query<
        ((), Relations<Child>),
        (With<Endpoint>, Without<PortID>, Changed<GlobalTransform>),
    >,
) {
    for (_, edges) in endpoints.iter() {
        edges.join::<Up<Child>>(&nets).for_each(|(_, edges)| {
            edges.join::<Up<Child>>(&circuits).for_each(|circuit| {
                commands.entity(circuit).insert(GraphDirty);
            });
        });
    }
}

#[derive(Debug, Default)]
pub struct RoutingPlugin;

impl bevy_app::Plugin for RoutingPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Vertices>()
            .register_type::<RoutingConfig>()
            .register_type::<GraphDirty>();

        app.init_resource::<RoutingConfig>();
        app.add_event::<RoutingComplete>();
        app.add_observer(inject_graph);
        app.add_observer(inject_vertices);
        app.add_systems(bevy_app::PreUpdate, route.in_set(RoutingSet));
        app.add_systems(bevy_app::PostUpdate, route_on_config_change);
        app.add_systems(
            bevy_app::PostUpdate,
            (route_on_symbol_change, route_on_endpoint_change).after(TransformSet),
        );
    }
}
