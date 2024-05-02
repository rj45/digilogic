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
  ID_WIRE,
  ID_JUNCTION,
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

typedef enum SparseMapChange {
  SMAP_ADD,
  SMAP_SWAP,
  SMAP_DEL,
  SMAP_UPDATE,
} SparseMapChange;

struct SparseMap;

typedef struct SyncedArray {
  void **ptr;
  uint32_t elemSize;
  void *user;
  void (*callback)(
    void *user, struct SparseMap *smap, SparseMapChange change, ID id,
    uint32_t index, uint32_t index2);
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

void smap_add_synced_array(
  SparseMap *smap, void **ptr, uint32_t elemSize, void *user,
  void (*callback)(
    void *user, SparseMap *smap, SparseMapChange change, ID id, uint32_t index,
    uint32_t index2));

ID smap_alloc(SparseMap *smap);
void smap_del(SparseMap *smap, ID id);

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

static inline void smap_update(SparseMap *smap, ID id) {
  assert(smap_has(smap, id));

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    SyncedArray *syncedArray = &smap->syncedArrays[i];
    syncedArray->callback(
      syncedArray->user, smap, SMAP_UPDATE, id, smap_index(smap, id), 0);
  }
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

typedef uint32_t NetID;
#define NO_NET ((NetID) - 1)

typedef uint32_t PortID;
#define NO_PORT ((PortID) - 1)

typedef uint32_t VertexID;
#define NO_VERTEX ((VertexID) - 1)

typedef uint32_t LabelID;
#define NO_LABEL ((LabelID) - 1)

typedef uint32_t WireID;
#define NO_WIRE ((WireID) - 1)

typedef uint32_t JunctionID;
#define NO_JUNCTION ((JunctionID) - 1)

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
  ComponentDescID desc;
  PortID portStart;
  LabelID typeLabel;
  LabelID nameLabel;
} Component;

typedef struct Port {
  ComponentID component;
  PortDescID desc; // index into the component's port descriptions
  LabelID label;

  // the net the port is connected to.
  NetID net;

  // linked list of all ports connected to the same net
  PortID next;
  PortID prev;
} Port;

typedef uint32_t WireEndID;
typedef enum WireEndType {
  WIRE_END_INVALID,
  WIRE_END_NONE,
  WIRE_END_PORT,
  WIRE_END_JUNC,
} WireEndType;

#define WIRE_END_TYPE_BITS 2
#define WIRE_END_TYPE_MASK ((1 << WIRE_END_TYPE_BITS) - 1)
#define WIRE_END_INDEX_BITS (sizeof(WireEndID) * 8 - WIRE_END_TYPE_BITS)
#define WIRE_END_INDEX_MASK ((1 << WIRE_END_INDEX_BITS) - 1)

#define wire_end_type(id)                                                      \
  (WireEndType)(((id) >> WIRE_END_INDEX_BITS) & WIRE_END_TYPE_MASK)
#define wire_end_index(id) ((id) & WIRE_END_INDEX_MASK)
#define wire_end_make(type, index)                                             \
  ((((uint32_t)(type) & WIRE_END_TYPE_MASK) << WIRE_END_INDEX_BITS) |          \
   ((uint32_t)(index) & WIRE_END_INDEX_MASK))

typedef struct Wire {
  NetID net;

  WireEndID from;
  WireEndID to;

  // linked list of all wires in the net
  WireID next;
  WireID prev;
} Wire;

typedef struct Junction {
  NetID net;

  // linked list of all junctions in the net
  JunctionID next;
  JunctionID prev;
} Junction;

typedef struct Net {
  // head and tail of the linked list of ports connected to this net
  PortID portFirst;
  PortID portLast;

  // head and tail of the linked list of wires in this net
  WireID wireFirst;
  WireID wireLast;

  // head and tail of the linked list of junctions in this net
  JunctionID junctionFirst;
  JunctionID junctionLast;

  LabelID label;
} Net;

typedef struct Label {
  uint32_t textOffset;
} Label;

typedef struct Circuit {
  const ComponentDesc *componentDescs;
  arr(Component) components;
  arr(Port) ports;
  arr(Net) nets;
  arr(Wire) wires;
  arr(Junction) junctions;
  arr(Label) labels;
  arr(char) text;

  struct {
    char key;
    uint32_t value;
  } *nextName;
} Circuit;

const ComponentDesc *circuit_component_descs();
void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs);
void circuit_free(Circuit *circuit);
ComponentID circuit_add_component(Circuit *circuit, ComponentDescID desc);
NetID circuit_add_net(Circuit *circuit);
JunctionID circuit_add_junction(Circuit *circuit);
WireID
circuit_add_wire(Circuit *circuit, NetID net, WireEndID from, WireEndID to);

LabelID circuit_add_label(Circuit *circuit, const char *text);
const char *circuit_label_text(Circuit *circuit, LabelID id);

void circuit_write_dot(Circuit *circuit, FILE *file);

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

////////////////////////////////////////////////////////////////////////////////
// Timer
////////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct Timer {
#if defined(__APPLE__)
  struct {
    mach_timebase_info_data_t timebase;
    uint64_t start;
  } mach;
#elif defined(__EMSCRIPTEN__)
  // empty
#elif defined(_WIN32)
  struct {
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
  } win;
#else // Linux, Android, ...
#ifdef CLOCK_MONOTONIC
#define TIMER_CLOCK_MONOTONIC CLOCK_MONOTONIC
#else
// on some embedded platforms, CLOCK_MONOTONIC isn't defined
#define TIMER_CLOCK_MONOTONIC (1)
#endif
  struct {
    uint64_t start;
  } posix;
#endif
} Timer;

void timer_init(Timer *ts);
double timer_now(Timer *ts);

#endif // CORE_H