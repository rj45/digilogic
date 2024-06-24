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

#ifndef CORE_H
#define CORE_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "handmade_math.h"
#include "stb_ds.h"

#include "strpool.h"

// defines an STB array
#define arr(type) type *

// defines an STB hash map
#define hashmap(keyType, valueType)                                            \
  struct {                                                                     \
    keyType key;                                                               \
    valueType value;                                                           \
  } *

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define MUST_USE_RETURN __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define MUST_USE_RETURN _Check_return_
#else
#define MUST_USE_RETURN
#endif

#if defined(__GNUC__)
#define PACK(...) __VA_ARGS__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define PACK(...) __pragma(pack(push, 1)) __VA_ARGS__ __pragma(pack(pop))
#else
#define PACK(...) __VA_ARGS__
#endif

////////////////////////////////////////////////////////////////////////////////
// Generational Handle IDs
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t ID;

#define NO_ID 0

typedef uint32_t Gen;

#define smap(type) type *

#define ID_FLAG_BITS 2
#define ID_GEN_BITS 6
#define ID_INDEX_BITS (32 - ID_FLAG_BITS - ID_GEN_BITS)

#define ID_FLAG_MASK ((1 << ID_FLAG_BITS) - 1)
#define ID_GEN_MASK ((1 << ID_GEN_BITS) - 1)
#define ID_INDEX_MASK ((1 << ID_INDEX_BITS) - 1)

#define ID_FLAG_SHIFT (ID_GEN_BITS + ID_INDEX_BITS)
#define ID_GEN_SHIFT (ID_INDEX_BITS)

#define id_make(type, gen, index)                                              \
  ((((ID)(type) & ID_FLAG_MASK) << ID_FLAG_SHIFT) |                            \
   (((ID)(gen) & ID_GEN_MASK) << ID_GEN_SHIFT) |                               \
   ((ID)(index) & ID_INDEX_MASK))
#define id_flags(id) ((uint32_t)(((id) >> ID_FLAG_SHIFT) & ID_FLAG_MASK))
#define id_gen(id) ((Gen)(((id) >> ID_GEN_SHIFT) & ID_GEN_MASK))
#define id_flagsgen(id) ((ID)(id) >> ID_GEN_SHIFT)
#define id_index(id) ((ID)(id) & ID_INDEX_MASK)
#define id_valid(id) (id_gen(id) != 0)

////////////////////////////////////////////////////////////////////////////////
// Bounding Boxes
////////////////////////////////////////////////////////////////////////////////

typedef struct Box {
  HMM_Vec2 center;
  HMM_Vec2 halfSize;
} Box;

static inline HMM_Vec2 box_top_left(Box box) {
  return HMM_SubV2(box.center, box.halfSize);
}

static inline HMM_Vec2 box_bottom_right(Box box) {
  return HMM_AddV2(box.center, box.halfSize);
}

static inline HMM_Vec2 box_size(Box box) { return HMM_MulV2F(box.halfSize, 2); }

static inline Box box_translate(Box box, HMM_Vec2 offset) {
  return (
    (Box){.center = HMM_AddV2(box.center, offset), .halfSize = box.halfSize});
}

static inline bool box_intersect_box(Box a, Box b) {
  HMM_Vec2 delta = HMM_SubV2(a.center, b.center);
  float ex = HMM_ABS(delta.X) - (a.halfSize.X + b.halfSize.X);
  float ey = HMM_ABS(delta.Y) - (a.halfSize.Y + b.halfSize.Y);
  return ex < 0 && ey < 0;
}

static inline bool box_intersect_point(Box a, HMM_Vec2 b) {
  HMM_Vec2 delta = HMM_SubV2(a.center, b);
  float ex = HMM_ABS(delta.X) - a.halfSize.X;
  float ey = HMM_ABS(delta.Y) - a.halfSize.Y;
  return ex < 0 && ey < 0;
}

static inline Box box_from_tlbr(HMM_Vec2 tl, HMM_Vec2 br) {
  if (tl.X > br.X) {
    float tmp = tl.X;
    tl.X = br.X;
    br.X = tmp;
  }
  if (tl.Y > br.Y) {
    float tmp = tl.Y;
    tl.Y = br.Y;
    br.Y = tmp;
  }
  return ((Box){
    .center = HMM_LerpV2(tl, 0.5f, br),
    .halfSize = HMM_MulV2F(HMM_SubV2(br, tl), 0.5f)});
}

