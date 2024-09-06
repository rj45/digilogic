use crate::*;
use aery::prelude::*;
use bevy_app::prelude::*;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::Read;
use bevy_ecs::system::SystemParam;
use bevy_reflect::prelude::*;
use bevy_state::prelude::*;
use bevy_time::prelude::*;
use digilogic_core::components::*;
use digilogic_core::resources::Project;
use digilogic_core::states::SimulationState;
use digilogic_core::{SharedStr, StateMut};
use std::net::ToSocketAddrs;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Reflect, Component)]
#[repr(transparent)]
pub struct StateOffset(pub u64);

trait RenetClientExt {
    fn send_command_message(&mut self, message: ClientMessage);
    fn receive_command_message(&mut self) -> Option<ServerMessage>;
    fn receive_sim_state(&mut self) -> Option<SimState>;
}

impl RenetClientExt for RenetClient {
    fn send_command_message(&mut self, message: ClientMessage) {
        let message = rmp_serde::to_vec(&message).unwrap();
        self.send_message(COMMAND_CHANNEL_ID, message);
    }

    fn receive_command_message(&mut self) -> Option<ServerMessage> {
        let message = self.receive_message(COMMAND_CHANNEL_ID)?;
        let message: ServerMessage =
            rmp_serde::from_slice(&message).expect("invalid server message");
        Some(message)
    }

    fn receive_sim_state(&mut self) -> Option<SimState> {
        let message = self.receive_message(DATA_CHANNEL_ID)?;
        let sim_state: SimState = rmp_serde::from_slice(&message).expect("invalid server message");
        Some(sim_state)
    }
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Reflect, Resource)]
#[repr(transparent)]
struct NextMessageId(u64);

impl NextMessageId {
    fn get(&mut self) -> u64 {
        let id = self.0;
        self.0 = self.0.wrapping_add(1);
        id
    }
}

#[derive(Debug, Clone, Reflect, Event)]
pub struct Connect {
    pub server_addr: (SharedStr, u16),
}

#[derive(Debug, Clone, Reflect, Event)]
pub struct Disconnect;

fn connect(
    trigger: Trigger<Connect>,
    mut commands: Commands,
    mut transport_errors: EventWriter<NetcodeTransportError>,
    mut next_state: ResMut<NextState<SimulationState>>,
) {
    let server_addr = (
        trigger.event().server_addr.0.as_str(),
        trigger.event().server_addr.1,
    );

    let server_addr = match server_addr.to_socket_addrs() {
        Ok(addrs) => {
            if let Some(addr) = addrs.into_iter().next() {
                addr
            } else {
                transport_errors.send(NetcodeTransportError::IO(
                    std::io::ErrorKind::AddrNotAvailable.into(),
                ));
                return;
            }
        }
        Err(err) => {
            transport_errors.send(NetcodeTransportError::IO(err));
            return;
        }
    };

    let current_time = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap();

    // TODO: use secure authentication
    let authentication = ClientAuthentication::Unsecure {
        protocol_id: PROTOCOL_VERSION,
        client_id: current_time.as_millis() as u64,
        server_addr,
        user_data: None,
    };

    let socket = match UdpSocket::bind("127.0.0.1:0") {
        Ok(socket) => socket,
        Err(err) => {
            transport_errors.send(NetcodeTransportError::IO(err));
            return;
        }
    };

    let transport = match NetcodeClientTransport::new(current_time, authentication, socket) {
        Ok(transport) => transport,
        Err(err) => {
            transport_errors.send(NetcodeTransportError::Netcode(err));
            return;
        }
    };

    commands.insert_resource(RenetClient::new(common_config()));
    commands.insert_resource(transport);
    next_state.set(SimulationState::WaitingOnServer);
}

fn disconnect(
    _trigger: Trigger<Disconnect>,
    mut commands: Commands,
    transport: Option<ResMut<NetcodeClientTransport>>,
    mut next_state: ResMut<NextState<SimulationState>>,
) {
    if let Some(mut transport) = transport {
        transport.disconnect();
    }

    commands.remove_resource::<RenetClient>();
    commands.remove_resource::<NetcodeClientTransport>();
    commands.remove_resource::<SimState>();
    next_state.set(SimulationState::Disconnected);
}

fn update(
    time: Res<Time<Real>>,
    mut client: ResMut<RenetClient>,
    mut transport: ResMut<NetcodeClientTransport>,
    mut transport_errors: EventWriter<NetcodeTransportError>,
) {
    client.update(time.delta());
    if let Err(err) = transport.update(time.delta(), &mut client) {
        transport_errors.send(err);
    }
}

fn send_packets(
    mut client: ResMut<RenetClient>,
    mut transport: ResMut<NetcodeClientTransport>,
    mut transport_errors: EventWriter<NetcodeTransportError>,
) {
    if let Err(err) = transport.send_packets(&mut client) {
        transport_errors.send(err);
    }
}

