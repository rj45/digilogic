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

#include "core/structs.h"
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

PACK(typedef enum IDType{
  ID_NONE,
  ID_COMPONENT,
  ID_PORT,
  ID_NET,
  ID_ENDPOINT,
  ID_WAYPOINT,
  ID_LABEL,
})
IDType;

#define ID_TYPE_COUNT 7

// ID is defined in structs.h

#define NO_ID 0

typedef uint32_t Gen;

#define smap(type) type *

#define ID_TYPE_BITS 3
#define ID_GEN_BITS 7
#define ID_INDEX_BITS (32 - ID_TYPE_BITS - ID_GEN_BITS)

#define ID_TYPE_MASK ((1 << ID_TYPE_BITS) - 1)
#define ID_GEN_MASK ((1 << ID_GEN_BITS) - 1)
#define ID_INDEX_MASK ((1 << ID_INDEX_BITS) - 1)

#define ID_TYPE_SHIFT (ID_GEN_BITS + ID_INDEX_BITS)
#define ID_GEN_SHIFT (ID_INDEX_BITS)

#define id_make(type, gen, index)                                              \
  ((((ID)(type) & ID_TYPE_MASK) << ID_TYPE_SHIFT) |                            \
   (((ID)(gen) & ID_GEN_MASK) << ID_GEN_SHIFT) |                               \
   ((ID)(index) & ID_INDEX_MASK))
#define id_type(id) ((IDType)(((id) >> ID_TYPE_SHIFT) & ID_TYPE_MASK))
#define id_gen(id) ((Gen)(((id) >> ID_GEN_SHIFT) & ID_GEN_MASK))
#define id_typegen(id) ((ID)(id) >> ID_GEN_SHIFT)
#define id_index(id) ((ID)(id) & ID_INDEX_MASK)
#define id_valid(id) (id_gen(id) != 0 && id_type(id) != ID_NONE)

////////////////////////////////////////////////////////////////////////////////
// Bounding Boxes
////////////////////////////////////////////////////////////////////////////////

// Box is defined in structs.h

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
void bvh_add(BVH *bvh, ID item, Box box);
void bvh_remove(BVH *bvh, ID item, Box box);
void bvh_update(BVH *bvh, ID item, Box oldBox, Box newBox);
void bvh_rebuild(BVH *bvh);
arr(ID) bvh_query(BVH *bvh, Box box, arr(ID) result);

////////////////////////////////////////////////////////////////////////////////
// SparseMap
////////////////////////////////////////////////////////////////////////////////

struct SparseMap;

typedef struct SmapCallback {
  void *user;
  void (*fn)(void *user, ID id, void *ptr);
} SmapCallback;

typedef struct SyncedArray {
  void **ptr;
  uint32_t elemSize;
  arr(SmapCallback) create;
  arr(SmapCallback) update;
  arr(SmapCallback) delete;
} SyncedArray;

typedef struct SparseMap {
  // has the full id of each element
  ID *ids;

  // length and capacity of all synced arrays
  uint32_t length;
  uint32_t capacity;

  // type of IDs generated
  IDType type;

  // internal - has the type, generation and index of each element
  arr(ID) sparse;

  // internal - to keep the index small, keep track of free handle indices and
  // their old generation
  arr(ID) freeList;

  // internal - list of synced arrays
  arr(SyncedArray) syncedArrays;
} SparseMap;

void smap_init(SparseMap *smap, IDType type);
void smap_free(SparseMap *smap);
void smap_clone_from(SparseMap *dst, SparseMap *src);

void smap_add_synced_array(SparseMap *smap, void **ptr, uint32_t elemSize);
void smap_on_create(SparseMap *smap, void *array, SmapCallback callback);
void smap_on_update(SparseMap *smap, void *array, SmapCallback callback);
void smap_on_delete(SparseMap *smap, void *array, SmapCallback callback);

void smap_clear(SparseMap *smap);
ID smap_add(SparseMap *smap, void *value);
void smap_del(SparseMap *smap, ID id);
void smap_update_id(SparseMap *smap, ID id);
void smap_update_index(SparseMap *smap, uint32_t index);

static inline int smap_len(SparseMap *smap) { return smap->length; }

static inline bool smap_has(SparseMap *smap, ID id) {
  return id_valid(id) &&
         id_typegen(smap->sparse[id_index(id)]) == id_typegen(id);
}