static inline Box box_union(Box a, Box b) {
  HMM_Vec2 tl = (HMM_Vec2){
    .X = HMM_MIN(a.center.X - a.halfSize.X, b.center.X - b.halfSize.X),
    .Y = HMM_MIN(a.center.Y - a.halfSize.Y, b.center.Y - b.halfSize.Y),
  };
  HMM_Vec2 br = (HMM_Vec2){
    .X = HMM_MAX(a.center.X + a.halfSize.X, b.center.X + b.halfSize.X),
    .Y = HMM_MAX(a.center.Y + a.halfSize.Y, b.center.Y + b.halfSize.Y),
  };
  return box_from_tlbr(tl, br);
}

static inline bool box_equal(Box a, Box b) {
  return HMM_EqV2(a.center, b.center) && HMM_EqV2(a.halfSize, b.halfSize);
}

////////////////////////////////////////////////////////////////////////////////
// Bounding Volume Hierarchy
////////////////////////////////////////////////////////////////////////////////

typedef struct BVHNode {
  Box box;
  uint32_t firstLeaf;
  uint32_t numLeaves;

  // for debugging
  float median;
  int axis;
} BVHNode;

typedef struct BVHLeaf {
  Box box;
  ID item;
  ID subitem;
} BVHLeaf;

typedef struct BVH {
  arr(BVHNode) nodeHeap;

  arr(BVHLeaf) leaves;

  arr(uint32_t) stack;
  arr(uint32_t) scratch;

  bool needsRebuild;
} BVH;

void bvh_init(BVH *bvh);
void bvh_free(BVH *bvh);
void bvh_clear(BVH *bvh);
void bvh_add(BVH *bvh, ID item, ID subitem, Box box);
void bvh_remove(BVH *bvh, ID item, ID subitem, Box box);
void bvh_update(BVH *bvh, ID item, ID subitem, Box oldBox, Box newBox);
void bvh_rebuild(BVH *bvh);
arr(BVHLeaf) bvh_query(BVH *bvh, Box box, arr(BVHLeaf) result);

////////////////////////////////////////////////////////////////////////////////
// ChangeLog
////////////////////////////////////////////////////////////////////////////////

#define MAX_COMPONENT_SIZE 10

typedef struct LogUpdate {
  uint32_t row;
  uint8_t column;
  uint8_t size;
  uint8_t newValue[MAX_COMPONENT_SIZE];
} LogUpdate;

typedef struct LogEntry {
  PACK(enum {
    LOG_CREATE,
    LOG_DELETE,
    LOG_UPDATE,
  })
  verb;
  uint16_t table;
  ID id;
  uint32_t dataIndex;
} LogEntry;

typedef struct ChangeLog {
  arr(LogEntry) log;
  arr(LogUpdate) updates;
  arr(size_t) commitPoints;
  size_t redoIndex;

  void *user;
  void (*cl_revert_snapshot)(void *user);
  void (*cl_replay_create)(void *user, ID id);
  void (*cl_replay_delete)(void *user, ID id);
  void (*cl_replay_update)(
    void *user, ID id, uint16_t table, uint16_t column, uint32_t row,
    void *data, size_t size);
} ChangeLog;

void cl_init(ChangeLog *log);
void cl_free(ChangeLog *log);

void cl_commit(ChangeLog *log);
void cl_create(ChangeLog *log, ID id, uint16_t table);
void cl_delete(ChangeLog *log, ID id);
void cl_update(
  ChangeLog *log, ID id, uint16_t table, uint16_t column, uint32_t row,
  void *newValue, size_t size);

void cl_undo(ChangeLog *log);
void cl_redo(ChangeLog *log);

////////////////////////////////////////////////////////////////////////////////
// Circuit
////////////////////////////////////////////////////////////////////////////////

