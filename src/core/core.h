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

// defines an STB array
#define arr(type) type *

// defines an STB hash map
#define hmap(type) type *

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define MUST_USE_RETURN __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define MUST_USE_RETURN _Check_return_
#else
#define MUST_USE_RETURN
#endif

////////////////////////////////////////////////////////////////////////////////
// SparseMap and Generational Handle IDs
////////////////////////////////////////////////////////////////////////////////

typedef enum IDType {
  ID_NONE,
  ID_COMPONENT,
  ID_PORT,
  ID_NET,
  ID_ENDPOINT,
  ID_WAYPOINT,
  ID_LABEL,
} IDType;

typedef uint32_t ID;
#define NO_ID 0

typedef int8_t Gen;

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
  (((type & ID_TYPE_MASK) << ID_TYPE_SHIFT) |                                  \
   ((gen & ID_GEN_MASK) << ID_GEN_SHIFT) | (index & ID_INDEX_MASK))
#define id_type(id) ((IDType)(((id) >> ID_TYPE_SHIFT) & ID_TYPE_MASK))
#define id_gen(id) ((Gen)(((id) >> ID_GEN_SHIFT) & ID_GEN_MASK))
#define id_typegen(id) ((id) >> ID_GEN_SHIFT)
#define id_index(id) ((id) & ID_INDEX_MASK)
#define id_valid(id) (id_gen(id) != 0 && id_type(id) != ID_NONE)

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

void smap_add_synced_array(SparseMap *smap, void **ptr, uint32_t elemSize);
void smap_on_create(SparseMap *smap, void *array, SmapCallback callback);
void smap_on_update(SparseMap *smap, void *array, SmapCallback callback);
void smap_on_delete(SparseMap *smap, void *array, SmapCallback callback);

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
    .Y = HMM_MIN(a.center.Y - a.halfSize.Y, b.center.Y - b.halfSize.Y)};
  HMM_Vec2 br = (HMM_Vec2){
    .X = HMM_MAX(a.center.X + a.halfSize.X, b.center.X + b.halfSize.X),
    .Y = HMM_MAX(a.center.Y + a.halfSize.Y, b.center.Y + b.halfSize.Y)};
  return box_from_tlbr(tl, br);
}

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
};

typedef uint32_t ComponentDescID;

typedef uint32_t PortDescID;

typedef uint32_t ComponentID;
#define NO_COMPONENT ((ComponentID) - 1)

typedef ID NetID;
#define NO_NET NO_ID

typedef ID PortID;
#define NO_PORT NO_ID

typedef ID EndpointID;
#define NO_ENDPOINT NO_ID

typedef ID WaypointID;
#define NO_WAYPOINT NO_ID

typedef ID LabelID;
#define NO_LABEL NO_ID

typedef uint32_t VertexIndex;
#define NO_VERTEX UINT32_MAX

typedef uint32_t WireIndex;
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

typedef struct Component {
  Box box;

  ComponentDescID desc;

  PortID portFirst;
  PortID portLast;

  LabelID typeLabel;
  LabelID nameLabel;
} Component;

typedef struct Port {
  HMM_Vec2 position;

  ComponentID component;
  PortDescID desc; // index into the component's port descriptions
  LabelID label;

  // linked list of all ports in the component
  PortID compNext;
  PortID compPrev;

  // the net the port is connected to.
  NetID net;

  // the endpoint the port is connected to.
  // ports must have an endpoint.
  EndpointID endpoint;

  // linked list of all ports connected to the same net
  PortID netNext;
  PortID netPrev;
} Port;

typedef struct Endpoint {
  HMM_Vec2 position;

  NetID net;

  // optional port connected to this endpoint
  // endpoints do not need to have a port.
  PortID port;

  EndpointID next;
  EndpointID prev;
} Endpoint;

typedef struct Waypoint {
  HMM_Vec2 position;

  NetID net;

  // linked list of all waypoints in the net
  WaypointID next;
  WaypointID prev;
} Waypoint;

