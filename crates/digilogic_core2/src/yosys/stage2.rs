use std::{
    collections::{BTreeMap, HashSet},
    sync::Arc,
};

use super::stage1;

#[derive(Debug)]
pub struct Port {
    pub name: Arc<str>,
    pub direction: crate::Direction,
    pub signed: bool,
    pub width: usize,
}

#[derive(Debug)]
pub struct Cell {
    pub name: Arc<str>,
    pub hidden_name: bool,
    pub cell_type: stage1::CellType,
    pub parameters: BTreeMap<Arc<str>, Arc<str>>,
    pub attributes: BTreeMap<Arc<str>, Arc<str>>,
    pub ports: Vec<Port>,
}

#[derive(Debug)]
pub struct Net {
    pub name: Arc<str>,
    pub hidden_name: bool,
    pub width: usize,
}

#[derive(Debug)]
pub struct Module {
    pub name: Arc<str>,
    pub ports: Vec<Port>,
    pub cells: Vec<Cell>,
    pub nets: Vec<Net>,
}

#[derive(Debug, Default)]
pub struct Project {
    pub modules: Vec<Module>,
    pub bits: Vec<Option<BitAssignment>>,
    pub extra_bits: Vec<BitAssignment>,
    pub const_bits: Vec<BitAssignment>,
}

#[derive(Debug, Clone, Default)]
pub struct NetAssignment {
    /// Index into `Module::nets` for the net
    index: usize,

    /// Which bit of the net this assignment is for
    bit: usize,
}

#[derive(Debug, Clone, Default)]
pub struct BitAssignment {
    name: Arc<str>,
    port: Arc<str>,
    bit: usize,
    value: Option<crate::WireState>,

    net: Option<NetAssignment>,

    /// Index into `extra_bits` for more than one
    /// assignment for this bit.
    next: Option<usize>,
}

#[derive(Debug, Default)]
pub struct Importer {
    project: Project,

    intern: HashSet<Arc<str>>,
}

impl Importer {
    pub fn import(reader: impl std::io::Read) -> anyhow::Result<Project> {
        let netlist = stage1::Netlist::import(reader)?;
        Importer::default().translate(&netlist)
    }

    fn intern(&mut self, name: &str) -> Arc<str> {
        match self.intern.get(name) {
            Some(name) => name.clone(),
            None => {
                let name: Arc<str> = Arc::from(name);
                self.intern.insert(name.clone());
                name
            }
        }
    }

    pub fn translate(mut self, netlist: &stage1::Netlist) -> anyhow::Result<Project> {
        for (name, module) in netlist.modules.iter() {
            let name = self.intern(name);
            let ports = self.translate_ports(name.clone(), &module.ports);
            let cells = self.translate_cells(&module.cells);
            let nets = self.translate_nets(&module.net_names);
            self.project.modules.push(Module {
                name,
                ports,
                cells,
                nets,
            });
        }
        Ok(self.project)
    }

    fn translate_bits(
        &mut self,
        bits: &[stage1::Signal],
        comp_name: Arc<str>,
        port_name: Arc<str>,
        offset: usize,
        msb_first: bool,
    ) {
        let total_bits = bits.len() + offset;
        for (bit, signal) in bits.iter().enumerate() {
            let bit = bit + offset;
            let bit = if msb_first { total_bits - bit - 1 } else { bit };
            match signal {
                stage1::Signal::Value(value) => {
                    self.project.const_bits.push(BitAssignment {
                        name: comp_name.clone(),
                        port: port_name.clone(),
                        bit,
                        value: Some(value.parse().unwrap_or_default()),
                        net: None,
                        next: None,
                    });
                }
                stage1::Signal::Net(net) => {
                    if self.project.bits.len() <= *net {
                        self.project.bits.resize(*net + 1, None);
                    }
                    let next = match self.project.bits[*net].clone() {
                        None => None,
                        Some(old_bit) => {
                            let next = self.project.extra_bits.len();
                            self.project.extra_bits.push(old_bit);
                            Some(next)
                        }
                    };

                    self.project.bits[*net] = Some(BitAssignment {
                        name: comp_name.clone(),
                        port: port_name.clone(),
                        bit,
                        value: None,
                        net: None,
                        next,
                    });
                }
            }
        }
    }

