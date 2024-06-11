/*
   Copyright 2024 Ryan "rj45" Sanche

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef NEWSTRUCTS_H
#define NEWSTRUCTS_H

#include "core/core.h"

typedef ID SymbolID;
typedef ID PortID;
typedef uint32_t StringHandle;

#define TABLE_HEADER                                                           \
  size_t length;                                                               \
  size_t capacity;                                                             \
  ID *id;

typedef struct Table {
  TABLE_HEADER
  void *component[];
} Table;

// #define id_index(id) ((ID)(id)&0x00FFFFFF)
// #define id_gen(id) (((ID)(id) >> 24) & 0xFF)
// #define id_make(index, gen) (((ID)(gen) << 24) | (ID)(index)&0x00FFFFFF)

#define tagtype_tag(tag) ((tag) & 0xFF00)
#define tagtype_type(tag) ((tag) & 0x00FF)

//////////////////////////////////////////
// ECS Tags
//////////////////////////////////////////

typedef enum EntityType {
  TYPE_PORT,
  TYPE_SYMBOL_KIND,
  TYPE_SYMBOL,
  TYPE_WAYPOINT,
  TYPE_ENDPOINT,
  TYPE_SUBNET_BIT,
  TYPE_SUBNET_BITS,
  TYPE_SUBNET,
  TYPE_NET,
  TYPE_NETLIST,
  TYPE_MODULE,
  TYPE_COUNT,
} EntityType;

#define circ_entity_type(type)                                                 \
  _Generic(                                                                    \
    type,                                                                      \
    Port2: TYPE_PORT,                                                          \
    SymbolKind2: TYPE_SYMBOL_KIND,                                             \
    Symbol2: TYPE_SYMBOL,                                                      \
    Waypoint2: TYPE_WAYPOINT,                                                  \
    Endpoint2: TYPE_ENDPOINT,                                                  \
    SubnetBit2: TYPE_SUBNET_BIT,                                               \
    SubnetBits2: TYPE_SUBNET_BITS,                                             \
    Subnet2: TYPE_SUBNET,                                                      \
    Net2: TYPE_NET,                                                            \
    Netlist2: TYPE_NETLIST,                                                    \
    Module2: TYPE_MODULE,                                                      \
    default: 0)

typedef enum Tag {
  // first few bits reserved for EntityType

  TAG_IN = 1 << 8,
  TAG_OUT = 1 << 9,

  TAG_HOVERED = 1 << 10,
  TAG_SELECTED = 1 << 11,
  TAG_DRAGGING = 1 << 12,
  TAG_DEBUG = 1 << 15,
} Tag;

//////////////////////////////////////////
// ECS components
//////////////////////////////////////////

typedef ID Parent;

typedef ID ModuleID;
typedef ID SubnetBitsID;
typedef ID NetlistID;

typedef HMM_Vec2 Position;
typedef HMM_Vec2 Size;
typedef StringHandle Name;
typedef StringHandle Prefix;
typedef int32_t Number;

typedef struct ListNode {
  ID next;
  ID prev;
} ListNode;

typedef struct LinkedList {
  ID head;
  ID tail;
} LinkedList;

typedef struct PortRef {
  SymbolID symbol;
  PortID port;
} PortRef;

typedef struct WireVertices {
  uint16_t *wireVertexCounts;
  size_t wireCount;
  HMM_Vec2 *vertices;
} WireVertices;

typedef enum ComponentID2 {
  COMPONENT_PARENT,
  COMPONENT_MODULE_ID,
  COMPONENT_SUBNET_BITS_ID,
  COMPONENT_NETLIST_ID,
  COMPONENT_POSITON,
  COMPONENT_SIZE,
  COMPONENT_NAME,
  COMPONENT_PREFIX,
  COMPONENT_NUMBER,
  COMPONENT_LIST_NODE,
  COMPONENT_LINKED_LIST,
  COMPONENT_PORT_REF,
  COMPONENT_WIRE_VERTICES,
  COMPONENT_COUNT,
} ComponentID2;

#define circ_component_id(type)                                                \
  _Generic(                                                                    \
    type,                                                                      \
    Parent: COMPONENT_PARENT,                                                  \
    ModuleID: COMPONENT_MODULE_ID,                                             \
    SubnetBitsID: COMPONENT_SUBNET_BITS_ID,                                    \
    NetlistID: COMPONENT_NETLIST_ID,                                           \
    Position: COMPONENT_POSITON,                                               \
    Size: COMPONENT_SIZE,                                                      \
    Name: COMPONENT_NAME,                                                      \
    Prefix: COMPONENT_PREFIX,                                                  \
    Number: COMPONENT_NUMBER,                                                  \
    ListNode: COMPONENT_LIST_NODE,                                             \
    LinkedList: COMPONENT_LINKED_LIST,                                         \
    PortRef: COMPONENT_PORT_REF,                                               \
    WireVertices: COMPONENT_WIRE_VERTICES,                                     \
    default: 0)

// this is here to make it easier to keep in sync with the above, but it ends up
// being in the componentSizes variable below
#define COMPONENT_SIZES_LIST                                                   \
  [COMPONENT_PARENT] = sizeof(Parent),                                         \
  [COMPONENT_MODULE_ID] = sizeof(ModuleID),                                    \
  [COMPONENT_SUBNET_BITS_ID] = sizeof(SubnetBitsID),                           \
  [COMPONENT_NETLIST_ID] = sizeof(NetlistID),                                  \
  [COMPONENT_POSITON] = sizeof(Position), [COMPONENT_SIZE] = sizeof(Size),     \
  [COMPONENT_NAME] = sizeof(Name), [COMPONENT_PREFIX] = sizeof(Prefix),        \
  [COMPONENT_NUMBER] = sizeof(Number),                                         \
  [COMPONENT_LIST_NODE] = sizeof(ListNode),                                    \
  [COMPONENT_LINKED_LIST] = sizeof(LinkedList),                                \
  [COMPONENT_PORT_REF] = sizeof(PortRef),                                      \
  [COMPONENT_WIRE_VERTICES] = sizeof(WireVertices)

extern const size_t componentSizes[COMPONENT_COUNT];

//////////////////////////////////////////
// tables / entities
//////////////////////////////////////////

typedef struct Port2 {
  TABLE_HEADER
  Parent *symbolKind;
  Position *position;
  Name *name;
  Number *pin;
  ListNode *list;
} Port2;
#define PORT_COMPONENTS                                                        \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_NAME |       \
    1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

typedef struct SymbolKind2 {
  TABLE_HEADER
  ModuleID *subcircuit;
  Size *size;
  Name *name;
  Prefix *prefix;
  LinkedList *ports;
} SymbolKind2;
#define SYMBOL_KIND_COMPONENTS                                                 \
  1 << COMPONENT_MODULE_ID | 1 << COMPONENT_SIZE | 1 << COMPONENT_NAME |       \
    1 << COMPONENT_PREFIX | 1 << COMPONENT_LINKED_LIST

typedef struct Symbol2 {
  TABLE_HEADER
  Parent *symbolKind;
  Position *position;
  Number *number; // combined with prefix to get the name
} Symbol2;
#define SYMBOL_COMPONENTS                                                      \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_NUMBER

typedef struct Waypoint2 {
  TABLE_HEADER
  Parent *endpoint;
  Position *position;
  ListNode *list;
} Waypoint2;
#define WAYPOINT_COMPONENTS                                                    \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_LIST_NODE

typedef struct Endpoint2 {
  TABLE_HEADER
  Parent *subnet;
  Position *position;
  ListNode *list;
  PortRef *port;
} Endpoint2;
#define ENDPOINT_COMPONENTS                                                    \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_PORT_REF |   \
    1 << COMPONENT_LIST_NODE

typedef struct SubnetBit2 {
  TABLE_HEADER
  Number *bit;
  ListNode *list;
} SubnetBit2;
#define SUBNET_BIT_COMPONENT_COMPONENTS                                        \
  1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

typedef struct SubnetBits2 {
  TABLE_HEADER
  Parent *subnet;
  SubnetBitsID *override;
  LinkedList *bits;
} SubnetBits2;
#define SUBNET_BITS_COMPONENTS                                                 \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SUBNET_BITS_ID |                      \
    1 << COMPONENT_LINKED_LIST

typedef struct Subnet2 {
  TABLE_HEADER
  Parent *net;
  SubnetBitsID *bits;
  Name *name;
  ListNode *list;
  LinkedList *endpoints;
} Subnet2;
#define SUBNET_COMPONENTS                                                      \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SUBNET_BITS_ID |                      \
    1 << COMPONENT_NAME | 1 << COMPONENT_LIST_NODE |                           \
    1 << COMPONENT_LINKED_LIST

typedef struct Net2 {
  TABLE_HEADER
  Parent *Netlist;
  Name *name;
  ListNode *list;
  LinkedList *subnets;
  WireVertices *wires;
} Net2;
#define NET_COMPONENTS                                                         \
  1 << COMPONENT_PARENT | 1 << COMPONENT_NAME | 1 << COMPONENT_LIST_NODE |     \
    1 << COMPONENT_LINKED_LIST | 1 << COMPONENT_WIRE_VERTICES

typedef struct Netlist2 {
  TABLE_HEADER
  LinkedList *nets;
} Netlist2;
#define NETLIST_COMPONENTS 1 << COMPONENT_LINKED_LIST

typedef struct Module2 {
  TABLE_HEADER
  NetlistID *nets;
  ListNode *list;
  LinkedList *symbols;
} Module2;
#define MODULE_COMPONENTS                                                      \
  1 << COMPONENT_NETLIST_ID | 1 << COMPONENT_LIST_NODE |                       \
    1 << COMPONENT_LINKED_LIST

#define MAX_COMPONENT_COUNT 5

#define STANDARD_TABLE_LIST                                                    \
  [TYPE_PORT] = {.components = PORT_COMPONENTS},                               \
  [TYPE_SYMBOL_KIND] = {.components = SYMBOL_KIND_COMPONENTS},                 \
  [TYPE_SYMBOL] = {.components = SYMBOL_COMPONENTS},                           \
  [TYPE_WAYPOINT] = {.components = WAYPOINT_COMPONENTS},                       \
  [TYPE_ENDPOINT] = {.components = ENDPOINT_COMPONENTS},                       \
  [TYPE_SUBNET_BIT] = {.components = SUBNET_BIT_COMPONENT_COMPONENTS},         \
  [TYPE_SUBNET_BITS] = {.components = SUBNET_BITS_COMPONENTS},                 \
  [TYPE_SUBNET] = {.components = SUBNET_COMPONENTS},                           \
  [TYPE_NET] = {.components = NET_COMPONENTS},                                 \
  [TYPE_NETLIST] = {.components = NETLIST_COMPONENTS},                         \
  [TYPE_MODULE] = {.components = MODULE_COMPONENTS},

typedef struct TableMeta {
  uint32_t components;
  size_t componentCount;
  size_t componentSizes[MAX_COMPONENT_COUNT];
  int32_t componentIndices[COMPONENT_COUNT];
} TableMeta;

typedef struct Circuit2 {
  ModuleID top;

  // entities
  uint8_t *generations;
  uint16_t *typeTags; // EntityType | Tag
  uint32_t *rows;
  uint32_t numEntities;
  uint32_t capacity;
  arr(ID) freelist;

  // standard tables
  Port2 port;
  SymbolKind2 symbolKind;
  Symbol2 symbol;
  Waypoint2 waypoint;
  Endpoint2 endpoint;
  SubnetBit2 subnetBit;
  SubnetBits2 subnetBits;
  Subnet2 subnet;
  Net2 net;
  Netlist2 netlist;
  Module2 module;

  // indices
  Table **table;
  TableMeta *tableMeta;
  int numTables;

  // changelog & events
  ChangeLog log;

} Circuit2;

static inline bool circ_has(Circuit2 *circuit, ID id) {
  return id_index(id) < circuit->numEntities &&
         circuit->generations[id_index(id)] == id_gen(id);
}

static inline size_t circ_row(Circuit2 *circuit, ID id) {
  assert(circ_has(circuit, id));
  return circuit->rows[id_index(id)];
}

static inline ID circ_id(Circuit2 *circuit, EntityType type, size_t row) {
  assert(type < TYPE_COUNT && type >= 0);
  assert(row < circuit->table[type]->length);
  return circuit->table[type]->id[row];
}

// Get the component array pointer within a table.
// Component index 0 is after the ID list.
#define circ_table_components_ptr(circuit, type, componentIndex)               \
  ((circuit)->table[(type)]->component[componentIndex])

// Get a pointer to the component array pointer within a table.
#define circ_table_components_ptr_ptr(circuit, type, componentIndex)           \
  (void **)&(circ_table_components_ptr(circuit, type, componentIndex))

// Get a pointer to a specific component within a table.
#define circ_table_component_ptr(circuit, type, componentIndex, row)           \
  (void *)((char *)circ_table_components_ptr(circuit, type, componentIndex) +  \
           (circuit)->tableMeta[type].componentSizes[componentIndex] * (row))

#define circ_set(circuit, id, table, component, value)                         \
  ((circuit)->table.component[(circuit)->rows[id_index(id)]] = *(value))

void circ_init(Circuit2 *circ);
void circ_free(Circuit2 *circ);
void circ_add_entity_id(Circuit2 *circ, EntityType type, ID id);
ID circ_add_entity(Circuit2 *circ, EntityType type);

typedef struct CircuitIter {
  // private:
  Circuit2 *circuit;
  int index;
} CircuitIter;

static inline Table *circ_iter_next_(CircuitIter *iter, EntityType type) {
  // this is an abstraction to allow things like paged tables or skipping
  // deleted rows in the future. it could also be used to allow multi-table
  // iterations over common subset of components. for now this is a stub
  // implementation
  iter->index++;
  if (iter->index > 0) {
    return NULL;
  }
  return iter->circuit->table[type];
}

static inline Table *
circ_iter_(Circuit2 *circuit, CircuitIter *it, EntityType type) {
  *it = (CircuitIter){.circuit = circuit, .index = -1};
  return circ_iter_next_(it, type);
}

#define circ_iter(circuit, iter, type)                                         \
  (type *)circ_iter_(circuit, iter, circ_entity_type(type))

#define circ_iter_next(iter, type)                                             \
  (type *)circ_iter_next_(iter, circ_entity_type(type))

#endif // NEWSTRUCTS_H