typedef struct Net {
  // head and tail of the linked list of endpoints connected to this net
  EndpointID endpointFirst;
  EndpointID endpointLast;

  // head and tail of the linked list of waypoints in this net
  WaypointID waypointFirst;
  WaypointID waypointLast;

  LabelID label;

  WireIndex wireOffset;
  uint32_t wireCount;
  VertexIndex vertexOffset;
} Net;

typedef struct Wire {
  uint16_t vertexCount;
} Wire;

typedef struct Label {
  Box box;
  uint32_t textOffset;
} Label;

typedef struct Circuit {
  struct {
    SparseMap components;
    SparseMap ports;
    SparseMap nets;
    SparseMap endpoints;
    SparseMap waypoints;
    SparseMap labels;
  } sm;

  const ComponentDesc *componentDescs;

  Component *components;
  Port *ports;
  Net *nets;
  Endpoint *endpoints;
  Waypoint *waypoints;
  Label *labels;

  arr(char) text;

  struct {
    char key;
    uint32_t value;
  } *nextName;

  arr(Wire) wires;
  arr(HMM_Vec2) vertices;
} Circuit;

#define circuit_component_index(circuit, id)                                   \
  (smap_index(&(circuit)->sm.components, (id)))
#define circuit_component_ptr(circuit, id)                                     \
  (&(circuit)->components[circuit_component_index(circuit, id)])
#define circuit_component_len(circuit) (smap_len(&(circuit)->sm.components))
#define circuit_component_id(circuit, index)                                   \
  (smap_id(&(circuit)->sm.components, (index)))
#define circuit_component_update_id(circuit, id)                               \
  smap_update_id(&(circuit)->sm.components, (id))
#define circuit_component_update_index(circuit, index)                         \
  smap_update_index(&(circuit)->sm.components, (index))
#define circuit_component_del(circuit, id)                                     \
  smap_del(&(circuit)->sm.components, (id))
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

#define circuit_port_index(circuit, id) (smap_index(&(circuit)->sm.ports, (id)))
#define circuit_port_ptr(circuit, id)                                          \
  (&(circuit)->ports[circuit_port_index(circuit, id)])
#define circuit_port_len(circuit) (smap_len(&(circuit)->sm.ports))
#define circuit_port_id(circuit, index) (smap_id(&(circuit)->sm.ports, (index)))
#define circuit_port_update_id(circuit, id)                                    \
  smap_update_id(&(circuit)->sm.ports, (id))
#define circuit_port_update_index(circuit, index)                              \
  smap_update_index(&(circuit)->sm.ports, (index))
#define circuit_port_del(circuit, id) smap_del(&(circuit)->sm.ports, (id))
#define circuit_on_port_create(circuit, user, callback)                        \
  smap_on_create(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})
#define circuit_on_port_update(circuit, user, callback)                        \
  smap_on_update(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})
#define circuit_on_port_delete(circuit, user, callback)                        \
  smap_on_delete(                                                              \
    &(circuit)->sm.ports, (circuit)->ports, (SmapCallback){user, callback})

#define circuit_net_index(circuit, id) (smap_index(&(circuit)->sm.nets, (id)))
#define circuit_net_ptr(circuit, id)                                           \
  (&(circuit)->nets[circuit_net_index(circuit, id)])
#define circuit_net_len(circuit) (smap_len(&(circuit)->sm.nets))
#define circuit_net_id(circuit, index) (smap_id(&(circuit)->sm.nets, (index)))
#define circuit_net_update_id(circuit, id)                                     \
  smap_update_id(&(circuit)->sm.nets, (id))
#define circuit_net_update_index(circuit, index)                               \
  smap_update_index(&(circuit)->sm.nets, (index))
#define circuit_net_del(circuit, id) smap_del(&(circuit)->sm.nets, (id))
#define circuit_on_net_create(circuit, user, callback)                         \
  smap_on_create(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})
#define circuit_on_net_update(circuit, user, callback)                         \
  smap_on_update(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})
#define circuit_on_net_delete(circuit, user, callback)                         \
  smap_on_delete(                                                              \
    &(circuit)->sm.nets, (circuit)->nets, (SmapCallback){user, callback})

#define circuit_endpoint_index(circuit, id)                                    \
  (smap_index(&(circuit)->sm.endpoints, (id)))
