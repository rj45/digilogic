// #![allow(dead_code)]
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

use serde::Deserialize;
use std::collections::BTreeMap;

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
    Unknown(String),
}

impl From<String> for CellType {
    fn from(value: String) -> Self {
        match value.as_ref() {
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
            _ => Self::Unknown(value),
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
    Value(String),
    Net(NetId),
}

/// LSB first
pub type Bits = Vec<Signal>;

#[derive(Deserialize)]
pub struct Port {
    pub direction: PortDirection,
    pub bits: Bits,
    #[serde(default)]
    pub offset: u64,
    #[serde(default, rename = "upto")]
    pub msb_first: u32,
    #[serde(default)]
    pub signed: u32,
}

#[derive(Deserialize)]
pub struct Cell {
    #[serde(default)]
    pub hide_name: u8,
    #[serde(rename = "type", deserialize_with = "cell_type")]
    pub cell_type: CellType,
    #[serde(default)]
    pub parameters: BTreeMap<String, String>,
    #[serde(default)]
    pub attributes: BTreeMap<String, String>,
    #[serde(default)]
    pub port_directions: BTreeMap<String, PortDirection>,
    pub connections: BTreeMap<String, Bits>,
}

#[derive(Deserialize)]
pub struct Memory {
    #[serde(default)]
    pub hide_name: u8,
    pub attributes: BTreeMap<String, String>,
    pub width: u32,
    pub start_address: u64,
    pub size: u64,
}

#[derive(Deserialize)]
pub struct NetNameOpts {
    #[serde(default)]
    pub hide_name: u8,
    pub bits: Bits,
}

#[derive(Deserialize)]
pub struct Module {
    pub ports: BTreeMap<String, Port>,
    #[serde(default)]
    pub cells: BTreeMap<String, Cell>,
    #[serde(default)]
    pub memories: BTreeMap<String, Memory>,
    #[serde(default, rename = "netnames")]
    pub net_names: BTreeMap<String, NetNameOpts>,
}

#[derive(Deserialize)]
#[serde(untagged)]
pub enum IntOrString {
    String(String),
    Int(usize),
}

#[derive(Deserialize)]
#[serde(tag = "0", rename_all = "lowercase")]
pub enum ModelNode {
    Port(Vec<IntOrString>),
    Nport(Vec<IntOrString>),
    And(Vec<IntOrString>),
    Nand(Vec<IntOrString>),
    True(Vec<IntOrString>),
    False(Vec<IntOrString>),
}

pub type Model = Vec<ModelNode>;

#[derive(Deserialize)]
pub struct Netlist {
    pub creator: String,
    pub modules: BTreeMap<String, Module>,
    #[serde(default)]
    pub models: BTreeMap<String, Model>,
}

impl Netlist {
    pub fn import(reader: impl std::io::Read) -> anyhow::Result<Self> {
        Ok(serde_json::from_reader(reader)?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_doc_example() {
        let json = json!({
          "creator": "Yosys 0.48+98 (git sha1 3f4f6c17d, aarch64-apple-darwin22.4-clang++ 18.1.8 -fPIC -O3)",
          "modules": {
            "test": {
              "attributes": {
                "top": "00000000000000000000000000000001",
                "src": "example.v:1.1-4.14"
              },
              "ports": {
                "x": {
                  "direction": "input",
                  "bits": [ 2 ]
                },
                "y": {
                  "direction": "input",
                  "bits": [ 3 ]
                }
              },
              "cells": {
                "foo_inst": {
                  "hide_name": 0,
                  "type": "foo",
                  "parameters": {
                    "P": "00000000000000000000000000101010",
                    "Q": "00000000000000000000010100111001"
                  },
                  "attributes": {
                    "keep": "00000000000000000000000000000001",
                    "module_not_derived": "00000000000000000000000000000001",
                    "src": "example.v:3.11-3.65"
                  },
                  "connections": {
                    "A": [ 3, 2 ],
                    "B": [ 2, 3 ],
                    "C": [ 2, 2, 2, 2, "0", "1", "0", "1" ]
                  }
                }
              },
              "netnames": {
                "x": {
                  "hide_name": 0,
                  "bits": [ 2 ],
                  "attributes": {
                    "src": "example.v:1.19-1.20"
                  }
                },
                "y": {
                  "hide_name": 0,
                  "bits": [ 3 ],
                  "attributes": {
                    "src": "example.v:1.22-1.23"
                  }
                }
              }
            }
          }
        });

        let _netlist: Netlist = serde_json::from_value(json).unwrap();
    }

    #[test]
    fn test_aig_model() {
        let json = json!({
          "creator": "Yosys 0.48+98 (git sha1 3f4f6c17d, aarch64-apple-darwin22.4-clang++ 18.1.8 -fPIC -O3)",
          "modules": {
            "test": {
              "attributes": {
                "top": "00000000000000000000000000000001",
                "src": "example.v:1.1-3.14"
              },
              "ports": {
                "in": {
                  "direction": "input",
                  "bits": [ 2, 3, 4 ]
                },
                "out": {
                  "direction": "output",
                  "bits": [ 5, "0", "0" ]
                }
              },
              "cells": {
                "$reduce_and$example.v:2$1": {
                  "hide_name": 1,
                  "type": "$reduce_and",
                  "model": "$reduce_and:3U:1",
                  "parameters": {
                    "A_SIGNED": "00000000000000000000000000000000",
                    "A_WIDTH": "00000000000000000000000000000011",
                    "Y_WIDTH": "00000000000000000000000000000001"
                  },
                  "attributes": {
                    "src": "example.v:2.20-2.23"
                  },
                  "port_directions": {
                    "A": "input",
                    "Y": "output"
                  },
                  "connections": {
                    "A": [ 2, 3, 4 ],
                    "Y": [ 5 ]
                  }
                }
              },
              "netnames": {
                "in": {
                  "hide_name": 0,
                  "bits": [ 2, 3, 4 ],
                  "attributes": {
                    "src": "example.v:1.25-1.27"
                  }
                },
                "out": {
                  "hide_name": 0,
                  "bits": [ 5, "0", "0" ],
                  "attributes": {
                    "src": "example.v:1.42-1.45"
                  }
                }
              }
            }
          },
          "models": {
            "$reduce_and:3U:1": [
              /*   0 */ [ "port", "A", 0 ],
              /*   1 */ [ "port", "A", 1 ],
              /*   2 */ [ "and", 0, 1 ],
              /*   3 */ [ "port", "A", 2 ],
              /*   4 */ [ "and", 2, 3, "Y", 0 ]
            ]
          }
        });

        let _netlist: Netlist = serde_json::from_value(json).unwrap();
    }
}