// default ComponentDescIDs for the built-in components
enum {
  COMP_NONE,
  COMP_AND,
  COMP_OR,
  COMP_XOR,
  COMP_NOT,
  COMP_INPUT,
  COMP_OUTPUT,
  COMP_COUNT,
};

typedef enum PortDirection {
  PORT_IN,
  PORT_OUT,
  PORT_INOUT,
} PortDirection;

typedef struct PortDesc {
  PortDirection direction;
  int number;
  const char *name;
} PortDesc;

typedef enum ShapeType {
  SHAPE_DEFAULT,
  SHAPE_AND,
  SHAPE_OR,
  SHAPE_XOR,
  SHAPE_NOT,
} ShapeType;

typedef struct ComponentDesc {
  const char *typeName;
  int numPorts;
  char namePrefix;
  ShapeType shape;
  PortDesc *ports;
} ComponentDesc;

const ComponentDesc *circuit_component_descs();

typedef struct Wire {
  uint16_t vertexCount;
} Wire;

////////////////////////////////////////////////////////////////////////////////
// Circuit ECS
////////////////////////////////////////////////////////////////////////////////

typedef ID SymbolID;
typedef ID PortID;
typedef uint32_t StringHandle;

#define TABLE_HEADER                                                           \
  size_t length;                                                               \
  size_t capacity;                                                             \
  ID *id;

#define tagtype_tag(tag) ((tag) & 0xFF00)
#define tagtype_type(tag) ((tag) & 0x00FF)

typedef struct SymbolLayout {
  float portSpacing;
  float symbolWidth;
  float borderWidth;
  float labelPadding;
  void *user;
  HMM_Vec2 (*textSize)(void *user, const char *text);
} SymbolLayout;

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

#define ENTITY_TYPE_Port TYPE_PORT
#define ENTITY_TYPE_SymbolKind TYPE_SYMBOL_KIND
#define ENTITY_TYPE_Symbol TYPE_SYMBOL
#define ENTITY_TYPE_Waypoint TYPE_WAYPOINT
#define ENTITY_TYPE_Endpoint TYPE_ENDPOINT
#define ENTITY_TYPE_SubnetBit TYPE_SUBNET_BIT
#define ENTITY_TYPE_SubnetBits TYPE_SUBNET_BITS
#define ENTITY_TYPE_Subnet TYPE_SUBNET
#define ENTITY_TYPE_Net TYPE_NET
#define ENTITY_TYPE_Netlist TYPE_NETLIST
#define ENTITY_TYPE_Module TYPE_MODULE

#define circ_entity_type(type) ENTITY_TYPE_##type

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

typedef ID SymbolKindID;
typedef ID ModuleID;
typedef ID SubnetBitsID;
typedef ID NetlistID;

typedef HMM_Vec2 Position;
typedef HMM_Vec2 Size;
typedef StringHandle Name;
typedef StringHandle Prefix;
typedef int32_t Number;

PACK(typedef enum SymbolShape{
  SYMSHAPE_DEFAULT,
  SYMSHAPE_AND,
  SYMSHAPE_OR,
  SYMSHAPE_XOR,
  SYMSHAPE_NOT,
})
SymbolShape;

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

PACK(typedef enum ComponentID{
  COMPONENT_PARENT,
  COMPONENT_SYMBOL_KIND_ID,
  COMPONENT_MODULE_ID,
  COMPONENT_SUBNET_BITS_ID,
  COMPONENT_NETLIST_ID,
  COMPONENT_POSITON,
  COMPONENT_SIZE,
  COMPONENT_NAME,
  COMPONENT_PREFIX,
  COMPONENT_NUMBER,
  COMPONENT_SYMBOL_SHAPE,
  COMPONENT_LIST_NODE,
  COMPONENT_LINKED_LIST,
  COMPONENT_PORT_REF,
  COMPONENT_WIRE_VERTICES,
  COMPONENT_COUNT,
})
ComponentID;

