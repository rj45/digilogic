//! Import circuits from Yosys JSON format
//!
//! Use the following command to generate compatible JSON files, unoptimized:
//!
//! ```
//! yosys -p "read_verilog <VERILOG-FILE>; hierarchy -auto-top; proc; opt_clean; fsm -expand; memory -nomap; wreduce -memx; opt_clean; write_json <OUTPUT-FILE>"
//! ```
//! or optimized:
//!
//! ```
//! yosys -p "read_verilog <VERILOG-FILE>; hierarchy -auto-top; proc; opt; fsm -expand; memory -nomap; wreduce -memx; opt; write_json <OUTPUT-FILE>"
//! ```

use digilogic_core::SharedStr;
use serde::Deserialize;
use std::{collections::BTreeMap, path::Path, sync::Arc};

/// The known Yosys cell types
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub enum CellType {
    Not,
    Pos,
    Neg,
    ReduceAnd,
    ReduceOr,
    ReduceXor,
    ReduceXnor,
    ReduceBool,
    LogicNot,
    And,
    Or,
    Xor,
    Xnor,
    Shl,
    Sshl,
    Shr,
    Sshr,
    LogicAnd,
    LogicOr,
    EqX,
    NeX,
    Pow,
    Lt,
    Le,
    Eq,
    Ne,
    Ge,
    Gt,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    DivFloor,
    ModFloor,
    Mux,
    Pmux,
    TriBuf,
    Sr,
    Dff,
    Dffe,
    Sdff,
    Sdffe,
    Sdffce,
    Dlatch,
    MemRdV2,
    MemWrV2,
    MemInitV2,
    MemV2,
    Unknown(Arc<str>),
}

impl From<String> for CellType {
    fn from(value: String) -> Self {
        match value.as_str() {
            "$not" => Self::Not,
            "$pos" => Self::Pos,
            "$neg" => Self::Neg,
            "$reduce_and" => Self::ReduceAnd,
            "$reduce_or" => Self::ReduceOr,
            "$reduce_xor" => Self::ReduceXor,
            "$reduce_xnor" => Self::ReduceXnor,
            "$reduce_bool" => Self::ReduceBool,
            "$logic_not" => Self::LogicNot,
            "$and" => Self::And,
            "$or" => Self::Or,
            "$xor" => Self::Xor,
            "$xnor" => Self::Xnor,
            "$shl" => Self::Shl,
            "$sshl" => Self::Sshl,
            "$shr" => Self::Shr,
            "$sshr" => Self::Sshr,
            "$logic_and" => Self::LogicAnd,
            "$logic_or" => Self::LogicOr,
            "$eqx" => Self::EqX,
            "$nex" => Self::NeX,
            "$pow" => Self::Pow,
            "$lt" => Self::Lt,
            "$le" => Self::Le,
            "$eq" => Self::Eq,
            "$ne" => Self::Ne,
            "$ge" => Self::Ge,
            "$gt" => Self::Gt,
            "$add" => Self::Add,
            "$sub" => Self::Sub,
            "$mul" => Self::Mul,
            "$div" => Self::Div,
            "$mod" => Self::Mod,
            "$divfloor" => Self::DivFloor,
            "$modfloor" => Self::ModFloor,
            "$mux" => Self::Mux,
            "$pmux" => Self::Pmux,
            "$tribuf" => Self::TriBuf,
            "$sr" => Self::Sr,
            "$dff" => Self::Dff,
            "$dffe" => Self::Dffe,
            "$sdff" => Self::Sdff,
            "$sdffe" => Self::Sdffe,
            "$sdffce" => Self::Sdffce,
            "$dlatch" => Self::Dlatch,
            "$memrd_v2" => Self::MemRdV2,
            "$memwr_v2" => Self::MemWrV2,
            "$meminit_v2" => Self::MemInitV2,
            "$mem_v2" => Self::MemV2,
            _ => Self::Unknown(value.into()),
        }
    }
}

fn cell_type<'de, D>(deserializer: D) -> Result<CellType, D::Error>
where
    D: serde::Deserializer<'de>,
{
    let name = String::deserialize(deserializer)?;
    Ok(name.into())
}

#[derive(Clone, Copy, PartialEq, Eq, Deserialize)]
pub enum PortDirection {
    #[serde(rename = "input")]
    Input,
    #[serde(rename = "output")]
    Output,
    #[serde(rename = "inout")]
    InOut,
}

type NetId = usize;

#[derive(Clone, PartialEq, Eq, Hash, Deserialize)]
#[serde(untagged)]
pub enum Signal {
    Value(Arc<str>),
    Net(NetId),
}

/// LSB first
pub type Bits = Vec<Signal>;

#[derive(Deserialize)]
pub struct Port {
    pub direction: PortDirection,
    pub bits: Bits,
}

#[derive(Deserialize)]
pub struct Cell {
    #[serde(default)]
    pub hide_name: u8,
    #[serde(rename = "type", deserialize_with = "cell_type")]
    pub cell_type: CellType,
    #[serde(default)]
    pub parameters: BTreeMap<String, String>,
    pub port_directions: BTreeMap<String, PortDirection>,
    pub connections: BTreeMap<String, Bits>,
}

#[derive(Deserialize)]
pub struct NetNameOpts {
    #[serde(default)]
    pub hide_name: u8,
    pub bits: Bits,
}

#[derive(Deserialize)]
pub struct Module {
    pub ports: BTreeMap<SharedStr, Port>,
    #[serde(default)]
    pub cells: BTreeMap<SharedStr, Cell>,
    #[serde(default, rename = "netnames")]
    pub net_names: BTreeMap<SharedStr, NetNameOpts>,
}

#[derive(Deserialize)]
pub struct Netlist {
    pub modules: BTreeMap<SharedStr, Module>,
}

impl TryFrom<&str> for Netlist {
    type Error = serde_json::Error;

    #[inline]
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        serde_json::from_str(value)
    }
}

impl Netlist {
    pub fn load<P: AsRef<Path>>(path: P) -> anyhow::Result<Self> {
        let file = std::fs::File::open(path)?;
        let reader = std::io::BufReader::new(file);
        Ok(serde_json::from_reader(reader)?)
    }
}
