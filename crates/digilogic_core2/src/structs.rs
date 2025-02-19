use std::{str::FromStr, sync::Arc};

use serde::{Deserialize, Serialize};

use crate::{
    intern::Intern,
    table::{Id, Table},
};

pub trait ForeignKey<T> {
    fn foreign_key(&self) -> T;
}

#[derive(Default, Debug, Clone, Copy, Deserialize, Serialize, PartialEq, Eq)]
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

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub enum Direction {
    #[default]
    In,
    Out,
    InOut,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct Size {
    pub width: u32,
    pub height: u32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Port {
    pub symbol_kind: Id<SymbolKind>,
    pub name: Arc<str>,
    pub direction: Direction,
    pub position: Position,
    pub pin: u32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SymbolKind {
    pub module: Option<Id<Module>>,
    pub ports: Vec<Id<Port>>,
    pub size: Size,
    pub name: Arc<str>,
    pub prefix: Arc<str>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Symbol {
    pub module: Id<Module>,
    pub symbol_kind: Id<SymbolKind>,
    pub endpoints: Vec<Id<Endpoint>>,
    pub position: Position,
    pub number: u32,
}

impl ForeignKey<Id<SymbolKind>> for Symbol {
    fn foreign_key(&self) -> Id<SymbolKind> {
        self.symbol_kind
    }
}

impl ForeignKey<Id<Module>> for Symbol {
    fn foreign_key(&self) -> Id<Module> {
        self.module
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum Endpoint {
    Attached {
        net: Id<Net>,
        symbol: Id<Symbol>,
        port: Id<Port>,
    },
    Free {
        net: Id<Net>,
        position: Position,
    },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Net {
    pub name: Arc<str>,
    pub width: u32,
    pub endpoints: Vec<Id<Endpoint>>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Module {
    pub name: Arc<str>,
    pub symbol_kind: Id<SymbolKind>,
    pub symbols: Vec<Id<Symbol>>,
    pub nets: Vec<Id<Net>>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Project {
    pub modules: Table<Module>,
    pub nets: Table<Net>,
    pub endpoints: Table<Endpoint>,
    pub symbols: Table<Symbol>,
    pub symbol_kinds: Table<SymbolKind>,
    pub ports: Table<Port>,

    #[serde(skip)]
    pub intern: Intern,
}
