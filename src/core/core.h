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

/** defines an STB array */
#define arr(type) type *

// default ComponentDescIDs for the built-in components
enum {
  COMP_NONE,
  COMP_AND,
  COMP_OR,
  COMP_NOT,
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
  const char *name;
  int numPorts;
  PortDesc *ports;
} ComponentDesc;

typedef struct Component {
  ComponentDescID desc;
  PortID portStart;
} Component;

typedef struct Port {
  ComponentID component;
  PortDescID desc; // index into the component's port descriptions

  // the net the port is connected to. May be just the head of a linked list of
  // nets.
  NetID net;
} Port;

typedef struct Net {
  PortID portFrom;
  PortID portTo;

  // linked list of all nets connected to the same source ports
  NetID next;
  NetID prev;
} Net;

typedef struct Circuit {
  const ComponentDesc *componentDescs;
  arr(Component) components;
  arr(Port) ports;
  arr(Net) nets;
} Circuit;

const ComponentDesc *circuit_component_descs();
void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs);
void circuit_free(Circuit *circuit);
ComponentID circuit_add_component(Circuit *circuit, ComponentDescID desc);
NetID circuit_add_net(Circuit *circuit, PortID portFrom, PortID portTo);

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