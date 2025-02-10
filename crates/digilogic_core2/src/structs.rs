use std::{str::FromStr, sync::Arc};

use serde::{Deserialize, Serialize};
use slotmap::{new_key_type, SlotMap};

#[derive(Default, Debug, Clone, Copy, Deserialize, Serialize)]
pub enum WireState {
    #[default]
    #[serde(rename = "0")]
    L,
    #[serde(rename = "1")]
    H,
    #[serde(rename = "x")]
    X,
    #[serde(rename = "z")]
    Z,
}

impl FromStr for WireState {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "0" | "l" | "L" => Ok(Self::L),
            "1" | "h" | "H" => Ok(Self::H),
            "x" | "X" => Ok(Self::X),
            "z" | "Z" => Ok(Self::Z),
            _ => Err(anyhow::anyhow!("invalid wire state: {}", s)),
        }
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum Direction {
    In,
    Out,
    InOut,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Size {
    pub width: u32,
    pub height: u32,
}

new_key_type! {
    pub struct PortID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Port {
    pub symbol_kind: SymbolKindID,
    pub name: Arc<str>,
    pub direction: Direction,
    pub position: Position,
    pub pin: u32,
}

new_key_type! {
    pub struct SymbolKindID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SymbolKind {
    pub module: ModuleID,
    pub ports: Vec<PortID>,
    pub size: Size,
    pub name: Arc<str>,
    pub prefix: Arc<str>,
}

new_key_type! {
    pub struct SymbolID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Symbol {
    pub symbol_kind: SymbolKindID,
    pub endpoints: Vec<EndpointID>,
    pub position: Position,
    pub number: u32,
}

new_key_type! {
    pub struct EndpointID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum Endpoint {
    Attached { symbol: SymbolID, port: PortID },
    Free { position: Position },
}

new_key_type! {
    pub struct SubnetID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Subnet {
    pub name: Arc<str>,
    pub bits: Vec<u8>,
    pub endpoints: Vec<EndpointID>,
}

new_key_type! {
    pub struct NetID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Net {
    pub name: Arc<str>,
    pub subnets: Vec<SubnetID>,
}

new_key_type! {
    pub struct ModuleID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Module {
    pub name: Arc<str>,
    pub symbol_kind: SymbolKindID,
    pub symbols: Vec<SymbolID>,
    pub nets: Vec<NetID>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Project {
    pub modules: SlotMap<ModuleID, Module>,
    pub nets: SlotMap<NetID, Net>,
    pub subnets: SlotMap<SubnetID, Subnet>,
    pub endpoints: SlotMap<EndpointID, Endpoint>,
    pub symbols: SlotMap<SymbolID, Symbol>,
    pub symbol_kinds: SlotMap<SymbolKindID, SymbolKind>,
    pub ports: SlotMap<PortID, Port>,
}

impl Default for Project {
    fn default() -> Self {
        Self {
            modules: SlotMap::with_key(),
            nets: SlotMap::with_key(),
            subnets: SlotMap::with_key(),
            endpoints: SlotMap::with_key(),
            symbols: SlotMap::with_key(),
            symbol_kinds: SlotMap::with_key(),
            ports: SlotMap::with_key(),
        }
    }
}
