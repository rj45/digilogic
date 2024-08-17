use renet::transport::*;
use renet::*;
use serde::{Deserialize, Serialize};
use std::net::{SocketAddr, UdpSocket};
use std::time::{Duration, SystemTime};

pub const PROTOCOL_MAJOR_VERSION: u32 = 1;
pub const PROTOCOL_MINOR_VERSION: u32 = 0;
const PROTOCOL_VERSION: u64 =
    ((PROTOCOL_MAJOR_VERSION as u64) << 32) | (PROTOCOL_MINOR_VERSION as u64);

pub const TARGET_TICK_RATE: usize = 30; // ticks/second
const TARGET_DATA_RATE: usize = 10_000_000; // bits/second
const BYTES_PER_TICK: usize = (TARGET_DATA_RATE / TARGET_TICK_RATE / 8).next_power_of_two();

const COMMAND_CHANNEL_ID: u8 = 0;
const COMMAND_CHANNEL: ChannelConfig = ChannelConfig {
    channel_id: COMMAND_CHANNEL_ID,
    max_memory_usage_bytes: BYTES_PER_TICK / 8,
    send_type: SendType::ReliableOrdered {
        resend_time: Duration::from_millis(200),
    },
};

const DATA_CHANNEL_ID: u8 = 1;
const DATA_CHANNEL: ChannelConfig = ChannelConfig {
    channel_id: DATA_CHANNEL_ID,
    max_memory_usage_bytes: BYTES_PER_TICK,
    send_type: SendType::Unreliable,
};

fn common_config() -> ConnectionConfig {
    ConnectionConfig {
        available_bytes_per_tick: BYTES_PER_TICK as u64,
        server_channels_config: vec![COMMAND_CHANNEL, DATA_CHANNEL],
        client_channels_config: vec![COMMAND_CHANNEL],
    }
}

#[derive(Serialize, Deserialize)]
enum ServerCommandMessage {
    // TODO
    Placeholder,
}

#[derive(Serialize, Deserialize)]
enum ClientCommandMessage {
    // TODO
    Placeholder,
}

#[derive(Serialize, Deserialize)]
enum DataMessageKind {
    // TODO
    Placeholder,
}

#[derive(Serialize, Deserialize)]
struct DataMessage {
    order: u64, // To detect if message order gets messed up
    kind: DataMessageKind,
}

#[derive(Default)]
#[cfg_attr(feature = "client", derive(bevy_ecs::prelude::Component))]
#[allow(missing_debug_implementations)]
pub struct SimState {
    // TODO: these are just placeholders
    nets: Vec<u32>,
    components: ahash::AHashMap<u32, u32>,
    #[cfg(feature = "client")]
    dirty_net_list: Vec<usize>,
}

impl SimState {
    #[cfg(feature = "client")]
    fn data_messages<'a>(
        &'a mut self,
        message_order: &'a mut u64,
    ) -> impl Iterator<Item = DataMessage> + 'a {
        self.dirty_net_list.drain(..).map(|net_index| DataMessage {
            order: *message_order,
            kind: DataMessageKind::Placeholder,
        })
    }
}

#[cfg(feature = "client")]
mod client;
#[cfg(feature = "client")]
pub use client::*;

#[cfg(feature = "server")]
mod server;
#[cfg(feature = "client")]
pub use server::*;