#define COMPONENT_ID_Parent COMPONENT_PARENT
#define COMPONENT_ID_SymbolKindID COMPONENT_SYMBOL_KIND_ID
#define COMPONENT_ID_ModuleID COMPONENT_MODULE_ID
#define COMPONENT_ID_SubnetBitsID COMPONENT_SUBNET_BITS_ID
#define COMPONENT_ID_NetlistID COMPONENT_NETLIST_ID
#define COMPONENT_ID_Position COMPONENT_POSITON
#define COMPONENT_ID_Size COMPONENT_SIZE
#define COMPONENT_ID_Name COMPONENT_NAME
#define COMPONENT_ID_Prefix COMPONENT_PREFIX
#define COMPONENT_ID_Number COMPONENT_NUMBER
#define COMPONENT_ID_SymbolShape COMPONENT_SYMBOL_SHAPE
#define COMPONENT_ID_ListNode COMPONENT_LIST_NODE
#define COMPONENT_ID_LinkedList COMPONENT_LINKED_LIST
#define COMPONENT_ID_PortRef COMPONENT_PORT_REF
#define COMPONENT_ID_WireVertices COMPONENT_WIRE_VERTICES

#define circ_component_id(type) COMPONENT_ID_##type

// this is here to make it easier to keep in sync with the above, but it ends up
// being in the componentSizes variable below
#define COMPONENT_SIZES_LIST                                                   \
  [COMPONENT_PARENT] = sizeof(Parent),                                         \
  [COMPONENT_SYMBOL_KIND_ID] = sizeof(SymbolKindID),                           \
  [COMPONENT_MODULE_ID] = sizeof(ModuleID),                                    \
  [COMPONENT_SUBNET_BITS_ID] = sizeof(SubnetBitsID),                           \
  [COMPONENT_NETLIST_ID] = sizeof(NetlistID),                                  \
  [COMPONENT_POSITON] = sizeof(Position), [COMPONENT_SIZE] = sizeof(Size),     \
  [COMPONENT_NAME] = sizeof(Name), [COMPONENT_PREFIX] = sizeof(Prefix),        \
  [COMPONENT_NUMBER] = sizeof(Number),                                         \
  [COMPONENT_SYMBOL_SHAPE] = sizeof(SymbolShape),                              \
  [COMPONENT_LIST_NODE] = sizeof(ListNode),                                    \
  [COMPONENT_LINKED_LIST] = sizeof(LinkedList),                                \
  [COMPONENT_PORT_REF] = sizeof(PortRef),                                      \
  [COMPONENT_WIRE_VERTICES] = sizeof(WireVertices)

extern const size_t componentSizes[COMPONENT_COUNT];

//////////////////////////////////////////
// tables
//////////////////////////////////////////

typedef struct Port {
  TABLE_HEADER
  Parent *symbolKind;
  Position *position;
  Name *name;
  Number *pin;
  ListNode *list;
} Port;
#define PORT_COMPONENTS                                                        \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_NAME |       \
    1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

typedef struct SymbolKind {
  TABLE_HEADER
  ModuleID *module;
  Size *size;
  Name *name;
  Prefix *prefix;
  SymbolShape *shape;
  LinkedList *ports;
} SymbolKind;
#define SYMBOL_KIND_COMPONENTS                                                 \
  1 << COMPONENT_MODULE_ID | 1 << COMPONENT_SIZE | 1 << COMPONENT_NAME |       \
    1 << COMPONENT_PREFIX | 1 << COMPONENT_SYMBOL_SHAPE |                      \
    1 << COMPONENT_LINKED_LIST

typedef struct Symbol {
  TABLE_HEADER
  Parent *module;
  SymbolKindID *symbolKind;
  Position *position;
  Number *number; // combined with prefix to get the name
  ListNode *list;
} Symbol;
#define SYMBOL_COMPONENTS                                                      \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SYMBOL_KIND_ID |                      \
    1 << COMPONENT_POSITON | 1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

typedef struct Waypoint {
  TABLE_HEADER
  Parent *endpoint;
  Position *position;
  ListNode *list;
} Waypoint;
#define WAYPOINT_COMPONENTS                                                    \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_LIST_NODE

typedef struct Endpoint {
  TABLE_HEADER
  Parent *subnet;
  Position *position;
  ListNode *list;
  LinkedList *waypoints;
  PortRef *port;
} Endpoint;
#define ENDPOINT_COMPONENTS                                                    \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_LIST_NODE |  \
    1 << COMPONENT_LINKED_LIST | 1 << COMPONENT_PORT_REF

