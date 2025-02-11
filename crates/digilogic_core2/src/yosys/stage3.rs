use std::{collections::HashMap, sync::Arc};

use crate::{ModuleID, PortID, SymbolID, SymbolKindID};

use super::{stage1, stage2};

pub struct Importer {
    project: stage2::Project,
    modules: HashMap<Arc<str>, ModuleID>,
    ports: HashMap<Arc<str>, PortID>,
    symbols: HashMap<Arc<str>, SymbolID>,
    cell_symbol_kinds: HashMap<stage1::CellType, SymbolKindID>,
    builder: crate::ProjectBuilder,
}

impl Importer {
    pub fn import_into(
        reader: impl std::io::Read,
        project: crate::Project,
    ) -> anyhow::Result<crate::Project> {
        let stage2 = stage2::Importer::import(reader)?;
        let builder = crate::ProjectBuilder::from(project);
        let mut importer = Importer {
            project: stage2,
            modules: HashMap::new(),
            ports: HashMap::new(),
            symbols: HashMap::new(),
            cell_symbol_kinds: HashMap::new(),
            builder,
        };
        importer.translate()?;
        Ok(importer.builder.build())
    }

    fn translate(&mut self) -> anyhow::Result<()> {
        self.translate_modules()?;
        self.translate_symbols()?;

        Ok(())
    }

    fn translate_modules(&mut self) -> anyhow::Result<()> {
        for module in self.project.modules.iter() {
            let (module_id, kind_id) = self.builder.add_module(&module.name);
            self.modules.insert(module.name.clone(), module_id);
            self.cell_symbol_kinds
                .insert(stage1::CellType::Unknown(module.name.clone()), kind_id);

            for port in module.ports.iter() {
                let port_id = self.builder.add_port(module_id, &port.name, port.direction);
                self.ports.insert(port.name.clone(), port_id);
            }
        }
        Ok(())
    }

