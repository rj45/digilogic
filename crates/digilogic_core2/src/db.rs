use std::{str::FromStr, sync::Arc};

use serde::{Deserialize, Serialize};

use crate::{
    intern::Intern,
    table::{Id, Table},
};

mod builder;
pub use builder::*;

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
    modules: Table<Module>,
    nets: Table<Net>,
    endpoints: Table<Endpoint>,
    symbols: Table<Symbol>,
    symbol_kinds: Table<SymbolKind>,
    ports: Table<Port>,

    #[serde(skip)]
    intern: Intern,
}

impl Project {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn modules(&self) -> &Table<Module> {
        &self.modules
    }

    pub fn nets(&self) -> &Table<Net> {
        &self.nets
    }

    pub fn endpoints(&self) -> &Table<Endpoint> {
        &self.endpoints
    }

    pub fn symbols(&self) -> &Table<Symbol> {
        &self.symbols
    }

    pub fn symbol_kinds(&self) -> &Table<SymbolKind> {
        &self.symbol_kinds
    }

    pub fn ports(&self) -> &Table<Port> {
        &self.ports
    }

    pub fn builder(self) -> ProjectBuilder {
        ProjectBuilder::from(self)
    }

    fn intern(&mut self, s: &str) -> Arc<str> {
        self.intern.intern(s)
    }

    pub fn merge_intern(&mut self, other: &Intern) {
        self.intern.merge(other);
    }

    pub fn add_module(&mut self, name: &str) -> (Id<Module>, Id<SymbolKind>) {
        let name = self.intern(name);

        let kind_id = self.symbol_kinds.insert(SymbolKind {
            name: name.clone(),
            module: None,
            ports: Vec::new(),
            size: Size::default(),
            prefix: "U".into(),
        });

        let module_id = self.modules.insert(Module {
            name,
            symbol_kind: kind_id,
            symbols: Vec::new(),
            nets: Vec::new(),
        });

        self.symbol_kinds.get_mut(&kind_id).unwrap().module = Some(module_id);

        (module_id, kind_id)
    }

    pub fn add_port(&mut self, module: Id<Module>, name: &str, direction: Direction) -> Id<Port> {
        let name = self.intern(name);
        let symbol_kind = self.modules[module].symbol_kind;
        let port_id = self.ports.insert(Port {
            name: name.clone(),
            direction,
            symbol_kind,
            position: Position::default(),
            pin: self.symbol_kinds[symbol_kind].ports.len() as u32,
        });
        self.symbol_kinds
            .get_mut(&symbol_kind)
            .unwrap()
            .ports
            .push(port_id);
        port_id
    }

    pub fn add_builtin_symbol_kind(&mut self, name: &str) -> Id<SymbolKind> {
        let name = self.intern(name);
        self.symbol_kinds.insert(SymbolKind {
            name,
            module: None,
            ports: Vec::new(),
            size: Size::default(),
            prefix: "U".into(),
        })
    }

    pub fn add_builtin_symbol_kind_port(
        &mut self,
        symbol_kind: Id<SymbolKind>,
        name: &str,
        direction: Direction,
    ) -> Id<Port> {
        let name = self.intern(name);
        let port_id = self.ports.insert(Port {
            name: name.clone(),
            direction,
            symbol_kind,
            position: Position::default(),
            pin: self.symbol_kinds[symbol_kind].ports.len() as u32,
        });
        self.symbol_kinds
            .get_mut(&symbol_kind)
            .unwrap()
            .ports
            .push(port_id);
        port_id
    }

    pub fn add_symbol(&mut self, module: Id<Module>, symbol_kind: Id<SymbolKind>) -> Id<Symbol> {
        let max_number = self.modules[module]
            .symbols
            .iter()
            .map(|&id| self.symbols[id].number)
            .max()
            .unwrap_or(0);
        let symbol_id = self.symbols.insert(Symbol {
            module,
            symbol_kind,
            endpoints: Vec::new(),
            position: Position::default(),
            number: max_number + 1,
        });
        self.modules
            .get_mut(&module)
            .unwrap()
            .symbols
            .push(symbol_id);
        symbol_id
    }
}