static inline int smap_index(SparseMap *smap, ID id) {
  assert(smap_has(smap, id));
  return id_index(smap->sparse[id_index(id)]);
}

static inline ID smap_id(SparseMap *smap, int index) {
  return smap->ids[index];
}

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

#define NO_COMPONENT NO_ID
#define NO_NET NO_ID
#define NO_PORT NO_ID
#define NO_ENDPOINT NO_ID
#define NO_WAYPOINT NO_ID
#define NO_LABEL NO_ID
#define NO_VERTEX UINT32_MAX
#define NO_VERTEX UINT32_MAX

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

typedef struct Circuit {
  // important: keep in sync with IDType
  union {
    struct {
      SparseMap none;
      SparseMap components;
      SparseMap ports;
      SparseMap nets;
      SparseMap endpoints;
      SparseMap waypoints;
      SparseMap labels;
    } sm;
    SparseMap sparsemaps[ID_TYPE_COUNT];
  };

  // important: keep in sync with IDType
  union {
    struct {
      uint32_t *none;
      Component *components;
      Port *ports;
      Net *nets;
      Endpoint *endpoints;
      Waypoint *waypoints;
      Label *labels;
    };
    void *ptrs[ID_TYPE_COUNT];
  };

  const ComponentDesc *componentDescs;

  arr(char) text;

  struct {
    char key;
    uint32_t value;
  } *nextName;

  arr(Wire) wires;
  arr(HMM_Vec2) vertices;
} Circuit;

#define circuit_has(circuit, id)                                               \
  (smap_has(&(circuit)->sparsemaps[id_type(id)], (id)))
#define circuit_index(circuit, id)                                             \
  (smap_index(&(circuit)->sparsemaps[id_type(id)], (id)))
#define circuit_update_id(circuit, id)                                         \
  (smap_update_id(&(circuit)->sparsemaps[id_type(id)], (id)))
#define circuit_del(circuit, id)                                               \
  (smap_del(&(circuit)->sparsemaps[id_type(id)], (id)))

#define circuit_len(circuit, type) (smap_len(&(circuit)->sparsemaps[type]))
#define circuit_id(circuit, type, index)                                       \
  (smap_id(&(circuit)->sparsemaps[type], (index)))
#define circuit_update_index(circuit, type, index)                             \
  (smap_update_index(&(circuit)->sparsemaps[type], (index)))
#define circuit_on_create(circuit, type, user, callback)                       \
  (smap_on_create(                                                             \
    &(circuit)->sparsemaps[type], (circuit)->ptrs[type],                       \
    (SmapCallback){user, callback}))
#define circuit_on_update(circuit, type, user, callback)                       \
  (smap_on_update(                                                             \
    &(circuit)->sparsemaps[type], (circuit)->ptrs[type],                       \
    (SmapCallback){user, callback}))
#define circuit_on_delete(circuit, type, user, callback)                       \
  (smap_on_delete(                                                             \
    &(circuit)->sparsemaps[type], (circuit)->ptrs[type],                       \
    (SmapCallback){user, callback}))

#define circuit_component_ptr(circuit, id)                                     \
  (&(circuit)->components[circuit_index(circuit, id)])
#define circuit_component_len(circuit) (smap_len(&(circuit)->sm.components))
#define circuit_component_id(circuit, index)                                   \
  (smap_id(&(circuit)->sm.components, (index)))
#define circuit_component_update_index(circuit, index)                         \
  smap_update_index(&(circuit)->sm.components, (index))
#define circuit_on_component_create(circuit, user, callback)                   \
  smap_on_create(                                                              \
    &(circuit)->sm.components, (circuit)->components,                          \
    (SmapCallback){user, callback})
#define circuit_on_component_update(circuit, user, callback)                   \
  smap_on_update(                                                              \
    &(circuit)->sm.components, (circuit)->components,                          \
    (SmapCallback){user, callback})
#define circuit_on_component_delete(circuit, user, callback)                   \
  smap_on_delete(                                                              \
    &(circuit)->sm.components, (circuit)->components,                          \
    (SmapCallback){user, callback})

#define circuit_port_ptr(circuit, id)                                          \
  (&(circuit)->ports[circuit_index(circuit, id)])
#define circuit_port_len(circuit) (smap_len(&(circuit)->sm.ports))
#define circuit_port_id(circuit, index) (smap_id(&(circuit)->sm.ports, (index)))
#define circuit_port_update_index(circuit, index)                              \
  smap_update_index(&(circuit)->sm.ports, (index))
