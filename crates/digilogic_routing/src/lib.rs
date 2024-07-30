mod graph;
pub use graph::*;

mod segment_tree;

use bevy_ecs::prelude::*;
use digilogic_core::components::*;
use digilogic_core::transform::*;

type HashSet<T> = ahash::AHashSet<T>;
type HashMap<K, V> = ahash::AHashMap<K, V>;

#[derive(Default, Component)]
#[repr(transparent)]
pub struct Graph(GraphData);

#[derive(Default, Component)]
#[repr(transparent)]
pub struct Vertices(pub Vec<[f32; 2]>);

#[derive(Default, Event)]
pub struct Route;

#[derive(Resource)]
pub struct RoutingConfig {
    pub minimal: bool,
}

impl Default for RoutingConfig {
    fn default() -> Self {
        Self { minimal: true }
    }
}

fn route(
    trigger: Trigger<Route>,
    config: Res<RoutingConfig>,
    mut circuits: Query<(&Circuit, &mut Graph)>,
    symbols: Query<(&Symbol, &AbsoluteBoundingBox)>,
    ports: Query<&GlobalTransform, With<Port>>,
    mut nets: Query<(&Net, &mut Vertices)>,
    endpoints: Query<(&Endpoint, &GlobalTransform)>,
    waypoints: Query<&GlobalTransform, With<Waypoint>>,
) {
    if let Ok((circuit, mut graph)) = circuits.get_mut(trigger.entity()) {
        graph.0.build(circuit, &symbols, &ports, config.minimal);

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

#[derive(Default)]
pub struct RoutingPlugin;

impl bevy_app::Plugin for RoutingPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.init_resource::<RoutingConfig>();
        app.observe(inject_graph);
        app.observe(inject_vertices);
    }
}
