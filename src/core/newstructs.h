#ifndef STRUCTS_H
#define STRUCTS_H

#include <stddef.h>
#include <stdint.h>

#include "handmade_math.h"

typedef uint32_t ID;
typedef ID PortID;
typedef ID SymbolKindID;
typedef ID SymbolID;
typedef ID SubnetID;
typedef ID NetID;
typedef ID EndpointID;
typedef ID WaypointID;
typedef uint32_t StringHandle;

// ECS Tags

typedef enum EntityType {
  TYPE_NONE = 0,
  TYPE_PORT = 1,
  TYPE_SYMBOL_KIND = 2,
  TYPE_SYMBOL = 3,
  TYPE_WAYPOINT = 4,
  TYPE_ENDPOINT = 5,
  TYPE_SUBNET_BIT = 6,
  TYPE_SUBNET_BITS = 7,
  TYPE_SUBNET = 8,
  TYPE_NET = 9,
  TYPE_NETLIST = 10,
  TYPE_SUBCIRCUIT = 11,
  TYPE_COUNT = 12,
} EntityType;

typedef enum Tag {
  // first few bits reserved for EntityType

  TAG_IN = 1 << 8,
  TAG_OUT = 1 << 9,

  TAG_HOVERED = 1 << 10,
  TAG_SELECTED = 1 << 11,
  TAG_DRAGGING = 1 << 12,
  TAG_DEBUG = 1 << 15,
} Tag;

// ECS components

typedef ID SubcircuitID;
typedef ID SubnetBitsID;
typedef ID NetlistID;

typedef HMM_Vec2 Position;
typedef HMM_Vec2 Size;
typedef StringHandle Name;
typedef StringHandle Prefix;
typedef int32_t Number;
typedef ID Parent;

typedef struct ListNode {
  size_t length;
  ID next;
  ID prev;
} ListNode;

typedef struct LinkedList {
  size_t length;
  ID head;
  ID tail;
} LinkedList;

typedef struct PortRef {
  size_t length;
  SymbolID symbol;
  PortID port;
} PortRef;

typedef struct WireVertices {
  size_t length;
  uint16_t *wireVertexCounts;
  size_t wireCount;
  HMM_Vec2 *vertices;
} WireVertices;

// tables / entities

typedef struct Port {
  size_t length;
  Parent *symbolKind;
  Position *position;
  ListNode *list;
  Name *name;
  Number *pin;
} Port;

typedef struct SymbolKind {
  size_t length;
  SubcircuitID *subcircuit;
  Size *size;
  Name *name;
  Prefix *prefix;
  LinkedList *ports;
} SymbolKind;

typedef struct Symbol {
  size_t length;
  Parent *symbolKind;
  Position *position;
  Number *number; // combined with prefix to get the name
} Symbol;

typedef struct Waypoint {
  size_t length;
  Parent *endpoint;
  Position *position;
  ListNode *list;
} Waypoint;

typedef struct Endpoint {
  size_t length;
  Parent *subnet;
  PortRef *port;
  Position *position;
  ListNode *list;
} Endpoint;

typedef struct SubnetBit {
  size_t length;
  Number *bit;
  ListNode *list;
} SubnetBit;

typedef struct SubnetBits {
  size_t length;
  Parent *subnet;
  SubnetBitsID *override;
  LinkedList *bits;
} SubnetBits;

typedef struct Subnet {
  size_t length;
  Parent *net;
  Name *name;
  SubnetBitsID *bits;
  LinkedList *endpoints;
  ListNode *list;
} Subnet;

typedef struct Net {
  size_t length;
  Parent *Netlist;
  LinkedList *subnets;
  Name *name;
  WireVertices *wires;
  ListNode *list;
} Net;

typedef struct NetList {
  size_t length;
  LinkedList *nets;
} NetList;

typedef struct Subcircuit {
  size_t length;
  LinkedList *symbols;
  NetlistID *nets;
  ListNode *list;
} Subcircuit;

typedef struct Circuit {
  SubcircuitID top;

  // entities
  ID *entities;
  uint16_t *typeTags; // EntityType | Tag
  uint32_t *rows;
  uint32_t numEntities;

  // tables
  Port port;
  SymbolKind symbolKind;
  Symbol symbol;
  Waypoint waypoint;
  Endpoint endpoint;
  SubnetBit subnetBit;
  SubnetBits subnetBits;
  Subnet subnet;
  Net net;
  NetList netList;
  Subcircuit subcircuit;
} Circuit;

#endif // STRUCTS_H
