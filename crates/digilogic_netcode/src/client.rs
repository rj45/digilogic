use crate::*;
use bevy_app::prelude::*;
use bevy_ecs::prelude::*;
use bevy_state::prelude::*;
use bevy_time::prelude::*;
use digilogic_core::components::Circuit;
use digilogic_core::{SharedStr, StateMut};
use std::net::ToSocketAddrs;

trait RenetClientExt {
    fn send_command_message(&mut self, message: ClientMessage);
    fn receive_command_message(&mut self) -> Option<ServerMessage>;
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
}

fn inject_sim_state(trigger: Trigger<OnAdd, Circuit>, mut commands: Commands) {
    commands
        .get_entity(trigger.entity())
        .unwrap()
        .insert(SimState::default());
}

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, States)]
enum ConnectionState {
    #[default]
    Disconnected,
    WaitingOnServer,
    ServerReady,
}

#[derive(Debug, Clone, Event)]
pub struct Connect {
    pub server_addr: (SharedStr, u16),
}

#[derive(Debug, Clone, Event)]
pub struct Disconnect;

fn connect(
    trigger: Trigger<Connect>,
    mut commands: Commands,
    mut transport_errors: EventWriter<NetcodeTransportError>,
    mut next_state: ResMut<NextState<ConnectionState>>,
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
    next_state.set(ConnectionState::WaitingOnServer);
}

fn disconnect(
    _trigger: Trigger<Disconnect>,
    mut commands: Commands,
    transport: Option<ResMut<NetcodeClientTransport>>,
    mut next_state: ResMut<NextState<ConnectionState>>,
) {
    if let Some(mut transport) = transport {
        transport.disconnect();
    }

    commands.remove_resource::<RenetClient>();
    commands.remove_resource::<NetcodeClientTransport>();
    next_state.set(ConnectionState::Disconnected);
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
    }
}

fn process_incomming(mut client: ResMut<RenetClient>, mut state: StateMut<ConnectionState>) {
    let mut actual_state = *state;

    while let Some(message) = client.receive_command_message() {
        match message.kind {
            ServerMessageKind::Error(_) => todo!(),
            ServerMessageKind::Ready => {
                assert_eq!(actual_state, ConnectionState::WaitingOnServer);
                actual_state = ConnectionState::ServerReady;
            }
            ServerMessageKind::BuildingFinished { net_map } => todo!(),
            ServerMessageKind::NetAdded { id } => todo!(),
            ServerMessageKind::CellAdded { id } => todo!(),
        }
    }

    if actual_state != *state {
        state.queue_next(actual_state);
    }
}

#[derive(Default, Debug)]
pub struct ClientPlugin;

impl Plugin for ClientPlugin {
    fn build(&self, app: &mut App) {
        app.init_state::<ConnectionState>()
            .add_event::<NetcodeTransportError>()
            .observe(inject_sim_state)
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
            process_incomming
                .run_if(resource_exists::<RenetClient>)
                .run_if(resource_exists::<NetcodeClientTransport>),
        );

        app.add_systems(
            PostUpdate,
            (send_packets, disconnect_on_exit)
                .run_if(resource_exists::<RenetClient>)
                .run_if(resource_exists::<NetcodeClientTransport>),
        );
    }
}
