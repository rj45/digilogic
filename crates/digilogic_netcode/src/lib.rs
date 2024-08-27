use renet::transport::*;
use renet::*;
use serde::{Deserialize, Serialize};
use std::net::{SocketAddr, UdpSocket};
use std::num::{NonZeroU64, NonZeroU8};
use std::time::{Duration, SystemTime};

pub type HashMap<K, V> = ahash::AHashMap<K, V>;

pub const PROTOCOL_MAJOR_VERSION: u32 = 1;
pub const PROTOCOL_MINOR_VERSION: u32 = 0;
const PROTOCOL_VERSION: u64 =
    ((PROTOCOL_MAJOR_VERSION as u64) << 32) | (PROTOCOL_MINOR_VERSION as u64);

// Generated on random.org and checked to be unassigned on
// https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml
pub const DEFAULT_PORT: u16 = 14123;

const COMMAND_CHANNEL_ID: u8 = 0;
const COMMAND_CHANNEL: ChannelConfig = ChannelConfig {
    channel_id: COMMAND_CHANNEL_ID,
    max_memory_usage_bytes: 64 * 1024 * 1024,
    send_type: SendType::ReliableOrdered {
        resend_time: Duration::from_millis(200),
    },
};

const DATA_CHANNEL_ID: u8 = 1;
const DATA_CHANNEL: ChannelConfig = ChannelConfig {
    channel_id: DATA_CHANNEL_ID,
    max_memory_usage_bytes: 1024 * 1024 * 1024,
    send_type: SendType::Unreliable,
};

