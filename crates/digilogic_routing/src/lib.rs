pub mod graph;
mod path_finding;
mod routing;
mod segment_tree;

use aery::prelude::*;
use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_ecs::system::SystemParam;
use bevy_reflect::Reflect;
use digilogic_core::components::*;
use digilogic_core::transform::*;

type HashSet<T> = ahash::AHashSet<T>;
type HashMap<K, V> = ahash::AHashMap<K, V>;

#[derive(Default, Debug, Component, Reflect)]
#[component(storage = "SparseSet")]
struct GraphDirty;

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Reflect)]
pub enum VertexKind {
    #[default]
    Normal,
    WireStart {
        is_root: bool,
    },
    WireEnd {
        is_junction: bool,
    },
}

#[derive(Default, Debug, Reflect)]
pub struct Vertex {
    pub position: Vec2,
    pub kind: VertexKind,
}

#[derive(Default, Debug, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct Vertices(Vec<Vertex>);

#[derive(Debug, Resource, Reflect)]
#[reflect(Resource)]
pub struct RoutingConfig {
    pub minimal: bool,
    pub perform_centering: bool,
}

impl Default for RoutingConfig {
    fn default() -> Self {
        Self {
            minimal: true,
            perform_centering: true,
        }
    }
}

type CircuitQuery<'w, 's> = Query<
    'w,
    's,
    ((Entity, Write<graph::Graph>), Relations<Child>),
    (With<Circuit>, With<GraphDirty>),
>;

#[allow(clippy::type_complexity)]
#[derive(SystemParam)]
struct CircuitTree<'w, 's> {
    symbols: Query<'w, 's, (Read<AbsoluteBoundingBox>, Relations<Child>), With<Symbol>>,
    ports: Query<'w, 's, (Read<GlobalTransform>, Read<AbsoluteDirections>), With<Port>>,
    nets: Query<'w, 's, (Write<Vertices>, Relations<Child>), With<Net>>,
    endpoints: Query<'w, 's, ((Entity, Read<GlobalTransform>), Relations<Child>), With<Endpoint>>,
    waypoints: Query<'w, 's, Read<GlobalTransform>, With<Waypoint>>,
}

fn route(
    mut commands: Commands,
    config: Res<RoutingConfig>,
    mut circuits: CircuitQuery,
    mut tree: CircuitTree,
) {
    for ((circuit, mut graph), circuit_children) in circuits.iter_mut() {
        commands.entity(circuit).remove::<GraphDirty>();
        graph.build(&circuit_children, &tree, config.minimal);

        //circuit_children
        //    .join::<Child>(&mut tree.nets)
        //    .for_each(|(mut vertices, net_children)| {
        //        routing::connect_net(
        //            &graph,
        //            &mut vertices.0,
        //            &net_children,
        //            &tree.endpoints,
        //            &tree.waypoints,
        //            config.perform_centering,
        //        )
        //        .unwrap();
        //    });
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

#[derive(Debug, Default)]
pub struct RoutingPlugin;

impl bevy_app::Plugin for RoutingPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Vertices>()
            .register_type::<RoutingConfig>()
            .register_type::<GraphDirty>();

        app.init_resource::<RoutingConfig>();
        app.observe(inject_graph);
        app.observe(inject_vertices);
        app.add_systems(bevy_app::PreUpdate, route);
        app.add_systems(bevy_app::Update, route_on_symbol_change);
    }
}
