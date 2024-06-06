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

// This file is mainly for documentation. This is the object heirarchy done
// in a more traditional object pointer graph kind of style.

#include <stddef.h>
#include <stdint.h>

#include "handmade_math.h"

// defines an STB array
#define arr(type) type *
typedef HMM_Vec2 Position;
typedef HMM_Vec2 Size;
typedef struct Subcircuit Subcircuit;

// Ports can be input, output or bidirectional.
typedef enum Direction {
  DIR_IN,
  DIR_OUT,
  DIR_INOUT,
} Direction;

// A Port is a connection point for a wire.
typedef struct Port {
  Position position;
  Direction direction;
  const char *name;
  int32_t pin;
} Port;

// A SymbolKind is a "class" or "prototype" for a Symbol. A SymbolKind
// can be implemented with a Subcircuit, allowing hierarchical design.
typedef struct SymbolKind {
  Subcircuit *subcircuit;
  arr(Port) ports;
  Size size;
  const char *name;
  const char *prefix;
} SymbolKind;

// A Symbol is an instance of a SymbolKind. It has a position and a name.
typedef struct Symbol {
  SymbolKind *kind;
  Position position;
  int number; // combined with kind->prefix to get the name
} Symbol;

// A Waypoint influences the path of a wire connected to an endpoint.
typedef struct Waypoint {
  Position position;
} Waypoint;

// An Endpoint is a connection point for a wire. It can be connected to a
// component or be floating.
typedef struct Endpoint {
  Symbol *component;
  Port *port;
  Position position;
  arr(Waypoint) waypoints;
} Endpoint;

// A SubnetBits is a collection of bits for a Subnet. It can be overridden in a
// Subnet in a parent circuit. The topmost SubnetBits is the one that is used.
typedef struct SubnetBits {
  struct SubnetBits *override;
  arr(uint8_t) bits;
} SubnetBits;

// A Subnet is a collection of endpoints that are all electrically connected,
// and is some subset of the bits in a Net.
typedef struct Subnet {
  const char *name;
  arr(Endpoint) endpoints;
  SubnetBits *bits;
} Subnet;

// A Net is a collection of subnets that are all electrically connected together
// in a Circuit. Nets can have named subnets, which are some subset of the bits
// of the net.
typedef struct Net {
  const char *name;
  arr(Subnet) subnets;

  // results from auto-routing (which is why they are not STB arrays)
  uint16_t *wireVertexCounts;
  size_t wireCount;
  HMM_Vec2 *vertices;
} Net;

// A NetList is a collection of nets in a Subcircuit.
typedef struct NetList {
  arr(Net) nets;
} NetList;

// A Subcircuit is a collection of components and nets. Think if this like
// the circuit's "class" or "prototype". Through Symbol it can be
// instantiated.
typedef struct Subcircuit {
  arr(Symbol) components;
  NetList *netlist;
} Subcircuit;

// A Circuit is the top level circuit that represents the entire workspace.
typedef struct Circuit {
  Subcircuit *top;
} Circuit;
