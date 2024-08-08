pub mod graph;
mod segment_tree;

use aery::prelude::*;
use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::Write;
use bevy_reflect::Reflect;
use digilogic_core::components::*;
use digilogic_core::transform::*;
use digilogic_core::Fixed;

type HashMap<K, V> = ahash::AHashMap<K, V>;

#[derive(Default, Debug, Deref, Component)]
#[repr(transparent)]
pub struct Graph(graph::GraphData);

#[derive(Default, Debug, Component, Reflect)]
#[component(storage = "SparseSet")]
struct GraphDirty;

#[derive(Default, Debug, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct Vertices(Vec<[Fixed; 2]>);

#[derive(Debug, Resource, Reflect)]
#[reflect(Resource)]
pub struct RoutingConfig {
    pub minimal: bool,
}

impl Default for RoutingConfig {
    fn default() -> Self {
        Self { minimal: true }
    }
}

type CircuitQuery<'w, 's> =
    Query<'w, 's, ((Entity, Write<Graph>), Relations<Child>), (With<Circuit>, With<GraphDirty>)>;

fn route(
    mut commands: Commands,
    config: Res<RoutingConfig>,
    mut circuits: CircuitQuery,
    symbols: Query<(&AbsoluteBoundingBox, Relations<Child>), With<Symbol>>,
    ports: Query<(&GlobalTransform, &AbsoluteDirections), With<Port>>,
    //mut nets: Query<(&Net, &mut Vertices)>,
    //endpoints: Query<(&Endpoint, &GlobalTransform)>,
    //waypoints: Query<&GlobalTransform, With<Waypoint>>,
) {
    for ((circuit, mut graph), circuit_children) in circuits.iter_mut() {
        commands.entity(circuit).remove::<GraphDirty>();
        graph
            .0
            .build(&circuit_children, &symbols, &ports, config.minimal);

        //for &circuit_child in circuit_children {
        //    if let Ok((net_children, net_vertices)) = nets.get_mut(circuit_child) {
        //        for &net_child in net_children {
        //            if let Ok((endpoint_position, endpoint_children)) = endpoints.get(net_child) {}
        //        }
        //    }
        //}
    }
}

fn inject_graph(trigger: Trigger<OnAdd, Circuit>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert((Graph::default(), GraphDirty));
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
        app.register_type::<Vertices>();
        app.register_type::<RoutingConfig>();
        app.init_resource::<RoutingConfig>();
        app.observe(inject_graph);
        app.observe(inject_vertices);
        app.add_systems(bevy_app::PreUpdate, route);
        app.add_systems(bevy_app::Update, route_on_symbol_change);
    }
}