    fn translate_symbols(&mut self) -> anyhow::Result<()> {
        for module in self.project.modules.iter() {
            let module_id = self.modules[&module.name];
            for cell in module.cells.iter() {
                let symbol_kind = if let Some(kind) = self.cell_symbol_kinds.get(&cell.cell_type) {
                    *kind
                } else {
                    let name = &cell.cell_type.to_string();
                    let kind_id = self.builder.add_builtin_symbol_kind(name);
                    self.cell_symbol_kinds
                        .insert(cell.cell_type.clone(), kind_id);
                    for port in cell.ports.iter() {
                        self.builder.add_builtin_symbol_kind_port(
                            kind_id,
                            &port.name,
                            port.direction,
                        );
                    }
                    kind_id
                };

                let cell_id = self.builder.add_symbol(module_id, symbol_kind);
                self.symbols.insert(cell.name.clone(), cell_id);
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use serde_json::json;

    use super::*;
    use crate::Direction;

    #[test]
    fn test_importer() {
        let input = json!({
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

        let text = serde_json::to_string(&input).unwrap();
        let reader = std::io::BufReader::new(text.as_bytes());

        let project = Importer::import_into(reader, crate::Project::default()).unwrap();

        println!("{:#?}", project);

        // Project {
        //     modules: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: Module {
        //                     name: "test",
        //                     symbol_kind: SymbolKindID(
        //                         1v1,
        //                     ),
        //                     symbols: [
        //                         SymbolID(
        //                             1v1,
        //                         ),
        //                     ],
        //                     nets: [],
        //                 },
        //             },
        //         ],
        //         free_head: 2,
        //         num_elems: 1,
        //         _k: PhantomData<fn(digilogic_core2::structs::ModuleID) -> digilogic_core2::structs::ModuleID>,
        //     },
        //     nets: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //         ],
        //         free_head: 1,
        //         num_elems: 0,
        //         _k: PhantomData<fn(digilogic_core2::structs::NetID) -> digilogic_core2::structs::NetID>,
        //     },
        //     subnets: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //         ],
        //         free_head: 1,
        //         num_elems: 0,
        //         _k: PhantomData<fn(digilogic_core2::structs::SubnetID) -> digilogic_core2::structs::SubnetID>,
        //     },
        //     endpoints: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //         ],
        //         free_head: 1,
        //         num_elems: 0,
        //         _k: PhantomData<fn(digilogic_core2::structs::EndpointID) -> digilogic_core2::structs::EndpointID>,
        //     },
        //     symbols: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: Symbol {
        //                     symbol_kind: SymbolKindID(
        //                         2v1,
        //                     ),
        //                     endpoints: [],
        //                     position: Position {
        //                         x: 0,
        //                         y: 0,
        //                     },
        //                     number: 1,
        //                 },
        //             },
        //         ],
        //         free_head: 2,
        //         num_elems: 1,
        //         _k: PhantomData<fn(digilogic_core2::structs::SymbolID) -> digilogic_core2::structs::SymbolID>,
        //     },
        //     symbol_kinds: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: SymbolKind {
        //                     module: Some(
        //                         ModuleID(
        //                             1v1,
        //                         ),
        //                     ),
        //                     ports: [
        //                         PortID(
        //                             1v1,
        //                         ),
        //                         PortID(
        //                             2v1,
        //                         ),
        //                     ],
        //                     size: Size {
        //                         width: 0,
        //                         height: 0,
        //                     },
        //                     name: "test",
        //                     prefix: "U",
        //                 },
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: SymbolKind {
        //                     module: None,
        //                     ports: [],
        //                     size: Size {
        //                         width: 0,
        //                         height: 0,
        //                     },
        //                     name: "foo",
        //                     prefix: "U",
        //                 },
        //             },
        //         ],
        //         free_head: 3,
        //         num_elems: 2,
        //         _k: PhantomData<fn(digilogic_core2::structs::SymbolKindID) -> digilogic_core2::structs::SymbolKindID>,
        //     },
        //     ports: SlotMap {
        //         slots: [
        //             Slot {
        //                 version: 0,
        //                 next_free: 0,
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: Port {
        //                     symbol_kind: SymbolKindID(
        //                         1v1,
        //                     ),
        //                     name: "x",
        //                     direction: In,
        //                     position: Position {
        //                         x: 0,
        //                         y: 0,
        //                     },
        //                     pin: 0,
        //                 },
        //             },
        //             Slot {
        //                 version: 1,
        //                 value: Port {
        //                     symbol_kind: SymbolKindID(
        //                         1v1,
        //                     ),
        //                     name: "y",
        //                     direction: In,
        //                     position: Position {
        //                         x: 0,
        //                         y: 0,
        //                     },
        //                     pin: 1,
        //                 },
        //             },
        //         ],
        //         free_head: 3,
        //         num_elems: 2,
        //         _k: PhantomData<fn(digilogic_core2::structs::PortID) -> digilogic_core2::structs::PortID>,
        //     },
        //     intern: Intern(
        //         {
        //             8312289520117458465: [
        //                 (Weak),
        //             ],
        //             14402189752926126668: [
        //                 (Weak),
        //             ],
        //             4506850079084802999: [
        //                 (Weak),
        //             ],
        //             4497542318236667727: [
        //                 (Weak),
        //             ],
        //         },
        //     ),
        // }

        let module = project.modules().next().unwrap();
        assert_eq!(module.name, "test".into());

        let symbol_kind = module.symbol_kind();
        assert_eq!(symbol_kind.name, "test".into());

        let ports = symbol_kind.ports().collect::<Vec<_>>();
        assert_eq!(ports.len(), 2);

        let port = ports[0];
        assert_eq!(port.name, "x".into());
        assert_eq!(port.direction, Direction::In);

        let port = ports[1];
        assert_eq!(port.name, "y".into());
        assert_eq!(port.direction, Direction::In);
    }
}
