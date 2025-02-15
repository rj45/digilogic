#![allow(missing_debug_implementations)]

use std::{
    collections::{HashMap, HashSet},
    hash::Hash,
    sync::Arc,
};

#[derive(Debug)]
pub enum State {
    S0,
    S1,
    Sx, // undefined value or conflict
    Sz, // high-impedance / not-connected
    Sa, // don't care (used only in cases)
    Sm, // marker (used internally by some passes)
}

#[derive(Debug)]
pub enum SyncType {
    ST0, // level sensitive: 0
    ST1, // level sensitive: 1
    STp, // edge sensitive: posedge
    STn, // edge sensitive: negedge
    STe, // edge sensitive: both edges
    STa, // always active
    STg, // global clock
    STi, // init
}

#[derive(Debug)]
pub enum ConstFlags {
    None,
    String,
    Signed, // only used for parameters
    Real,   // only used for parameters
}

#[derive(Debug)]
pub struct IDString(u32);

// TODO: IDStrings are stored in a global hashmap

#[derive(Debug)]
pub enum Const {
    Bits(Vec<State>),
    String(String),
}

// superclass of a bunch of things that have attributes
pub trait AttrObject {
    fn attributes(&self) -> &HashMap<IDString, Const>;
}

pub enum WireData {
    Wire(Wire),
    States(Vec<State>),
}

pub struct SigChunk {
    pub wire_data: WireData,
    pub offset: u32,
    pub width: u32,
}

pub enum WireDatum {
    Wire(Wire),
    State(State),
}

pub struct SigBit {
    pub wire_data: WireDatum,
    pub offset: u32,
}

type SigSig = (SigSpec, SigSpec);

pub enum SigSpec {
    Packed(Vec<SigChunk>),
    Unpacked(Vec<SigBit>),
}

#[derive(Debug)]
pub struct Selection {
    pub full_selection: bool,
    pub selected_modules: Vec<IDString>,
    pub selected_members: HashMap<IDString, Vec<IDString>>,
}

pub trait Monitor {
    // super class to contain callbacks for various monitors
    fn notify_module_add(&self, module: &Module) {}
    fn notify_module_del(&self, module: &Module) {}
    fn notify_connect_cell(&self, cell: &Cell, name: &IDString, from: &SigSpec, to: &SigSpec) {}
    fn notify_connect_wire(&self, module: &Module, wire: &SigSpec) {}
    fn notify_connect_many(&self, module: &Module, wires: &Vec<SigSpec>) {}
    fn notify_blackout(&self, module: &Module) {}
}

/// Represents an AST node linked to from verilog
#[derive(Debug)]
pub struct AstNode;

pub struct Design {
    pub monitors: Vec<Box<dyn Monitor>>,
    pub scratchpad: HashMap<String, String>,
    pub flag_buffered_normalized: bool,
    pub refcount_modules: u32,
    pub modules: HashMap<IDString, Module>,
    pub bindings: Vec<Binding>,
    pub verilog_packages: Vec<AstNode>,
    pub verilog_globals: Vec<AstNode>,
    pub verilog_defines: Vec<AstNode>,
    pub selection_stack: Vec<Selection>,
    pub selection_vars: HashMap<IDString, Selection>,
    pub selected_active_module: String,
}

pub struct Module {
    pub attributes: HashMap<IDString, Const>,
    pub design: Arc<Design>,
    pub monitors: Vec<Box<dyn Monitor>>,
    pub refcount_wires: u32,
    pub refcount_cells: u32,
    pub wires: HashMap<IDString, Wire>,
    pub cells: HashMap<IDString, Cell>,
    pub connections: Vec<SigSig>,
    pub bindings: Vec<Binding>,
    pub name: IDString,
    pub avail_parameters: HashSet<IDString>,
    pub parameter_default_values: HashMap<IDString, Const>,
    pub memories: HashMap<IDString, Memory>,
    pub processes: HashMap<IDString, Process>,
    pub ports: Vec<IDString>,
    pub buf_norm_queue: Vec<(Cell, IDString)>,
}

impl AttrObject for Module {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct Wire {
    pub attributes: HashMap<IDString, Const>,
    pub driver_cell: Option<Arc<Cell>>,
    pub driver_port: IDString,
    pub module: Arc<Module>,
    pub name: IDString,
    pub width: u32,
    pub start_offset: u32,
    pub port_id: u32,
    pub port_input: bool,
    pub port_output: bool,
    pub upto: bool,
    pub is_signed: bool,
}

impl AttrObject for Wire {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

#[derive(Debug)]
pub struct Memory {
    pub attributes: HashMap<IDString, Const>,
    pub name: IDString,
    pub width: u32,
    pub start_offset: u32,
    pub size: u32,
}

impl AttrObject for Memory {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct Cell {
    pub attributes: HashMap<IDString, Const>,
    pub module: Arc<Module>,
    pub name: IDString,
    pub type_: IDString,
    pub connections: HashMap<IDString, SigSpec>,
    pub parameters: HashMap<IDString, Const>,
}

impl AttrObject for Cell {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct CaseRule {
    pub attributes: HashMap<IDString, Const>,
    pub compare: Vec<SigSpec>,
    pub actions: Vec<SigSig>,
    pub switches: Vec<Arc<SwitchRule>>,
}

impl AttrObject for CaseRule {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct SwitchRule {
    pub attributes: HashMap<IDString, Const>,
    pub signal: SigSpec,
    pub cases: Vec<CaseRule>,
}

impl AttrObject for SwitchRule {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct MemWriteAction {
    pub attributes: HashMap<IDString, Const>,
    pub memid: IDString,
    pub address: SigSpec,
    pub data: SigSpec,
    pub enable: SigSpec,
    pub priority_mask: Const,
}

impl AttrObject for MemWriteAction {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct SyncRule {
    pub attributes: HashMap<IDString, Const>,

    pub type_: SyncType,
    pub signal: SigSpec,
    pub actions: Vec<SigSig>,
    pub mem_write_actions: Vec<MemWriteAction>,
}

impl AttrObject for SyncRule {
    fn attributes(&self) -> &HashMap<IDString, Const> {
        &self.attributes
    }
}

pub struct Process {
    pub attributes: HashMap<IDString, Const>,
    pub name: IDString,
    pub module: Arc<Module>,
    pub root_case: CaseRule,
    pub syncs: Vec<Arc<SyncRule>>,
}

// Represents a bind construct.
//
// The target of the binding is represented by target_type and
// target_name (see comments above the fields).
#[derive(Debug)]
pub struct Binding {
    // May be empty. If not, it's the name of the module or interface to
    // bind to.
    pub target_type: IDString,

    // If target_type is nonempty (the usual case), this is a hierarchical
    // reference to the bind target. If target_type is empty, we have to
    // wait until the hierarchy pass to figure out whether this was the name
    // of a module/interface type or an instance.
    pub target_name: IDString,

    // An attribute name which contains an ID that's unique across binding
    // instances (used to ensure we don't apply a binding twice to a module)
    pub attr_name: IDString,
}