typedef struct SubnetBit {
  TABLE_HEADER
  Number *bit;
  ListNode *list;
} SubnetBit;
#define SUBNET_BIT_COMPONENT_COMPONENTS                                        \
  1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

typedef struct SubnetBits {
  TABLE_HEADER
  Parent *subnet;
  SubnetBitsID *override;
  LinkedList *bits;
} SubnetBits;
#define SUBNET_BITS_COMPONENTS                                                 \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SUBNET_BITS_ID |                      \
    1 << COMPONENT_LINKED_LIST

typedef struct Subnet {
  TABLE_HEADER
  Parent *net;
  SubnetBitsID *bits;
  Name *name;
  ListNode *list;
  LinkedList *endpoints;
} Subnet;
#define SUBNET_COMPONENTS                                                      \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SUBNET_BITS_ID |                      \
    1 << COMPONENT_NAME | 1 << COMPONENT_LIST_NODE |                           \
    1 << COMPONENT_LINKED_LIST

typedef struct Net {
  TABLE_HEADER
  Parent *netlist;
  Name *name;
  ListNode *list;
  LinkedList *subnets;
  WireVertices *wires;
} Net;
#define NET_COMPONENTS                                                         \
  1 << COMPONENT_PARENT | 1 << COMPONENT_NAME | 1 << COMPONENT_LIST_NODE |     \
    1 << COMPONENT_LINKED_LIST | 1 << COMPONENT_WIRE_VERTICES

typedef struct Netlist {
  TABLE_HEADER
  Parent *module;
  LinkedList *nets;
} Netlist;
#define NETLIST_COMPONENTS 1 << COMPONENT_PARENT | 1 << COMPONENT_LINKED_LIST

typedef struct Module {
  TABLE_HEADER
  SymbolKindID *symbolKind;
  NetlistID *nets;
  ListNode *list;
  LinkedList *symbols;
} Module;
#define MODULE_COMPONENTS                                                      \
  1 << COMPONENT_SYMBOL_KIND_ID | 1 << COMPONENT_NETLIST_ID |                  \
    1 << COMPONENT_LIST_NODE | 1 << COMPONENT_LINKED_LIST

#define MAX_COMPONENT_COUNT 6

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

#define MAX_INVERTED_INDEX_COUNT 2

// where in the rows list to find the "children" of the current row
typedef struct InvertedIndexEntry {
  uint32_t firstRow;
  uint32_t count;
} InvertedIndexEntry;

// InvertedIndex is a list of all entities that reference a specific entity via
// a specific component on that entity. For example, this can be used to quickly
// enumerate all Endpoints that a Subnet has via the Endpoint's Parent
// component.
typedef struct InvertedIndex {
  InvertedIndexEntry *entries;
  arr(uint32_t) rows;
  EntityType foreignTable;
  ComponentID foreignKey;
} InvertedIndex;

typedef struct Table {
  TABLE_HEADER
  void *component[];
} Table;

// TableMeta is the metadata for a table for quick access to components.
typedef struct TableMeta {

  // TODO: convert to SoA

  // number of components in the table
  size_t componentCount;

  // size of each component in the table
  size_t componentSize[MAX_COMPONENT_COUNT];

  // flags for each component, so 1 << COMPONENT_ID for each component
  uint32_t components;

  // ComponentID to column index mapping
  int32_t componentColumn[COMPONENT_COUNT];

  // InvertedIndices for this table
  int32_t invertedIndexCount;
  InvertedIndex invertedIndex[MAX_INVERTED_INDEX_COUNT];
} TableMeta;

typedef struct Circuit {
  ModuleID top;

  // entities
  uint8_t *generations;
  uint16_t *typeTags; // EntityType | Tag
  uint32_t *rows;
  uint32_t numEntities;
  uint32_t capacity;
  arr(ID) freelist;

  // standard tables
  Port port;
  SymbolKind symbolKind;
  Symbol symbol;
  Waypoint waypoint;
  Endpoint endpoint;
  SubnetBit subnetBit;
  SubnetBits subnetBits;
  Subnet subnet;
  Net net;
  Netlist netlist;
  Module module;

  // indices
  Table *table[TYPE_COUNT];
  TableMeta tableMeta[TYPE_COUNT];

  // TODO: strpool is not a good fit because it can't be easily cloned, which
  // is important for snapshots. Need to probably make a custom string pool.
  // It would also be nice to use entity IDs for strings, so the string pool
  // could be a table like any other.
  strpool_t strpool;
  bool foreignStrpool; // is strpool owned by another Circuit?

  // changelog & events
  ChangeLog log;
} Circuit;

