pub mod graph;
mod segment_tree;

use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_hierarchy::prelude::*;
use bevy_reflect::Reflect;
use digilogic_core::components::*;
use digilogic_core::events::LoadedEvent;
use digilogic_core::transform::*;
use digilogic_core::Fixed;

type HashSet<T> = ahash::AHashSet<T>;
type HashMap<K, V> = ahash::AHashMap<K, V>;

#[derive(Default, Deref, Component)]
#[repr(transparent)]
pub struct Graph(graph::GraphData);

#[derive(Default, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct Vertices(Vec<[Fixed; 2]>);

#[derive(Default, Event)]
pub struct RouteEvent;

#[derive(Resource, Reflect)]
#[reflect(Resource)]
pub struct RoutingConfig {
    pub minimal: bool,
}

impl Default for RoutingConfig {
    fn default() -> Self {
        Self { minimal: true }
    }
}

fn route(
    trigger: Trigger<RouteEvent>,
    config: Res<RoutingConfig>,
    mut circuits: Query<(&mut Graph, &Children), With<Circuit>>,
    symbols: Query<(&AbsoluteBoundingBox, &Children), With<Symbol>>,
    ports: Query<&GlobalTransform, With<Port>>,
    mut nets: Query<(&Net, &mut Vertices)>,
    endpoints: Query<(&Endpoint, &GlobalTransform)>,
    waypoints: Query<&GlobalTransform, With<Waypoint>>,
) {
    if let Ok((mut graph, children)) = circuits.get_mut(trigger.entity()) {
        graph.0.build(children, &symbols, &ports, config.minimal);

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
        .insert(Graph::default())
        .observe(route);
}

fn inject_vertices(trigger: Trigger<OnAdd, Net>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert(Vertices::default());
}

fn route_on_load(mut commands: Commands, mut loaded_events: EventReader<LoadedEvent>) {
    for loaded_event in loaded_events.read() {
        commands.trigger_targets(RouteEvent, loaded_event.circuit.0);
    }
}

#[derive(Default)]
pub struct RoutingPlugin;

impl bevy_app::Plugin for RoutingPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Vertices>();
        app.register_type::<RoutingConfig>();
        app.init_resource::<RoutingConfig>();
        app.observe(inject_graph);
        app.observe(inject_vertices);
        app.add_systems(bevy_app::Update, route_on_load);
    }
}