#define circuit_on_port_create(circuit, user, callback)                        \
  smap_on_create(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})
#define circuit_on_port_update(circuit, user, callback)                        \
  smap_on_update(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})
#define circuit_on_port_delete(circuit, user, callback)                        \
  smap_on_delete(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})

#define circuit_net_ptr(circuit, id)                                           \
  (&(circuit)->nets[circuit_index(circuit, id)])
#define circuit_net_len(circuit) (smap_len(&(circuit)->sm.nets))
#define circuit_net_id(circuit, index) (smap_id(&(circuit)->sm.nets, (index)))
#define circuit_net_update_index(circuit, index)                               \
  smap_update_index(&(circuit)->sm.nets, (index))
#define circuit_on_net_create(circuit, user, callback)                         \
  smap_on_create(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})
#define circuit_on_net_update(circuit, user, callback)                         \
  smap_on_update(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})
#define circuit_on_net_delete(circuit, user, callback)                         \
  smap_on_delete(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})

#define circuit_endpoint_ptr(circuit, id)                                      \
  (&(circuit)->endpoints[circuit_index(circuit, id)])
#define circuit_endpoint_len(circuit) (smap_len(&(circuit)->sm.endpoints))
#define circuit_endpoint_id(circuit, index)                                    \
  (smap_id(&(circuit)->sm.endpoints, (index)))
#define circuit_endpoint_update_index(circuit, index)                          \
  smap_update_index(&(circuit)->sm.endpoints, (index))
#define circuit_on_endpoint_create(circuit, user, callback)                    \
  smap_on_create(                                                              \
    &(circuit)->sm.endpoints, (circuit)->endpoints,                            \
    (SmapCallback){user, callback})
#define circuit_on_endpoint_update(circuit, user, callback)                    \
  smap_on_update(                                                              \
    &(circuit)->sm.endpoints, (circuit)->endpoints,                            \
    (SmapCallback){user, callback})
#define circuit_on_endpoint_delete(circuit, user, callback)                    \
  smap_on_delete(                                                              \
    &(circuit)->sm.endpoints, (circuit)->endpoints,                            \
    (SmapCallback){user, callback})

#define circuit_waypoint_ptr(circuit, id)                                      \
  (&(circuit)->waypoints[circuit_index(circuit, id)])
#define circuit_waypoint_len(circuit) (smap_len(&(circuit)->sm.waypoints))
#define circuit_waypoint_id(circuit, index)                                    \
  (smap_id(&(circuit)->sm.waypoints, (index)))
#define circuit_waypoint_update_index(circuit, index)                          \
  smap_update_index(&(circuit)->sm.waypoints, (index))
#define circuit_on_waypoint_create(circuit, user, callback)                    \
  smap_on_create(                                                              \
    &(circuit)->sm.waypoints, (circuit)->waypoints,                            \
    (SmapCallback){user, callback})
#define circuit_on_waypoint_update(circuit, user, callback)                    \
  smap_on_update(                                                              \
    &(circuit)->sm.waypoints, (circuit)->waypoints,                            \
    (SmapCallback){user, callback})
#define circuit_on_waypoint_delete(circuit, user, callback)                    \
  smap_on_delete(                                                              \
    &(circuit)->sm.waypoints, (circuit)->waypoints,                            \
    (SmapCallback){user, callback})

#define circuit_label_ptr(circuit, id)                                         \
  (&(circuit)->labels[circuit_index(circuit, id)])
#define circuit_label_len(circuit) (smap_len(&(circuit)->sm.labels))
#define circuit_label_id(circuit, index)                                       \
  (smap_id(&(circuit)->sm.labels, (index)))
#define circuit_label_update_index(circuit, index)                             \
  smap_update_index(&(circuit)->sm.labels, (index))
#define circuit_on_label_create(circuit, user, callback)                       \
  smap_on_create(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})
#define circuit_on_label_update(circuit, user, callback)                       \
  smap_on_update(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})
#define circuit_on_label_delete(circuit, user, callback)                       \
  smap_on_delete(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})

#define circuit_wire_vertex_count(wire_view) ((wire_view) & 0x7FFF)
#define circuit_wire_ends_in_junction(wire_view) ((bool)((wire_view) >> 15))

