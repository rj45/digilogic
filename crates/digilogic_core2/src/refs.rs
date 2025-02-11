use crate::{structs, Project};

pub trait Mutability {
    type Ref<'a, T: ?Sized>: std::ops::Deref<Target = T>
    where
        T: 'a;
}

#[derive(Debug)]
pub enum Immutable {}

impl Mutability for Immutable {
    type Ref<'a, T: ?Sized>
        = &'a T
    where
        T: 'a;
}

#[derive(Debug)]
pub enum Mutable {}

impl Mutability for Mutable {
    type Ref<'a, T: ?Sized>
        = &'a mut T
    where
        T: 'a;
}

macro_rules! ref_type {
    ($name:ident, $t_id:ty, $t_deref:ty, $storage:ident) => {
        pub struct $name<'a, M: Mutability> {
            id: $t_id,
            project: M::Ref<'a, structs::Project>,
        }

        impl<M: Mutability> std::fmt::Debug for $name<'_, M> {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                std::fmt::Debug::fmt(&**self, f)
            }
        }

        impl Clone for $name<'_, Immutable> {
            fn clone(&self) -> Self {
                *self
            }
        }

        impl Copy for $name<'_, Immutable> {}

        impl<M: Mutability> std::ops::Deref for $name<'_, M> {
            type Target = $t_deref;

            fn deref(&self) -> &Self::Target {
                &self.project.$storage[self.id]
            }
        }

        impl std::ops::DerefMut for $name<'_, Mutable> {
            fn deref_mut(&mut self) -> &mut Self::Target {
                &mut self.project.$storage[self.id]
            }
        }

        impl<M: Mutability> $name<'_, M> {
            pub fn reborrow(&self) -> $name<Immutable> {
                $name {
                    id: self.id,
                    project: &*self.project,
                }
            }
        }

        impl $name<'_, Mutable> {
            pub fn reborrow_mut(&mut self) -> $name<Mutable> {
                $name {
                    id: self.id,
                    project: &mut *self.project,
                }
            }
        }
    };
}

ref_type!(PortRef, structs::PortID, structs::Port, ports);
ref_type!(
    SymbolKindRef,
    structs::SymbolKindID,
    structs::SymbolKind,
    symbol_kinds
);
ref_type!(SymbolRef, structs::SymbolID, structs::Symbol, symbols);
ref_type!(
    EndpointRef,
    structs::EndpointID,
    structs::Endpoint,
    endpoints
);
ref_type!(SubnetRef, structs::SubnetID, structs::Subnet, subnets);
ref_type!(NetRef, structs::NetID, structs::Net, nets);
ref_type!(ModuleRef, structs::ModuleID, structs::Module, modules);

macro_rules! one_to_one {
    ($name:ident, $t_foreign_ref:ident, $foreign_id:ident, $foreign_id_mut:ident) => {
        impl<M: Mutability> $name<'_, M> {
            pub fn $foreign_id(&self) -> $t_foreign_ref<Immutable> {
                $t_foreign_ref {
                    id: self.$foreign_id,
                    project: &*self.project,
                }
            }
        }

        impl $name<'_, Mutable> {
            pub fn $foreign_id_mut(&mut self) -> $t_foreign_ref<Mutable> {
                $t_foreign_ref {
                    id: self.$foreign_id,
                    project: &mut *self.project,
                }
            }
        }
    };
}

one_to_one!(PortRef, SymbolKindRef, symbol_kind, symbol_kind_mut);
// one_to_one!(SymbolKindRef, ModuleRef, module, module_mut);
one_to_one!(SymbolRef, SymbolKindRef, symbol_kind, symbol_kind_mut);
one_to_one!(ModuleRef, SymbolKindRef, symbol_kind, symbol_kind_mut);

