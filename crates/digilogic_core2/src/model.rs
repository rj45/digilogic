use std::sync::Arc;

use serde::{Deserialize, Serialize};
use smallvec::SmallVec;

use crate::{
    intern::Intern,
    table::{Id, Table},
};

mod builder;
mod value;

pub use builder::*;
pub use value::*;

pub trait Parent<T> {
    fn parent(&self) -> T;
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

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub enum ParamValue {
    #[default]
    None,
    String(Arc<str>),
    Integer(i64),
    Float(f64),
    Boolean(bool),
    Value(Value),
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub enum Param {
    #[default]
    None,
    // Data / General bit width
    BitWidth(u32),
    NumInputs(u32),
    NumOutputs(u32),
    // Address / Selector width
    AddressWidth(u32),
    Named(Arc<str>, ParamValue),
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct SymbolParam {
    pub symbol: Id<Symbol>,
    pub param: Param,
}

impl Parent<Id<Symbol>> for SymbolParam {
    fn parent(&self) -> Id<Symbol> {
        self.symbol
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct SymbolKindParam {
    pub symbol_kind: Id<SymbolKind>,
    pub param: Param,
}

impl Parent<Id<SymbolKind>> for SymbolKindParam {
    fn parent(&self) -> Id<SymbolKind> {
        self.symbol_kind
    }
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
    pub ports: SmallVec<[Id<Port>; 4]>,
    pub size: Size,
    pub name: Arc<str>,
    pub prefix: Arc<str>,
    pub params: SmallVec<[Id<SymbolKindParam>; 4]>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Symbol {
    pub module: Id<Module>,
    pub symbol_kind: Id<SymbolKind>,
    pub endpoints: SmallVec<[Id<Endpoint>; 4]>,
    pub position: Position,
    pub number: u32,
    pub params: SmallVec<[Id<SymbolParam>; 4]>,
}

impl Parent<Id<SymbolKind>> for Symbol {
    fn parent(&self) -> Id<SymbolKind> {
        self.symbol_kind
    }
}

impl Parent<Id<Module>> for Symbol {
    fn parent(&self) -> Id<Module> {
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
    pub endpoints: SmallVec<[Id<Endpoint>; 4]>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Module {
    pub name: Arc<str>,
    pub symbol_kind: Id<SymbolKind>,
    pub symbols: SmallVec<[Id<Symbol>; 4]>,
    pub nets: SmallVec<[Id<Net>; 4]>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Project {
    modules: Table<Module>,
    nets: Table<Net>,
    endpoints: Table<Endpoint>,
    symbols: Table<Symbol>,
    symbol_kinds: Table<SymbolKind>,
    ports: Table<Port>,
    symbol_params: Table<SymbolParam>,
    symbol_kind_params: Table<SymbolKindParam>,

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
            ports: SmallVec::new(),
            size: Size::default(),
            prefix: "U".into(),
            params: SmallVec::new(),
        });

        let module_id = self.modules.insert(Module {
            name,
            symbol_kind: kind_id,
            symbols: SmallVec::new(),
            nets: SmallVec::new(),
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

    pub fn add_module_param(&mut self, module: Id<Module>, param: Param) -> Id<SymbolKindParam> {
        let symbol_kind = self.modules[module].symbol_kind;
        let param_id = self
            .symbol_kind_params
            .insert(SymbolKindParam { symbol_kind, param });
        self.symbol_kinds
            .get_mut(&symbol_kind)
            .unwrap()
            .params
            .push(param_id);
        param_id
    }

    pub fn add_builtin_symbol_kind(&mut self, name: &str) -> Id<SymbolKind> {
        let name = self.intern(name);
        self.symbol_kinds.insert(SymbolKind {
            name,
            module: None,
            ports: SmallVec::new(),
            size: Size::default(),
            prefix: "U".into(),
            params: SmallVec::new(),
        })
    }

    pub fn add_builtin_symbol_kind_param(
        &mut self,
        symbol_kind: Id<SymbolKind>,
        param: Param,
    ) -> Id<SymbolKindParam> {
        let param_id = self
            .symbol_kind_params
            .insert(SymbolKindParam { symbol_kind, param });
        self.symbol_kinds
            .get_mut(&symbol_kind)
            .unwrap()
            .params
            .push(param_id);
        param_id
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
            endpoints: SmallVec::new(),
            position: Position::default(),
            number: max_number + 1,
            params: SmallVec::new(),
        });
        self.symbol_kinds
            .get(&symbol_kind)
            .unwrap()
            .params
            .iter()
            .for_each(|&param_id| {
                self.symbol_params.insert(SymbolParam {
                    symbol: symbol_id,
                    param: self.symbol_kind_params[param_id].param.clone(),
                });
            });
        self.modules
            .get_mut(&module)
            .unwrap()
            .symbols
            .push(symbol_id);
        symbol_id
    }
}