const ComponentDesc *circuit_component_descs();
void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs);
void circuit_free(Circuit *circuit);
void circuit_clear(Circuit *circuit);
void circuit_clone_from(Circuit *dst, Circuit *src);
ComponentID circuit_add_component(
  Circuit *circuit, ComponentDescID desc, HMM_Vec2 position);
void circuit_move_component(Circuit *circuit, ComponentID id, HMM_Vec2 delta);
void circuit_move_component_to(Circuit *circuit, ComponentID id, HMM_Vec2 pos);
NetID circuit_add_net(Circuit *circuit);
EndpointID circuit_add_endpoint(
  Circuit *circuit, NetID net, PortID port, HMM_Vec2 position);
void circuit_move_endpoint_to(
  Circuit *circuit, EndpointID id, HMM_Vec2 position);
void circuit_endpoint_connect(Circuit *circuit, EndpointID id, PortID port);
WaypointID
circuit_add_waypoint(Circuit *circuit, NetID netID, HMM_Vec2 position);
void circuit_move_waypoint(Circuit *circuit, WaypointID id, HMM_Vec2 delta);

LabelID circuit_add_label(Circuit *circuit, const char *text, Box bounds);
const char *circuit_label_text(Circuit *circuit, LabelID id);

void circuit_write_dot(Circuit *circuit, FILE *file);

////////////////////////////////////////////////////////////////////////////////
// Save / Load
////////////////////////////////////////////////////////////////////////////////

#define SAVE_VERSION 1

bool circuit_save_file(Circuit *circuit, const char *filename);
bool circuit_load_file(Circuit *circuit, const char *filename);

////////////////////////////////////////////////////////////////////////////////
// New circuit ECS
////////////////////////////////////////////////////////////////////////////////

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
    default: "type not found")

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

typedef enum ComponentID2 {
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
} ComponentID2;

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
  ModuleID *module;
  Size *size;
  Name *name;
  Prefix *prefix;
  SymbolShape *shape;
  LinkedList *ports;
} SymbolKind2;
#define SYMBOL_KIND_COMPONENTS                                                 \
  1 << COMPONENT_MODULE_ID | 1 << COMPONENT_SIZE | 1 << COMPONENT_NAME |       \
    1 << COMPONENT_PREFIX | 1 << COMPONENT_SYMBOL_SHAPE |                      \
    1 << COMPONENT_LINKED_LIST

typedef struct Symbol2 {
  TABLE_HEADER
  Parent *module;
  SymbolKindID *symbolKind;
  Position *position;
  Number *number; // combined with prefix to get the name
  ListNode *list;
} Symbol2;
#define SYMBOL_COMPONENTS                                                      \
  1 << COMPONENT_PARENT | 1 << COMPONENT_SYMBOL_KIND_ID |                      \
    1 << COMPONENT_POSITON | 1 << COMPONENT_NUMBER | 1 << COMPONENT_LIST_NODE

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
  LinkedList *waypoints;
  PortRef *port;
} Endpoint2;
#define ENDPOINT_COMPONENTS                                                    \
  1 << COMPONENT_PARENT | 1 << COMPONENT_POSITON | 1 << COMPONENT_LIST_NODE |  \
    1 << COMPONENT_LINKED_LIST | 1 << COMPONENT_PORT_REF

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
  Parent *netlist;
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
  Parent *module;
  LinkedList *nets;
} Netlist2;
#define NETLIST_COMPONENTS 1 << COMPONENT_PARENT | 1 << COMPONENT_LINKED_LIST

typedef struct Module2 {
  TABLE_HEADER
  SymbolKindID *symbolKind;
  NetlistID *nets;
  ListNode *list;
  LinkedList *symbols;
} Module2;
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
  strpool_t strpool;

  // changelog & events
  ChangeLog log;

  // temporary crossover code
  Circuit *oldCircuit;
  hashmap(ID, ID) oldToNew;
  hashmap(ID, ID) newToOld;
} Circuit2;

static inline bool circ_has(Circuit2 *circuit, ID id) {
  return id_index(id) < circuit->capacity &&
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
      .componentIndices[circ_component_id(componentType)],                     \
    ptr)

#define circ_set(circuit, id, componentType, ...)                              \
  circ_set_ptr(circuit, id, componentType, &(componentType)__VA_ARGS__)

#define circ_get_ptr(circuit, id, componentType)                               \
  ((const componentType *)((componentType *)circ_table_component_ptr(          \
    circuit, circ_type_for_id(circuit, id),                                    \
    circ_table_meta_for_id(circuit, id)                                        \
      .componentIndices[circ_component_id(componentType)],                     \
    circ_row_for_id(circuit, id))))

