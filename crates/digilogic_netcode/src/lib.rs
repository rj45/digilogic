use renet::transport::*;
use renet::*;
use serde::{Deserialize, Serialize};
use std::net::{SocketAddr, UdpSocket};
use std::num::NonZeroU8;
use std::time::{Duration, SystemTime};

pub type HashMap<K, V> = ahash::AHashMap<K, V>;

pub const PROTOCOL_MAJOR_VERSION: u32 = 1;
pub const PROTOCOL_MINOR_VERSION: u32 = 0;
const PROTOCOL_VERSION: u64 =
    ((PROTOCOL_MAJOR_VERSION as u64) << 32) | (PROTOCOL_MINOR_VERSION as u64);

// Generated on random.org and checked to be unassigned on
// https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml
pub const DEFAULT_PORT: u16 = 14123;

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

// Invariant for this struct: `state_bits` and `valid_bits` vectors need to contain at least one unused bit at any time
#[derive(Serialize, Deserialize)]
#[cfg_attr(feature = "client", derive(bevy_ecs::prelude::Component))]
#[allow(missing_debug_implementations)]
pub struct SimState {
    order: u64, // To detect if message order gets messed up
    bit_len: u64,
    #[serde(with = "serde_bytes")]
    state_bits: Vec<u8>,
    #[serde(with = "serde_bytes")]
    valid_bits: Vec<u8>,
}

impl Default for SimState {
    fn default() -> Self {
        Self {
            order: 0,
            bit_len: 0,
            state_bits: vec![0],
            valid_bits: vec![0],
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
        self.state_bits.clear();
        self.state_bits.push(0);
        self.valid_bits.clear();
        self.valid_bits.push(0);
    }

    pub fn push_net(&mut self, bit_width: NonZeroU8, state_bits: &[u8], valid_bits: &[u8]) -> u64 {
        // Make sure invariant is upheld
        debug_assert_eq!(self.state_bits.len(), self.valid_bits.len());
        debug_assert!(((self.state_bits.len() as u64) * 8) > self.bit_len);

        let bit_offset = (self.bit_len % 8) as u8;
        let byte_width = bit_width.get().div_ceil(8) as usize;
        assert!(state_bits.len() >= byte_width);
        assert!(valid_bits.len() >= byte_width);

        let mut remaining_bit_width = bit_width.get();
        for i in 0..byte_width {
            let byte_bit_width = remaining_bit_width.min(8);
            remaining_bit_width -= byte_bit_width;

            let mask = mask(byte_bit_width);
            let (state_low, state_high) = unalign_byte(state_bits[i] & mask, bit_offset);
            let (valid_low, valid_high) = unalign_byte(valid_bits[i] & mask, bit_offset);
            *self.state_bits.last_mut().unwrap() |= state_low;
            *self.valid_bits.last_mut().unwrap() |= valid_low;
            if (bit_offset + byte_bit_width) >= 8 {
                self.state_bits.push(state_high);
                self.valid_bits.push(valid_high);
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
        state_bits: &mut [u8],
        valid_bits: &mut [u8],
    ) {
        // Make sure invariant is upheld
        debug_assert_eq!(self.state_bits.len(), self.valid_bits.len());
        debug_assert!(((self.state_bits.len() as u64) * 8) > self.bit_len);

        assert!((offset + (bit_width.get() as u64)) <= self.bit_len);

        let byte_offset = (offset / 8) as usize;
        let bit_offset = (offset % 8) as u8;
        let byte_width = bit_width.get().div_ceil(8) as usize;
        assert!(state_bits.len() >= byte_width);
        assert!(valid_bits.len() >= byte_width);

        let mut remaining_bit_width = bit_width.get();
        for i in 0..byte_width {
            let byte_bit_width = remaining_bit_width.min(8);
            remaining_bit_width -= byte_bit_width;

            let state_low = self.state_bits[byte_offset + i];
            let valid_low = self.valid_bits[byte_offset + i];
            let (state_high, valid_high) = if (bit_offset + byte_bit_width) >= 8 {
                (
                    self.state_bits[byte_offset + i + 1],
                    self.valid_bits[byte_offset + i + 1],
                )
            } else {
                (0, 0)
            };

            let mask = mask(byte_bit_width);
            state_bits[i] = align_byte(state_low, state_high, bit_offset) & mask;
            valid_bits[i] = align_byte(valid_low, valid_high, bit_offset) & mask;
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
        ($v:expr) => {
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
        assert_eq!(sim_state.state_bits, [0b1]);
        assert_eq!(sim_state.valid_bits, [0b1]);
    }

    #[test]
    fn read_1_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 1,
            state_bits: vec![0b1],
            valid_bits: vec![0b1],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(1), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0b1]);
        assert_eq!(valid_bits, [0b1]);
    }

    #[test]
    fn insert_8_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(8), &[0xAA], &[0x55]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.state_bits, [0xAA, 0]);
        assert_eq!(sim_state.valid_bits, [0x55, 0]);
    }