#define circuit_endpoint_ptr(circuit, id)                                      \
  (&(circuit)->endpoints[circuit_endpoint_index(circuit, id)])
#define circuit_endpoint_len(circuit) (smap_len(&(circuit)->sm.endpoints))
#define circuit_endpoint_id(circuit, index)                                    \
  (smap_id(&(circuit)->sm.endpoints, (index)))
#define circuit_endpoint_update_id(circuit, id)                                \
  smap_update_id(&(circuit)->sm.endpoints, (id))
#define circuit_endpoint_update_index(circuit, index)                          \
  smap_update_index(&(circuit)->sm.endpoints, (index))
#define circuit_endpoint_del(circuit, id)                                      \
  smap_del(&(circuit)->sm.endpoints, (id))
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

#define circuit_waypoint_index(circuit, id)                                    \
  (smap_index(&(circuit)->sm.waypoints, (id)))
#define circuit_waypoint_ptr(circuit, id)                                      \
  (&(circuit)->waypoints[circuit_waypoint_index(circuit, id)])
#define circuit_waypoint_len(circuit) (smap_len(&(circuit)->sm.waypoints))
#define circuit_waypoint_id(circuit, index)                                    \
  (smap_id(&(circuit)->sm.waypoints, (index)))
#define circuit_waypoint_update_id(circuit, id)                                \
  smap_update_id(&(circuit)->sm.waypoints, (id))
#define circuit_waypoint_update_index(circuit, index)                          \
  smap_update_index(&(circuit)->sm.waypoints, (index))
#define circuit_waypoint_del(circuit, id)                                      \
  smap_del(&(circuit)->sm.waypoints, (id))
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

#define circuit_label_index(circuit, id)                                       \
  (smap_index(&(circuit)->sm.labels, (id)))
#define circuit_label_ptr(circuit, id)                                         \
  (&(circuit)->labels[circuit_label_index(circuit, id)])
#define circuit_label_len(circuit) (smap_len(&(circuit)->sm.labels))
#define circuit_label_id(circuit, index)                                       \
  (smap_id(&(circuit)->sm.labels, (index)))
#define circuit_label_update_id(circuit, id)                                   \
  smap_update_id(&(circuit)->sm.labels, (id))
#define circuit_label_update_index(circuit, index)                             \
  smap_update_index(&(circuit)->sm.labels, (index))
#define circuit_label_del(circuit, id) smap_del(&(circuit)->sm.labels, (id))
#define circuit_on_label_create(circuit, user, callback)                       \
  smap_on_create(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})
#define circuit_on_label_update(circuit, user, callback)                       \
  smap_on_update(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})
#define circuit_on_label_delete(circuit, user, callback)                       \
  smap_on_delete(                                                              \
    &(circuit)->sm.labels, (circuit)->labels, (SmapCallback){user, callback})

const ComponentDesc *circuit_component_descs();
void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs);
void circuit_free(Circuit *circuit);
ComponentID circuit_add_component(
  Circuit *circuit, ComponentDescID desc, HMM_Vec2 position);
void circuit_move_component(Circuit *circuit, ComponentID id, HMM_Vec2 delta);
NetID circuit_add_net(Circuit *circuit);
EndpointID circuit_add_endpoint(
  Circuit *circuit, NetID net, PortID port, HMM_Vec2 position);
WaypointID circuit_add_waypoint(Circuit *circuit, NetID, HMM_Vec2 position);
void circuit_move_waypoint(Circuit *circuit, WaypointID id, HMM_Vec2 delta);

LabelID circuit_add_label(Circuit *circuit, const char *text, Box bounds);
const char *circuit_label_text(Circuit *circuit, LabelID id);

void circuit_write_dot(Circuit *circuit, FILE *file);

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
} BVH;

void bvh_init(BVH *bvh);
void bvh_free(BVH *bvh);
void bvh_add(BVH *bvh, Box box, ID item);
void bvh_remove(BVH *bvh, Box box, ID item);
void bvh_update(BVH *bvh, Box oldBox, Box newBox, ID item);
void bvh_rebuild(BVH *bvh);
arr(ID) bvh_query(BVH *bvh, Box box, arr(ID) result);

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