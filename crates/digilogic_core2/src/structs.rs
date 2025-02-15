use std::{str::FromStr, sync::Arc};

use serde::{Deserialize, Serialize};
use slotmap::{new_key_type, SecondaryMap, SlotMap};

use crate::intern::Intern;

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
    pub module: Option<ModuleID>,
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
    pub module: ModuleID,
    pub symbol_kind: SymbolKindID,
    pub endpoints: Vec<EndpointID>,
    pub position: Position,
    pub number: u32,
}

impl ForeignKey<SymbolKindID> for Symbol {
    fn foreign_key(&self) -> SymbolKindID {
        self.symbol_kind
    }
}

impl ForeignKey<ModuleID> for Symbol {
    fn foreign_key(&self) -> ModuleID {
        self.module
    }
}

new_key_type! {
    pub struct EndpointID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum Endpoint {
    Attached {
        net: NetID,
        symbol: SymbolID,
        port: PortID,
    },
    Free {
        net: NetID,
        position: Position,
    },
}

new_key_type! {
    pub struct NetID;
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Net {
    pub name: Arc<str>,
    pub width: u32,
    pub endpoints: Vec<EndpointID>,
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
    pub endpoints: SlotMap<EndpointID, Endpoint>,
    pub symbols: SlotMap<SymbolID, Symbol>,
    pub symbol_kinds: SlotMap<SymbolKindID, SymbolKind>,
    pub ports: SlotMap<PortID, Port>,

    #[serde(skip)]
    pub intern: Intern,
}

impl Default for Project {
    fn default() -> Self {
        Self {
            modules: SlotMap::with_key(),
            nets: SlotMap::with_key(),
            endpoints: SlotMap::with_key(),
            symbols: SlotMap::with_key(),
            symbol_kinds: SlotMap::with_key(),
            ports: SlotMap::with_key(),
            intern: Intern::default(),
        }
    }
}

// FID: ModuleID, LID: SymbolID
pub struct ReverseIndex<LID: slotmap::Key, FID: slotmap::Key> {
    ranges: SecondaryMap<FID, (u32, u32)>,
    ids: Vec<LID>,
}

impl<LID: slotmap::Key, FID: slotmap::Key> ReverseIndex<LID, FID> {
    pub fn new<V, FK: ForeignKey<FID>>(
        foreign_table: &SlotMap<FID, V>,
        table: &SlotMap<LID, FK>,
    ) -> Self {
        let mut ranges = SecondaryMap::new();
        let mut ids = Vec::new();
        for (fid, _) in foreign_table.iter() {
            let start = ids.len() as u32;
            ids.extend(table.iter().filter_map(|(lid, fk)| {
                if fk.foreign_key() == fid {
                    Some(lid)
                } else {
                    None
                }
            }));
            let end = ids.len() as u32;
            ranges.insert(fid, (start, end));
        }
        Self { ranges, ids }
    }

    pub fn get(&self, fid: FID) -> &[LID] {
        let (start, end) = self.ranges[fid];
        &self.ids[start as usize..end as usize]
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_reverse_index() {
        let mut project = Project::default();
        let module_id = project.modules.insert(Module {
            name: "test".into(),
            symbol_kind: SymbolKindID::default(),
            symbols: Vec::new(),
            nets: Vec::new(),
        });

        let symbol_id1 = project.symbols.insert(Symbol {
            module: module_id,
            symbol_kind: SymbolKindID::default(),
            endpoints: Vec::new(),
            position: Position::default(),
            number: 0,
        });

        let symbol_id2 = project.symbols.insert(Symbol {
            module: module_id,
            symbol_kind: SymbolKindID::default(),
            endpoints: Vec::new(),
            position: Position::default(),
            number: 0,
        });

        let module_id2 = project.modules.insert(Module {
            name: "test2".into(),
            symbol_kind: SymbolKindID::default(),
            symbols: Vec::new(),
            nets: Vec::new(),
        });

        let symbol_id3 = project.symbols.insert(Symbol {
            module: module_id2,
            symbol_kind: SymbolKindID::default(),
            endpoints: Vec::new(),
            position: Position::default(),
            number: 0,
        });

        let symbol_id4 = project.symbols.insert(Symbol {
            module: module_id2,
            symbol_kind: SymbolKindID::default(),
            endpoints: Vec::new(),
            position: Position::default(),
            number: 0,
        });

        let reverse_index = ReverseIndex::new(&project.modules, &project.symbols);
        assert_eq!(reverse_index.get(module_id), &[symbol_id1, symbol_id2]);
        assert_eq!(reverse_index.get(module_id2), &[symbol_id3, symbol_id4]);
    }
}