#define circ_get(circuit, id, componentType)                                   \
  (*circ_get_ptr(circuit, id, componentType))

#define circ_add(circuit, tableType)                                           \
  circ_add_type(circuit, circ_entity_type(tableType))
#define circ_add_id(circuit, tableType, id)                                    \
  circ_add_type_id(circuit, circ_entity_type(tableType), id)

void circ_init(Circuit2 *circ);
void circ_free(Circuit2 *circ);
void circ_load_symbol_descs(
  Circuit2 *circ, SymbolLayout *layout, const ComponentDesc *descs,
  size_t count);
void circ_add_type_id(Circuit2 *circ, EntityType type, ID id);
ID circ_add_type(Circuit2 *circ, EntityType type);
void circ_remove(Circuit2 *circ, ID id);
void circ_add_tags(Circuit2 *circ, ID id, Tag tags);
bool circ_has_tags(Circuit2 *circ, ID id, Tag tags);

StringHandle circ_str(Circuit2 *circ, const char *str, size_t len);
StringHandle circ_str_c(Circuit2 *circ, const char *str);
void circ_str_free(Circuit2 *circ, StringHandle handle);
const char *circ_str_get(Circuit2 *circ, StringHandle handle);

void circ_linked_list_append(Circuit2 *circ, ID parent, ID child);
void circ_linked_list_remove(Circuit2 *circ, ID parent, ID child);

static inline void
circ_set_(Circuit2 *circuit, int table, int row, int column, void *value) {
  memcpy(
    circ_table_component_ptr(circuit, table, column, row), value,
    circuit->tableMeta[table].componentSizes[column]);
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
  Circuit2 *circuit;
  EntityType type;
  int index;
} CircuitIter;

static inline CircuitIter circ_iter_(Circuit2 *circuit, EntityType type) {
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
  CircuitIter *it, int index, ComponentID2 componentID, void *value) {
  int column = it->circuit->tableMeta[it->type].componentIndices[componentID];
  circ_set_(it->circuit, it->type, index, column, value);
}

#define circ_iter(circuit, type) circ_iter_(circuit, circ_entity_type(type))
#define circ_iter_table(iter, type)                                            \
  (type *)circ_iter_table_(iter, circ_entity_type(type))
#define circ_iter_set_ptr(iter, index, componentType, ptr)                     \
  circ_iter_set_(iter, index, circ_component_id(componentType), ptr)
#define circ_iter_set(iter, index, componentType, ...)                         \
  circ_iter_set_ptr(iter, index, componentType, &(componentType)__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Higher level functions
////////////////////////////////////////////////////////////////////////////////

ID circ_add_port(Circuit2 *circ, ID symbolKind);
void circ_remove_port(Circuit2 *circ, ID id);

ID circ_add_symbol_kind(Circuit2 *circ);
void circ_remove_symbol_kind(Circuit2 *circ, ID id);

ID circ_add_symbol(Circuit2 *circ, ID module, ID symbolKind);
void circ_remove_symbol(Circuit2 *circ, ID id);
void circ_set_symbol_position(Circuit2 *circ, ID id, HMM_Vec2 position);

ID circ_add_waypoint(Circuit2 *circ, ID endpoint);
void circ_remove_waypoint(Circuit2 *circ, ID id);
void circ_set_waypoint_position(Circuit2 *circ, ID id, HMM_Vec2 position);

ID circ_add_endpoint(Circuit2 *circ, ID subnet);
void circ_remove_endpoint(Circuit2 *circ, ID id);
void circ_connect_endpoint_to_port(
  Circuit2 *circ, ID endpointID, ID symbolID, ID portID);

ID circ_add_subnet_bit(Circuit2 *circ, ID subnetBits);
void circ_remove_subnet_bit(Circuit2 *circ, ID id);

ID circ_add_subnet_bits(Circuit2 *circ, ID subnet);
void circ_remove_subnet_bits(Circuit2 *circ, ID id);

ID circ_add_subnet(Circuit2 *circ, ID net);
void circ_remove_subnet(Circuit2 *circ, ID id);

ID circ_add_net(Circuit2 *circ, ID module);
void circ_remove_net(Circuit2 *circ, ID id);

ID circ_add_module(Circuit2 *circ);
void circ_remove_module(Circuit2 *circ, ID id);

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