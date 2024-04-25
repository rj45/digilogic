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

#include <stdint.h>

// defines an STB array
#define arr(type) type *

typedef enum ID_TYPE {
  ID_NONE,
  ID_COMPONENT,
  ID_PORT,
  ID_NET,
  ID_WIRE,
  ID_JUNCTION,
  ID_LABEL,
} ID_TYPE;

typedef uint32_t ID;

#define ID_TYPE_BITS 3
#define ID_TYPE_MASK ((1 << ID_TYPE_BITS) - 1)
#define ID_INDEX_BITS (sizeof(ID) * 8 - ID_TYPE_BITS)
#define ID_INDEX_MASK ((1 << ID_INDEX_BITS) - 1)

#define id_type(id) (((id) >> ID_INDEX_BITS) & ID_TYPE_MASK)
#define id_index(id) ((id) & ID_INDEX_MASK)
#define id_make(type, index)                                                   \
  (((type & ID_TYPE_MASK) << ID_INDEX_BITS) | (index & ID_INDEX_MASK))

// default ComponentDescIDs for the built-in components
enum {
  COMP_NONE,
  COMP_AND,
  COMP_OR,
  COMP_NOT,
  COMP_INPUT,
  COMP_OUTPUT,
};

typedef uint32_t ComponentDescID;

typedef uint32_t PortDescID;

typedef uint32_t ComponentID;
#define NO_COMPONENT ((ComponentID)-1)

typedef uint32_t NetID;
#define NO_NET ((NetID)-1)

typedef uint32_t PortID;
#define NO_PORT ((PortID)-1)

typedef uint32_t VertexID;
#define NO_VERTEX ((VertexID)-1)

typedef uint32_t LabelID;
#define NO_LABEL ((LabelID)-1)

typedef uint32_t WireID;
#define NO_WIRE ((WireID)-1)

typedef uint32_t JunctionID;
#define NO_JUNCTION ((JunctionID)-1)

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

typedef struct ComponentDesc {
  const char *typeName;
  int numPorts;
  char namePrefix;
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
#define bv_set(bv, i) (bv[i >> BV_BIT_SHIFT(bv)] |= (1 << (i & BV_MASK(bv))))

/** Set a bit to a specific value in the bitvector. */
#define bv_set_to(bv, i, val) (val ? bv_set(bv, i) : bv_clear(bv, i))

/** Clear a specific bit of the bitvector. */
#define bv_clear(bv, i) (bv[i >> BV_BIT_SHIFT(bv)] &= ~(1 << (i & BV_MASK(bv))))

/** Clear all bits in the bitvector. */
#define bv_clear_all(bv) memset(bv, 0, arrlen(bv) * sizeof(bv[0]))

/** Set all bits in the bitvector. */
#define bv_set_all(bv) memset(bv, 0xFF, arrlen(bv) * sizeof(bv[0]))

/** Toggle a specific bit in the bitvector. */
#define bv_toggle(bv, i) (bv[i >> BV_BIT_SHIFT(bv)] ^= (1 << (i & BV_MASK(bv))))

/** Check if a specific bit is set in the bitvector. */
#define bv_is_set(bv, i) (bv[i >> BV_BIT_SHIFT(bv)] & (1 << (i & BV_MASK(bv))))

/** Get the value of a specific bit in the bitvector. */
#define bv_val(bv, i) bv_is_set(bv, i) >> (i & BV_MASK(bv))

#endif // CORE_H