impl<M: Mutability> EndpointRef<'_, M> {
    pub fn symbol(&self) -> Option<SymbolRef<Immutable>> {
        match &self.project.endpoints[self.id] {
            structs::Endpoint::Attached { symbol, .. } => Some(SymbolRef {
                id: *symbol,
                project: &*self.project,
            }),
            structs::Endpoint::Free { .. } => None,
        }
    }

    pub fn port(&self) -> Option<PortRef<Immutable>> {
        match &self.project.endpoints[self.id] {
            structs::Endpoint::Attached { port, .. } => Some(PortRef {
                id: *port,
                project: &*self.project,
            }),
            structs::Endpoint::Free { .. } => None,
        }
    }
}

impl EndpointRef<'_, Mutable> {
    pub fn symbol_mut(&mut self) -> Option<SymbolRef<Mutable>> {
        match &self.project.endpoints[self.id] {
            structs::Endpoint::Attached { symbol, .. } => Some(SymbolRef {
                id: *symbol,
                project: &mut *self.project,
            }),
            structs::Endpoint::Free { .. } => None,
        }
    }

    pub fn port_mut(&mut self) -> Option<PortRef<Mutable>> {
        match &self.project.endpoints[self.id] {
            structs::Endpoint::Attached { port, .. } => Some(PortRef {
                id: *port,
                project: &mut *self.project,
            }),
            structs::Endpoint::Free { .. } => None,
        }
    }
}

macro_rules! one_to_many {
    ($ref:ident, $foreign_ref:ident, $foreign_list:ident, $each_mut:ident) => {
        impl<M: Mutability> $ref<'_, M> {
            pub fn $foreign_list(
                &self,
            ) -> impl Iterator<Item = $foreign_ref<Immutable>> + use<'_, M> {
                let project = &*self.project;
                self.$foreign_list
                    .iter()
                    .map(move |&id| $foreign_ref { id, project })
            }
        }

        impl $ref<'_, Mutable> {
            pub fn $each_mut(&mut self, mut f: impl FnMut($foreign_ref<Mutable>)) {
                let item_len = self.$foreign_list.len();
                for i in 0..item_len {
                    let id = self.$foreign_list[i];
                    let project = &mut *self.project;
                    let item = $foreign_ref { id, project };
                    f(item);
                }
            }
        }
    };
}

one_to_many!(SymbolKindRef, PortRef, ports, each_port_mut);
one_to_many!(SymbolRef, EndpointRef, endpoints, each_endpoint_mut);
one_to_many!(SubnetRef, EndpointRef, endpoints, each_endpoint_mut);
one_to_many!(NetRef, SubnetRef, subnets, each_subnet_mut);
one_to_many!(ModuleRef, SymbolRef, symbols, each_symbol_mut);
one_to_many!(ModuleRef, NetRef, nets, each_net_mut);

macro_rules! project_wrapper {
    ($name:ident, $ref: ident, $each_mut: ident) => {
        pub fn $name(&self) -> impl Iterator<Item = $ref<Immutable>> {
            self.$name
                .iter()
                .map(move |(id, _)| $ref { id, project: self })
        }

        pub fn $each_mut(&mut self, mut f: impl FnMut($ref<Mutable>)) {
            let item_len = self.$name.len();
            let keys = self.$name.keys().collect::<Vec<_>>();
            for i in 0..item_len {
                let project = &mut *self;
                let id = keys[i];
                let item = $ref { id, project };
                f(item);
            }
        }
    };
}

impl Project {
    project_wrapper!(ports, PortRef, each_port_mut);
    project_wrapper!(symbol_kinds, SymbolKindRef, each_symbol_kind_mut);
    project_wrapper!(symbols, SymbolRef, each_symbol_mut);
    project_wrapper!(endpoints, EndpointRef, each_endpoint_mut);
    project_wrapper!(subnets, SubnetRef, each_subnet_mut);
    project_wrapper!(nets, NetRef, each_net_mut);
    project_wrapper!(modules, ModuleRef, each_module_mut);
}
