use std::sync::Arc;

use crate::{
    intern::Intern, Direction, Module, ModuleID, Port, PortID, Position, Project, Size, Symbol,
    SymbolID, SymbolKind, SymbolKindID,
};

#[derive(Debug, Default)]
pub struct ProjectBuilder {
    project: Project,
}

impl ProjectBuilder {
    pub fn from(project: Project) -> Self {
        Self { project }
    }

    pub fn build(self) -> Project {
        self.project
    }

    fn intern(&mut self, s: &str) -> Arc<str> {
        self.project.intern.intern(s)
    }

    fn merge_intern(&mut self, other: &Intern) {
        self.project.intern.merge(other);
    }

    pub fn add_module(&mut self, name: &str) -> (ModuleID, SymbolKindID) {
        let name = self.intern(name);
        let module_id = self.project.modules.insert(Module {
            name: name.clone(),
            symbol_kind: SymbolKindID::default(),
            symbols: Vec::new(),
            nets: Vec::new(),
        });

        let kind_id = self.project.symbol_kinds.insert(SymbolKind {
            name,
            module: Some(module_id),
            ports: Vec::new(),
            size: Size::default(),
            prefix: "U".into(),
        });

        self.project.modules[module_id].symbol_kind = kind_id;

        (module_id, kind_id)
    }

    pub fn add_port(&mut self, module: ModuleID, name: &str, direction: Direction) -> PortID {
        let name = self.intern(name);
        let symbol_kind = self.project.modules[module].symbol_kind;
        let port_id = self.project.ports.insert(Port {
            name: name.clone(),
            direction,
            symbol_kind,
            position: Position::default(),
            pin: self.project.symbol_kinds[symbol_kind].ports.len() as u32,
        });
        self.project.symbol_kinds[symbol_kind].ports.push(port_id);
        port_id
    }

    pub fn add_builtin_symbol_kind(&mut self, name: &str) -> SymbolKindID {
        let name = self.intern(name);
        self.project.symbol_kinds.insert(SymbolKind {
            name,
            module: None,
            ports: Vec::new(),
            size: Size::default(),
            prefix: "U".into(),
        })
    }

    pub fn add_builtin_symbol_kind_port(
        &mut self,
        symbol_kind: SymbolKindID,
        name: &str,
        direction: Direction,
    ) -> PortID {
        let name = self.intern(name);
        let port_id = self.project.ports.insert(Port {
            name: name.clone(),
            direction,
            symbol_kind,
            position: Position::default(),
            pin: self.project.symbol_kinds[symbol_kind].ports.len() as u32,
        });
        self.project.symbol_kinds[symbol_kind].ports.push(port_id);
        port_id
    }

    pub fn add_symbol(&mut self, module: ModuleID, symbol_kind: SymbolKindID) -> SymbolID {
        let max_number = self.project.modules[module]
            .symbols
            .iter()
            .map(|&id| self.project.symbols[id].number)
            .max()
            .unwrap_or(0);
        let symbol_id = self.project.symbols.insert(Symbol {
            module,
            symbol_kind,
            endpoints: Vec::new(),
            position: Position::default(),
            number: max_number + 1,
        });
        self.project.modules[module].symbols.push(symbol_id);
        symbol_id
    }
}