fn disconnect_on_exit(
    mut commands: Commands,
    exit_events: EventReader<AppExit>,
    mut transport: ResMut<NetcodeClientTransport>,
) {
    if !exit_events.is_empty() {
        transport.disconnect();
        commands.remove_resource::<RenetClient>();
        commands.remove_resource::<NetcodeClientTransport>();
        commands.remove_resource::<SimState>();
    }
}

fn process_messages(
    mut commands: Commands,
    mut client: ResMut<RenetClient>,
    mut state: StateMut<SimulationState>,
    mut next_message_id: ResMut<NextMessageId>,
    current_sim_state: Option<Res<SimState>>,
) {
    let mut actual_state = *state;

    while let Some(message) = client.receive_command_message() {
        match message {
            ServerMessage::Error { .. } => todo!(),
            ServerMessage::Ready => {
                assert_eq!(actual_state, SimulationState::WaitingOnServer);
                actual_state = SimulationState::Building;
            }
            ServerMessage::BuildingFinished => {
                assert_eq!(actual_state, SimulationState::Building);
                actual_state = SimulationState::ActiveIdle;

                client.send_command_message(ClientMessage {
                    id: next_message_id.get(),
                    kind: ClientMessageKind::QuerySimState,
                });
            }
        }
    }

    let mut order = current_sim_state.map(|state| state.order).unwrap_or(0);
    let mut new_sim_state = None;
    while let Some(sim_state) = client.receive_sim_state() {
        if sim_state.order >= order {
            order = sim_state.order;
            new_sim_state = Some(sim_state);
        }
    }
    if let Some(new_sim_state) = new_sim_state {
        commands.insert_resource(new_sim_state);
    }

    if actual_state != *state {
        state.queue_next(actual_state);
    }
}

type CircuitQuery<'w, 's> = Query<'w, 's, ((), Relations<Child>), With<Circuit>>;
type SymbolQuery<'w, 's> = Query<'w, 's, (Entity, Relations<Child>), With<Symbol>>;
type PortQuery<'w, 's> = Query<'w, 's, Entity, With<Port>>;
type NetQuery<'w, 's> = Query<'w, 's, (Entity, Relations<Child>), With<Net>>;
type EndpointQuery<'w, 's> = Query<'w, 's, (Entity, Read<PortID>), With<Endpoint>>;

#[derive(SystemParam)]
struct BuildQueries<'w, 's> {
    circuits: CircuitQuery<'w, 's>,
    symbols: SymbolQuery<'w, 's>,
    ports: PortQuery<'w, 's>,
    nets: NetQuery<'w, 's>,
    endpoints: EndpointQuery<'w, 's>,
}

fn build(
    mut commands: Commands,
    mut client: ResMut<RenetClient>,
    project: Res<Project>,
    mut next_message_id: ResMut<NextMessageId>,
    queries: BuildQueries,
) {
    let root_circuit = project
        .root_circuit
        .expect("simulation started with no root");
    let (_, root_children) = queries
        .circuits
        .get(root_circuit.0)
        .expect("invalid root circuit");

    client.send_command_message(ClientMessage {
        id: next_message_id.get(),
        kind: ClientMessageKind::BeginBuild,
    });

    let mut net_id = NetId(0);
    let mut offset = 0u64;
    root_children
        .join::<Child>(&queries.nets)
        .for_each(|(net, _)| {
            client.send_command_message(ClientMessage {
                id: next_message_id.get(),
                kind: ClientMessageKind::AddNet {
                    width: NonZeroU8::MIN, // TODO: use actual net width
                },
            });

            commands.entity(net).insert(StateOffset(offset));

            net_id.0 += 1;
            offset += 1; // TODO: use actual net width
        });

    client.send_command_message(ClientMessage {
        id: next_message_id.get(),
        kind: ClientMessageKind::EndBuild,
    });
}

#[derive(Default, Debug)]
pub struct ClientPlugin;

impl Plugin for ClientPlugin {
    fn build(&self, app: &mut App) {
        app.register_type::<SimState>()
            .register_type::<StateOffset>()
            .register_type::<NextMessageId>()
            .register_type::<Connect>()
            .register_type::<Disconnect>();

        app.init_resource::<NextMessageId>()
            .add_event::<NetcodeTransportError>()
            .observe(connect)
            .observe(disconnect);

        app.add_systems(
            PreUpdate,
            update
                .run_if(resource_exists::<RenetClient>)
                .run_if(resource_exists::<NetcodeClientTransport>),
        );

        app.add_systems(
            Update,
            process_messages
                .run_if(resource_exists::<RenetClient>)
                .run_if(resource_exists::<NetcodeClientTransport>),
        );

        app.add_systems(
            PostUpdate,
            (send_packets, disconnect_on_exit)
                .run_if(resource_exists::<RenetClient>)
                .run_if(resource_exists::<NetcodeClientTransport>),
        );

        app.add_systems(OnEnter(SimulationState::Building), build);
    }
}