fn common_config() -> ConnectionConfig {
    ConnectionConfig {
        available_bytes_per_tick: 1024 * 1024 * 1024,
        server_channels_config: vec![COMMAND_CHANNEL, DATA_CHANNEL],
        client_channels_config: vec![COMMAND_CHANNEL],
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct NetId(u32);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct CellId(u32);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
enum ServerMessageId {
    Status,
    Response(NonZeroU64),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ServerError {
    Unsupported,
    InvalidState,
}

#[derive(Debug, Serialize, Deserialize)]
enum ServerMessageKind {
    Error(ServerError),
    NetAdded { id: NetId },
    CellAdded { id: CellId },
}

#[derive(Debug, Serialize, Deserialize)]
struct ServerMessage {
    id: ServerMessageId,
    kind: ServerMessageKind,
}

#[derive(Debug, Serialize, Deserialize)]
enum ClientMessageKind {
    BeginBuild,
    EndBuild,

    AddNet {
        width: NonZeroU8,
    },
    AddAndGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddOrGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddXorGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddNandGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddNorGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddXnorGate {
        width: NonZeroU8,
        inputs: Vec<NetId>,
        output: NetId,
    },
    AddNotGate {
        width: NonZeroU8,
        input: NetId,
        output: NetId,
    },

    QuerySimState,
}

#[derive(Debug, Serialize, Deserialize)]
struct ClientMessage {
    id: NonZeroU64,
    kind: ClientMessageKind,
}

// Invariant for this struct: bit planes need to contain at least one unused bit at any time
#[derive(Serialize, Deserialize)]
#[cfg_attr(feature = "client", derive(bevy_ecs::prelude::Component))]
#[allow(missing_debug_implementations)]
pub struct SimState {
    order: u64, // To detect if message order gets messed up
    bit_len: u64,
    #[serde(with = "serde_bytes")]
    bit_plane_0: Vec<u8>,
    #[serde(with = "serde_bytes")]
    bit_plane_1: Vec<u8>,
}

impl Default for SimState {
    fn default() -> Self {
        Self {
            order: 0,
            bit_len: 0,
            bit_plane_0: vec![0],
            bit_plane_1: vec![0],
        }
    }
}

#[inline]
fn mask(bit_width: u8) -> u8 {
    if bit_width >= 8 {
        u8::MAX
    } else {
        (1 << bit_width) - 1
    }
}

#[inline]
fn unalign_byte(value: u8, bit_offset: u8) -> (u8, u8) {
    assert!(bit_offset < 8);
    let reverse_bit_offset = 8 - bit_offset;

    let low = value << bit_offset;
    let high = if reverse_bit_offset >= 8 {
        0
    } else {
        value >> reverse_bit_offset
    };

    (low, high)
}

#[inline]
fn align_byte(low: u8, high: u8, bit_offset: u8) -> u8 {
    assert!(bit_offset < 8);
    let reverse_bit_offset = 8 - bit_offset;

    let low = low >> bit_offset;
    let high = if reverse_bit_offset >= 8 {
        0
    } else {
        high << reverse_bit_offset
    };

    low | high
}

impl SimState {
    pub fn reset(&mut self) {
        self.order += 1;
        self.bit_len = 0;
        self.bit_plane_0.clear();
        self.bit_plane_0.push(0);
        self.bit_plane_1.clear();
        self.bit_plane_1.push(0);
    }

    pub fn push_net(
        &mut self,
        bit_width: NonZeroU8,
        bit_plane_0: &[u8],
        bit_plane_1: &[u8],
    ) -> u64 {
        // Make sure invariant is upheld
        debug_assert_eq!(self.bit_plane_0.len(), self.bit_plane_1.len());
        debug_assert!(((self.bit_plane_0.len() as u64) * 8) > self.bit_len);

        let bit_offset = (self.bit_len % 8) as u8;
        let byte_width = bit_width.get().div_ceil(8) as usize;
        assert!(bit_plane_0.len() >= byte_width);
        assert!(bit_plane_1.len() >= byte_width);

        let mut remaining_bit_width = bit_width.get();
        for i in 0..byte_width {
            let byte_bit_width = remaining_bit_width.min(8);
            remaining_bit_width -= byte_bit_width;

            let mask = mask(byte_bit_width);
            let (low_0, high_0) = unalign_byte(bit_plane_0[i] & mask, bit_offset);
            let (low_1, high_1) = unalign_byte(bit_plane_1[i] & mask, bit_offset);
            *self.bit_plane_0.last_mut().unwrap() |= low_0;
            *self.bit_plane_1.last_mut().unwrap() |= low_1;
            if (bit_offset + byte_bit_width) >= 8 {
                self.bit_plane_0.push(high_0);
                self.bit_plane_1.push(high_1);
            }
        }

        let net_offset = self.bit_len;
        self.bit_len += bit_width.get() as u64;
        net_offset
    }

    pub fn get_net(
        &self,
        offset: u64,
        bit_width: NonZeroU8,
        bit_plane_0: &mut [u8],
        bit_plane_1: &mut [u8],
    ) {
        // Make sure invariant is upheld
        debug_assert_eq!(self.bit_plane_0.len(), self.bit_plane_1.len());
        debug_assert!(((self.bit_plane_0.len() as u64) * 8) > self.bit_len);

        assert!((offset + (bit_width.get() as u64)) <= self.bit_len);

        let byte_offset = (offset / 8) as usize;
        let bit_offset = (offset % 8) as u8;
        let byte_width = bit_width.get().div_ceil(8) as usize;
        assert!(bit_plane_0.len() >= byte_width);
        assert!(bit_plane_1.len() >= byte_width);

        let mut remaining_bit_width = bit_width.get();
        for i in 0..byte_width {
            let byte_bit_width = remaining_bit_width.min(8);
            remaining_bit_width -= byte_bit_width;

            let low_0 = self.bit_plane_0[byte_offset + i];
            let low_1 = self.bit_plane_1[byte_offset + i];
            let (high_0, high_1) = if (bit_offset + byte_bit_width) >= 8 {
                (
                    self.bit_plane_0[byte_offset + i + 1],
                    self.bit_plane_1[byte_offset + i + 1],
                )
            } else {
                (0, 0)
            };

            let mask = mask(byte_bit_width);
            bit_plane_0[i] = align_byte(low_0, high_0, bit_offset) & mask;
            bit_plane_1[i] = align_byte(low_1, high_1, bit_offset) & mask;
        }
    }
}

#[cfg(feature = "client")]
mod client;
#[cfg(feature = "client")]
pub use client::*;

#[cfg(feature = "server")]
mod server;
#[cfg(feature = "server")]
pub use server::*;

#[cfg(test)]
mod tests {
    use super::*;

    macro_rules! nz {
        ($v:literal) => {
            match NonZeroU8::new($v) {
                Some(v) => v,
                None => panic!("passed zero to a non-zero value"),
            }
        };
    }

    #[test]
    fn insert_1_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(1), &[0b1], &[0b1]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.bit_plane_0, [0b1]);
        assert_eq!(sim_state.bit_plane_1, [0b1]);
    }

    #[test]
    fn read_1_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 1,
            bit_plane_0: vec![0b1],
            bit_plane_1: vec![0b1],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(1), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0b1]);
        assert_eq!(bit_plane_1, [0b1]);
    }

    #[test]
    fn insert_8_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(8), &[0xAA], &[0x55]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0]);
    }

    #[test]
    fn read_8_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 8,
            bit_plane_0: vec![0xAA, 0],
            bit_plane_1: vec![0x55, 0],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(8), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA]);
        assert_eq!(bit_plane_1, [0x55]);
    }

    #[test]
    fn insert_16_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(16), &[0xAA, 0x55], &[0x55, 0xAA]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0x55, 0]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0xAA, 0]);
    }

    #[test]
    fn read_16_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 16,
            bit_plane_0: vec![0xAA, 0x55, 0],
            bit_plane_1: vec![0x55, 0xAA, 0],
        };
        let mut bit_plane_0 = [0u8; 2];
        let mut bit_plane_1 = [0u8; 2];
        sim_state.get_net(0, nz!(16), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA, 0x55]);
        assert_eq!(bit_plane_1, [0x55, 0xAA]);
    }

    #[test]
    fn insert_7_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(7), &[0x2A], &[0x55]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.bit_plane_0, [0x2A]);
        assert_eq!(sim_state.bit_plane_1, [0x55]);
    }

    #[test]
    fn read_7_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 7,
            bit_plane_0: vec![0x2A],
            bit_plane_1: vec![0x55],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(7), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x2A]);
        assert_eq!(bit_plane_1, [0x55]);
    }

    #[test]
    fn insert_9_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(9), &[0xAA, 0b0], &[0x55, 0b1]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0b0]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0b1]);
    }

    #[test]
    fn read_9_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 9,
            bit_plane_0: vec![0xAA, 0b0],
            bit_plane_1: vec![0x55, 0b1],
        };
        let mut bit_plane_0 = [0u8; 2];
        let mut bit_plane_1 = [0u8; 2];
        sim_state.get_net(0, nz!(9), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA, 0b0]);
        assert_eq!(bit_plane_1, [0x55, 0b1]);
    }

    #[test]
    fn insert_multiple_1_bit_nets() {
        let mut sim_state = SimState::default();
        let net_index_a = sim_state.push_net(nz!(1), &[0b1], &[0b0]);
        let net_index_b = sim_state.push_net(nz!(1), &[0b0], &[0b1]);
        let net_index_c = sim_state.push_net(nz!(1), &[0b1], &[0b0]);
        assert_eq!(net_index_a, 0);
        assert_eq!(net_index_b, 1);
        assert_eq!(net_index_c, 2);
        assert_eq!(sim_state.bit_plane_0, [0b101]);
        assert_eq!(sim_state.bit_plane_1, [0b010]);
    }

    #[test]
    fn read_multiple_1_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 3,
            bit_plane_0: vec![0b101],
            bit_plane_1: vec![0b010],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(1), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0b1]);
        assert_eq!(bit_plane_1, [0b0]);
        sim_state.get_net(1, nz!(1), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0b0]);
        assert_eq!(bit_plane_1, [0b1]);
        sim_state.get_net(2, nz!(1), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0b1]);
        assert_eq!(bit_plane_1, [0b0]);
    }

    #[test]
    fn insert_multiple_8_bit_nets() {
        let mut sim_state = SimState::default();
        let net_index_a = sim_state.push_net(nz!(8), &[0xAA], &[0x55]);
        let net_index_b = sim_state.push_net(nz!(8), &[0x55], &[0xAA]);
        let net_index_c = sim_state.push_net(nz!(8), &[0xAA], &[0x55]);
        assert_eq!(net_index_a, 0);
        assert_eq!(net_index_b, 8);
        assert_eq!(net_index_c, 16);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0x55, 0xAA, 0]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0xAA, 0x55, 0]);
    }

    #[test]
    fn read_multiple_8_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 24,
            bit_plane_0: vec![0xAA, 0x55, 0xAA, 0],
            bit_plane_1: vec![0x55, 0xAA, 0x55, 0],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(8), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA]);
        assert_eq!(bit_plane_1, [0x55]);
        sim_state.get_net(8, nz!(8), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x55]);
        assert_eq!(bit_plane_1, [0xAA]);
        sim_state.get_net(16, nz!(8), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA]);
        assert_eq!(bit_plane_1, [0x55]);
    }

    #[test]
    fn insert_multiple_7_bit_nets() {
        let mut sim_state = SimState::default();
        let net_index_a = sim_state.push_net(nz!(7), &[0x2A], &[0x55]);
        let net_index_b = sim_state.push_net(nz!(7), &[0x55], &[0x2A]);
        let net_index_c = sim_state.push_net(nz!(7), &[0x2A], &[0x55]);
        assert_eq!(net_index_a, 0);
        assert_eq!(net_index_b, 7);
        assert_eq!(net_index_c, 14);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0xAA, 0x0A]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0x55, 0x15]);
    }

    #[test]
    fn read_multiple_7_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 21,
            bit_plane_0: vec![0xAA, 0xAA, 0x0A],
            bit_plane_1: vec![0x55, 0x55, 0x15],
        };
        let mut bit_plane_0 = [0u8; 1];
        let mut bit_plane_1 = [0u8; 1];
        sim_state.get_net(0, nz!(7), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x2A]);
        assert_eq!(bit_plane_1, [0x55]);
        sim_state.get_net(7, nz!(7), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x55]);
        assert_eq!(bit_plane_1, [0x2A]);
        sim_state.get_net(14, nz!(7), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x2A]);
        assert_eq!(bit_plane_1, [0x55]);
    }

    #[test]
    fn insert_multiple_9_bit_nets() {
        let mut sim_state = SimState::default();
        let net_index_a = sim_state.push_net(nz!(9), &[0xAA, 0b0], &[0x55, 0b1]);
        let net_index_b = sim_state.push_net(nz!(9), &[0x55, 0b1], &[0xAA, 0b0]);
        let net_index_c = sim_state.push_net(nz!(9), &[0xAA, 0b0], &[0x55, 0b1]);
        assert_eq!(net_index_a, 0);
        assert_eq!(net_index_b, 9);
        assert_eq!(net_index_c, 18);
        assert_eq!(sim_state.bit_plane_0, [0xAA, 0xAA, 0xAA, 0x02]);
        assert_eq!(sim_state.bit_plane_1, [0x55, 0x55, 0x55, 0x05]);
    }

    #[test]
    fn read_multiple_9_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 27,
            bit_plane_0: vec![0xAA, 0xAA, 0xAA, 0x02],
            bit_plane_1: vec![0x55, 0x55, 0x55, 0x05],
        };
        let mut bit_plane_0 = [0u8; 2];
        let mut bit_plane_1 = [0u8; 2];
        sim_state.get_net(0, nz!(9), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA, 0b0]);
        assert_eq!(bit_plane_1, [0x55, 0b1]);
        sim_state.get_net(9, nz!(9), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0x55, 0b1]);
        assert_eq!(bit_plane_1, [0xAA, 0b0]);
        sim_state.get_net(18, nz!(9), &mut bit_plane_0, &mut bit_plane_1);
        assert_eq!(bit_plane_0, [0xAA, 0b0]);
        assert_eq!(bit_plane_1, [0x55, 0b1]);
    }
}
