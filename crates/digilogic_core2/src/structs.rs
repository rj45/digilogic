use serde::{Deserialize, Serialize};
use slotmap::{new_key_type, SlotMap};

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
    pub name: String,
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
    pub name: String,
    pub prefix: String,
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
    pub name: String,
    pub bits: Vec<u8>,
    pub endpoints: Vec<EndpointID>,
}

new_key_type! {
    pub struct NetID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Net {
    pub name: String,
    pub subnets: Vec<SubnetID>,
}

new_key_type! {
    pub struct ModuleID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Module {
    pub name: String,
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