static inline bool circ_has(Circuit *circuit, ID id) {
  return id_index(id) < circuit->capacity &&
         circuit->generations[id_index(id)] == id_gen(id);
}

static inline size_t circ_row(Circuit *circuit, ID id) {
  assert(circ_has(circuit, id));
  return circuit->rows[id_index(id)];
}

static inline ID circ_id(Circuit *circuit, EntityType type, size_t row) {
  assert(type < TYPE_COUNT && type >= 0);
  assert(row < circuit->table[type]->length);
  return circuit->table[type]->id[row];
}

static inline size_t circ_table_len(Circuit *circuit, EntityType type) {
  return circuit->table[type]->length;
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
           (circuit)->tableMeta[type].componentSize[componentIndex] * (row))

#define circ_type_for_id(circuit, id)                                          \
  ((EntityType)tagtype_type((circuit)->typeTags[id_index(id)]))

#define circ_row_for_id(circuit, id) ((EntityType)(circuit)->rows[id_index(id)])

#define circ_table_for_id(circuit, id)                                         \
  ((circuit)->table[circ_type_for_id(circuit, id)])

#define circ_table_meta_for_id(circuit, id)                                    \
  ((circuit)->tableMeta[circ_type_for_id(circuit, id)])

#define circ_set_ptr(circuit, id, componentType, ptr)                          \
  circ_set_(                                                                   \
    (circuit), circ_type_for_id((circuit), (id)),                              \
    circ_row_for_id((circuit), (id)),                                          \
    circ_table_meta_for_id(circuit, id)                                        \
      .componentColumn[circ_component_id(componentType)],                      \
    ptr)

#define circ_set(circuit, id, componentType, ...)                              \
  circ_set_ptr(circuit, id, componentType, &(componentType)__VA_ARGS__)

#define circ_get_ptr(circuit, id, componentType)                               \
  (assert(circ_has(circuit, id)),                                              \
   (const componentType *)((componentType *)circ_table_component_ptr(          \
     circuit, circ_type_for_id(circuit, id),                                   \
     circ_table_meta_for_id(circuit, id)                                       \
       .componentColumn[circ_component_id(componentType)],                     \
     circ_row_for_id(circuit, id))))

#define circ_get(circuit, id, componentType)                                   \
  (*circ_get_ptr(circuit, id, componentType))

#define circ_add(circuit, tableType)                                           \
  circ_add_type(circuit, circ_entity_type(tableType))
#define circ_add_id(circuit, tableType, id)                                    \
  circ_add_type_id(circuit, circ_entity_type(tableType), id)

#define circ_has_component(circuit, id, componentType)                         \
  (circ_table_meta_for_id(circuit, id).components &                            \
   (1 << circ_component_id(componentType)))

#define circ_len(circuit, type) circ_table_len(circuit, circ_entity_type(type))

void circ_init(Circuit *circ);
void circ_free(Circuit *circ);
void circ_clone(Circuit *dst, Circuit *src);
void circ_load_symbol_descs(
  Circuit *circ, SymbolLayout *layout, const ComponentDesc *descs,
  size_t count);
void circ_add_type_id(Circuit *circ, EntityType type, ID id);
ID circ_add_type(Circuit *circ, EntityType type);
void circ_remove(Circuit *circ, ID id);
void circ_add_tags(Circuit *circ, ID id, Tag tags);
bool circ_has_tags(Circuit *circ, ID id, Tag tags);

StringHandle circ_str(Circuit *circ, const char *str, size_t len);
StringHandle circ_str_c(Circuit *circ, const char *str);
StringHandle circ_str_tmp(Circuit *circ, const char *str, size_t len);
StringHandle circ_str_tmp_c(Circuit *circ, const char *str);
void circ_str_free(Circuit *circ, StringHandle handle);
const char *circ_str_get(Circuit *circ, StringHandle handle);