    fn translate_ports(
        &mut self,
        module_name: Arc<str>,
        ports: &BTreeMap<String, stage1::Port>,
    ) -> Vec<Port> {
        ports
            .iter()
            .map(|(name, port)| {
                let name = self.intern(name);
                self.translate_bits(
                    &port.bits,
                    module_name.clone(),
                    name.clone(),
                    port.offset as usize,
                    port.msb_first != 0,
                );
                Port {
                    name,
                    direction: Self::translate_direction(port.direction),
                    signed: port.signed != 0,
                    width: port.bits.len(),
                }
            })
            .collect()
    }

    fn translate_direction(direction: stage1::PortDirection) -> crate::Direction {
        match direction {
            stage1::PortDirection::Input => crate::Direction::In,
            stage1::PortDirection::Output => crate::Direction::Out,
            stage1::PortDirection::InOut => crate::Direction::InOut,
        }
    }

    fn translate_cells(&mut self, cells: &BTreeMap<String, stage1::Cell>) -> Vec<Cell> {
        cells
            .iter()
            .map(|(name, cell)| {
                let name = self.intern(name);
                let parameters = cell
                    .parameters
                    .iter()
                    .map(|(k, v)| {
                        let k = self.intern(k);
                        let v = self.intern(v);
                        (k, v)
                    })
                    .collect::<BTreeMap<_, _>>();
                let attributes = cell
                    .attributes
                    .iter()
                    .map(|(k, v)| {
                        let k = self.intern(k);
                        let v = self.intern(v);
                        (k, v)
                    })
                    .collect::<BTreeMap<_, _>>();
                let ports = cell
                    .port_directions
                    .iter()
                    .map(|(orig_name, direction)| {
                        let signed_value =
                            parameters.get(format!("{}_SIGNED", name).as_str()).cloned();
                        let signed = if let Some(signed) = signed_value {
                            isize::from_str_radix(&signed, 1).unwrap_or_default() != 0
                        } else {
                            false
                        };
                        let port_name = self.intern(orig_name);
                        self.translate_bits(
                            &cell.connections[orig_name],
                            name.clone(),
                            port_name.clone(),
                            0,
                            false,
                        );
                        Port {
                            name: port_name,
                            direction: Self::translate_direction(*direction),
                            signed,
                            width: cell.connections[orig_name].len(),
                        }
                    })
                    .collect();
                Cell {
                    name,
                    hidden_name: cell.hide_name != 0,
                    cell_type: cell.cell_type.clone(),
                    parameters,
                    attributes,
                    ports,
                }
            })
            .collect()
    }

    fn translate_nets(&mut self, net_names: &BTreeMap<String, stage1::NetNameOpts>) -> Vec<Net> {
        net_names
            .iter()
            .enumerate()
            .map(|(index, (name, net))| {
                let name = self.intern(name);
                for (bit_index, bit) in net.bits.iter().enumerate() {
                    match bit {
                        stage1::Signal::Value(value) => {
                            self.project.const_bits.push(BitAssignment {
                                name: name.clone(),
                                port: name.clone(),
                                bit: bit_index,
                                value: Some(value.parse().unwrap_or_default()),
                                net: Some(NetAssignment {
                                    index,
                                    bit: bit_index,
                                }),
                                next: None,
                            });
                        }
                        stage1::Signal::Net(net) => {
                            if let Some(bit_assign) = &mut self.project.bits[*net] {
                                bit_assign.net = Some(NetAssignment {
                                    index,
                                    bit: bit_index,
                                });
                            }
                        }
                    }
                }
                Net {
                    name,
                    hidden_name: net.hide_name != 0,
                    width: net.bits.len(),
                }
            })
            .collect()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    /// make sure interning works as expected
    #[test]
    fn test_interner() {
        let mut importer = Importer::default();

        let one = importer.intern("foo");
        {
            let two = importer.intern("foo");
            assert_eq!(one, two);
            assert_eq!(Arc::strong_count(&one), 3);
            assert_eq!(Arc::strong_count(&two), 3);
        }
        assert_eq!(Arc::strong_count(&one), 2);
    }

    #[test]
    fn test_translation() {
        let project = Importer::import(
            std::fs::File::open("../../crates/digilogic/assets/testdata/large.yosys")
                .expect("failed to open file"),
        )
        .expect("failed to import file");

        println!("{:?}", project.bits);
        println!("{:?}", project.bits.len());
        println!("{:?}", project.extra_bits.len());
    }
}