    #[test]
    fn read_8_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 8,
            state_bits: vec![0xAA, 0],
            valid_bits: vec![0x55, 0],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(8), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA]);
        assert_eq!(valid_bits, [0x55]);
    }

    #[test]
    fn insert_16_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(16), &[0xAA, 0x55], &[0x55, 0xAA]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.state_bits, [0xAA, 0x55, 0]);
        assert_eq!(sim_state.valid_bits, [0x55, 0xAA, 0]);
    }

    #[test]
    fn read_16_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 16,
            state_bits: vec![0xAA, 0x55, 0],
            valid_bits: vec![0x55, 0xAA, 0],
        };
        let mut state_bits = [0u8; 2];
        let mut valid_bits = [0u8; 2];
        sim_state.get_net(0, nz!(16), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA, 0x55]);
        assert_eq!(valid_bits, [0x55, 0xAA]);
    }

    #[test]
    fn insert_7_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(7), &[0x2A], &[0x55]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.state_bits, [0x2A]);
        assert_eq!(sim_state.valid_bits, [0x55]);
    }

    #[test]
    fn read_7_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 7,
            state_bits: vec![0x2A],
            valid_bits: vec![0x55],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(7), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x2A]);
        assert_eq!(valid_bits, [0x55]);
    }

    #[test]
    fn insert_9_bit_net() {
        let mut sim_state = SimState::default();
        let net_index = sim_state.push_net(nz!(9), &[0xAA, 0b0], &[0x55, 0b1]);
        assert_eq!(net_index, 0);
        assert_eq!(sim_state.state_bits, [0xAA, 0b0]);
        assert_eq!(sim_state.valid_bits, [0x55, 0b1]);
    }

    #[test]
    fn read_9_bit_net() {
        let sim_state = SimState {
            order: 0,
            bit_len: 9,
            state_bits: vec![0xAA, 0b0],
            valid_bits: vec![0x55, 0b1],
        };
        let mut state_bits = [0u8; 2];
        let mut valid_bits = [0u8; 2];
        sim_state.get_net(0, nz!(9), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA, 0b0]);
        assert_eq!(valid_bits, [0x55, 0b1]);
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
        assert_eq!(sim_state.state_bits, [0b101]);
        assert_eq!(sim_state.valid_bits, [0b010]);
    }

    #[test]
    fn read_multiple_1_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 3,
            state_bits: vec![0b101],
            valid_bits: vec![0b010],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(1), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0b1]);
        assert_eq!(valid_bits, [0b0]);
        sim_state.get_net(1, nz!(1), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0b0]);
        assert_eq!(valid_bits, [0b1]);
        sim_state.get_net(2, nz!(1), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0b1]);
        assert_eq!(valid_bits, [0b0]);
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
        assert_eq!(sim_state.state_bits, [0xAA, 0x55, 0xAA, 0]);
        assert_eq!(sim_state.valid_bits, [0x55, 0xAA, 0x55, 0]);
    }

    #[test]
    fn read_multiple_8_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 24,
            state_bits: vec![0xAA, 0x55, 0xAA, 0],
            valid_bits: vec![0x55, 0xAA, 0x55, 0],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(8), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA]);
        assert_eq!(valid_bits, [0x55]);
        sim_state.get_net(8, nz!(8), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x55]);
        assert_eq!(valid_bits, [0xAA]);
        sim_state.get_net(16, nz!(8), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA]);
        assert_eq!(valid_bits, [0x55]);
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
        assert_eq!(sim_state.state_bits, [0xAA, 0xAA, 0x0A]);
        assert_eq!(sim_state.valid_bits, [0x55, 0x55, 0x15]);
    }

    #[test]
    fn read_multiple_7_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 21,
            state_bits: vec![0xAA, 0xAA, 0x0A],
            valid_bits: vec![0x55, 0x55, 0x15],
        };
        let mut state_bits = [0u8; 1];
        let mut valid_bits = [0u8; 1];
        sim_state.get_net(0, nz!(7), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x2A]);
        assert_eq!(valid_bits, [0x55]);
        sim_state.get_net(7, nz!(7), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x55]);
        assert_eq!(valid_bits, [0x2A]);
        sim_state.get_net(14, nz!(7), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x2A]);
        assert_eq!(valid_bits, [0x55]);
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
        assert_eq!(sim_state.state_bits, [0xAA, 0xAA, 0xAA, 0x02]);
        assert_eq!(sim_state.valid_bits, [0x55, 0x55, 0x55, 0x05]);
    }

    #[test]
    fn read_multiple_9_bit_nets() {
        let sim_state = SimState {
            order: 0,
            bit_len: 27,
            state_bits: vec![0xAA, 0xAA, 0xAA, 0x02],
            valid_bits: vec![0x55, 0x55, 0x55, 0x05],
        };
        let mut state_bits = [0u8; 2];
        let mut valid_bits = [0u8; 2];
        sim_state.get_net(0, nz!(9), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA, 0b0]);
        assert_eq!(valid_bits, [0x55, 0b1]);
        sim_state.get_net(9, nz!(9), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0x55, 0b1]);
        assert_eq!(valid_bits, [0xAA, 0b0]);
        sim_state.get_net(18, nz!(9), &mut state_bits, &mut valid_bits);
        assert_eq!(state_bits, [0xAA, 0b0]);
        assert_eq!(valid_bits, [0x55, 0b1]);
    }
}