void circ_linked_list_append(Circuit *circ, ID parent, ID child);
void circ_linked_list_remove(Circuit *circ, ID parent, ID child);

static inline void
circ_set_(Circuit *circuit, int table, int row, int column, void *value) {
  memcpy(
    circ_table_component_ptr(circuit, table, column, row), value,
    circuit->tableMeta[table].componentSize[column]);
}

//////////////////////////////////////////
// Iterator
//////////////////////////////////////////

// this is an abstraction to allow things like paged tables or skipping
// deleted rows in the future. it could also be used to allow multi-table
// iterations over common subset of components. for now this is a stub
// implementation
typedef struct CircuitIter {
  // private:
  Circuit *circuit;
  EntityType type;
  int index;
} CircuitIter;

static inline CircuitIter circ_iter_(Circuit *circuit, EntityType type) {
  return (CircuitIter){.circuit = circuit, .type = type, .index = -1};
}

static inline bool circ_iter_next(CircuitIter *iter) {
  iter->index++;
  return iter->index == 0;
}

static inline Table *circ_iter_table_(CircuitIter *iter, EntityType type) {
  assert(iter->type == type); // make sure the type matches the iterator
  if (iter->index != 0) {
    return NULL;
  }
  return iter->circuit->table[iter->type];
}

static inline void circ_iter_set_(
  CircuitIter *it, int index, ComponentID componentID, void *value) {
  int column = it->circuit->tableMeta[it->type].componentColumn[componentID];
  circ_set_(it->circuit, it->type, index, column, value);
}

#define circ_iter(circuit, type) circ_iter_(circuit, circ_entity_type(type))
#define circ_iter_table(iter, type)                                            \
  (type *)circ_iter_table_(iter, circ_entity_type(type))
#define circ_iter_set_ptr(iter, index, componentType, ptr)                     \
  circ_iter_set_(iter, index, circ_component_id(componentType), ptr)
#define circ_iter_set(iter, index, componentType, ...)                         \
  circ_iter_set_ptr(iter, index, componentType, &(componentType)__VA_ARGS__)

typedef struct LinkedListIter {
  Circuit *circ;
  ID current;
  ID next;
} LinkedListIter;

static inline LinkedListIter circ_lliter(Circuit *circ, ID parent) {
  LinkedList list = circ_get(circ, parent, LinkedList);
  return (LinkedListIter){circ, .next = list.head};
}

static inline bool circ_lliter_next(LinkedListIter *iter) {
  if (!circ_has(iter->circ, iter->next)) {
    return false;
  }
  ListNode node = circ_get(iter->circ, iter->next, ListNode);
  iter->current = iter->next;
  iter->next = node.next;
  return true;
}

static inline ID circ_lliter_get(LinkedListIter *iter) { return iter->current; }

////////////////////////////////////////////////////////////////////////////////
// Higher level functions
////////////////////////////////////////////////////////////////////////////////

void circ_clear(Circuit *circ);

ID circ_add_port(Circuit *circ, ID symbolKind);
void circ_remove_port(Circuit *circ, ID id);
HMM_Vec2 circ_port_position(Circuit *circ, PortRef portRef);

ID circ_add_symbol_kind(Circuit *circ);
void circ_remove_symbol_kind(Circuit *circ, ID id);
ID circ_get_symbol_kind_by_name(Circuit *circuit, const char *name);

ID circ_add_symbol(Circuit *circ, ID module, ID symbolKind);
void circ_remove_symbol(Circuit *circ, ID id);
void circ_set_symbol_position(Circuit *circ, ID id, HMM_Vec2 position);
Box circ_get_symbol_box(Circuit *circ, ID id);

ID circ_add_waypoint(Circuit *circ, ID endpoint);
void circ_remove_waypoint(Circuit *circ, ID id);
void circ_set_waypoint_position(Circuit *circ, ID id, HMM_Vec2 position);

ID circ_add_endpoint(Circuit *circ, ID subnet);
void circ_remove_endpoint(Circuit *circ, ID id);
void circ_set_endpoint_position(Circuit *circ, ID id, HMM_Vec2 position);
void circ_connect_endpoint_to_port(
  Circuit *circ, ID endpointID, ID symbolID, ID portID);
void circ_disconnect_endpoint_from_port(Circuit *circ, ID endpointID);

ID circ_add_subnet_bit(Circuit *circ, ID subnetBits);
void circ_remove_subnet_bit(Circuit *circ, ID id);

ID circ_add_subnet_bits(Circuit *circ, ID subnet);
void circ_remove_subnet_bits(Circuit *circ, ID id);

ID circ_add_subnet(Circuit *circ, ID net);
void circ_remove_subnet(Circuit *circ, ID id);

ID circ_add_net(Circuit *circ, ID module);
void circ_remove_net(Circuit *circ, ID id);
void circuit_set_net_wire_vertices(
  Circuit *circ, ID netID, WireVertices wireVerts);

ID circ_add_module(Circuit *circ);
void circ_remove_module(Circuit *circ, ID id);

////////////////////////////////////////////////////////////////////////////////
// Save / Load
////////////////////////////////////////////////////////////////////////////////

#define SAVE_VERSION 2

bool circ_save_file(Circuit *circ, const char *filename);
bool circ_load_file(Circuit *circ, const char *filename);

////////////////////////////////////////////////////////////////////////////////
// Platform
////////////////////////////////////////////////////////////////////////////////

const char *platform_locale();
const char *platform_resource_path();
const char *platform_data_path();
const char *platform_cache_path();
const char *platform_autosave_path();

////////////////////////////////////////////////////////////////////////////////
// Bitvector
////////////////////////////////////////////////////////////////////////////////

#define BV_BIT_SHIFT(bv)                                                       \
  ((sizeof(bv[0]) == 1)                                                        \
     ? 3                                                                       \
     : ((sizeof(bv[0]) == 2) ? 4 : ((sizeof(bv[0]) == 4) ? 5 : 6)))
#define BV_MASK(bv) ((1 << BV_BIT_SHIFT(bv)) - 1)

/** Helper to mark a bitvector type. Type should be 8, 16, 32 or 64 bit int. */
#define bv(type) type *

/** Set the length of a bitvector. The bitvector should be a null pointer to a
 * int array. */
#define bv_setlen(bv, len)                                                     \
  arrsetlen(bv, ((len) >> BV_BIT_SHIFT(bv)) + ((len) & BV_MASK(bv) ? 1 : 0))

/** Free the bitvector. */
#define bv_free(bv) arrfree(bv)

/** Set a bit in the bitvector. */
#define bv_set(bv, i)                                                          \
  (bv[(i) >> BV_BIT_SHIFT(bv)] |= (1ull << ((i) & BV_MASK(bv))))

/** Set a bit to a specific value in the bitvector. */
#define bv_set_to(bv, i, val) (val ? bv_set(bv, i) : bv_clear(bv, i))

/** Clear a specific bit of the bitvector. */
#define bv_clear(bv, i)                                                        \
  (bv[(i) >> BV_BIT_SHIFT(bv)] &= ~(1ull << ((i) & BV_MASK(bv))))

/** Clear all bits in the bitvector. */
#define bv_clear_all(bv) memset(bv, 0, arrlen(bv) * sizeof(bv[0]))

/** Set all bits in the bitvector. */
#define bv_set_all(bv) memset(bv, 0xFF, arrlen(bv) * sizeof(bv[0]))

/** Toggle a specific bit in the bitvector. */
#define bv_toggle(bv, i)                                                       \
  (bv[(i) >> BV_BIT_SHIFT(bv)] ^= (1ull << ((i) & BV_MASK(bv))))

/** Check if a specific bit is set in the bitvector. */
#define bv_is_set(bv, i)                                                       \
  (bv[(i) >> BV_BIT_SHIFT(bv)] & (1ull << ((i) & BV_MASK(bv))))

/** Get the value of a specific bit in the bitvector. */
#define bv_val(bv, i) bv_is_set(bv, i) >> ((i) & BV_MASK(bv))

#endif // CORE_H