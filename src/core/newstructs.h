#ifndef STRUCTS_H
#define STRUCTS_H

#include <stddef.h>
#include <stdint.h>

#include "handmade_math.h"

// defines an STB array
#define arr(type) type *

typedef uint32_t ID;
typedef ID PortID;
typedef ID SymbolKindID;
typedef ID SymbolID;
typedef ID SubnetID;
typedef ID NetID;
typedef ID EndpointID;
typedef ID WaypointID;
typedef uint32_t StringHandle;

typedef struct TableHeader {
  size_t length;
  ID *id;
} TableHeader;

#define id_index(id) ((ID)(id)&0x00FFFFFF)
#define id_gen(id) (((ID)(id) >> 24) & 0xFF)
#define id_make(index, gen) (((ID)(gen) << 24) | (ID)(index)&0x00FFFFFF)

#define tagtype_tag(tag) ((tag)&0xFF00)
#define tagtype_type(tag) ((tag)&0x00FF)

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
  TYPE_SUBCIRCUIT,
  TYPE_COUNT,
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

//////////////////////////////////////////
// ECS Events
//////////////////////////////////////////

typedef enum EventType {
  EVENT_CREATE,
  EVENT_DELETE,
  EVENT_UPDATE,
  EVENT_COUNT,
} EventType;

typedef struct EventCreateDelete {
  EntityType entityType;
} EventCreateDelete;

typedef struct EventUpdate {
  uint32_t columnOffset;
  uint32_t fieldOffset;
  uint32_t fieldSize;
} EventUpdate;

typedef struct EventChange {
  EventType type;
  ID id;

  union {
    EventCreateDelete createDelete;
    EventUpdate update;
  };
} EventChange;

typedef struct EventQueue {
  arr(ID) ids;
} EventQueue;

//////////////////////////////////////////
// ECS components
//////////////////////////////////////////

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

//////////////////////////////////////////
// tables / entities
//////////////////////////////////////////

typedef struct Port {
  size_t length;
  ID *id;
  Parent *symbolKind;
  Position *position;
  ListNode *list;
  Name *name;
  Number *pin;
} Port;

typedef struct SymbolKind {
  size_t length;
  ID *id;
  SubcircuitID *subcircuit;
  Size *size;
  Name *name;
  Prefix *prefix;
  LinkedList *ports;
} SymbolKind;

typedef struct Symbol {
  size_t length;
  ID *id;
  Parent *symbolKind;
  Position *position;
  Number *number; // combined with prefix to get the name
} Symbol;

typedef struct Waypoint {
  size_t length;
  ID *id;
  Parent *endpoint;
  Position *position;
  ListNode *list;
} Waypoint;

typedef struct Endpoint {
  size_t length;
  ID *id;
  Parent *subnet;
  PortRef *port;
  Position *position;
  ListNode *list;
} Endpoint;

typedef struct SubnetBit {
  size_t length;
  ID *id;
  Number *bit;
  ListNode *list;
} SubnetBit;

typedef struct SubnetBits {
  size_t length;
  ID *id;
  Parent *subnet;
  SubnetBitsID *override;
  LinkedList *bits;
} SubnetBits;

typedef struct Subnet {
  size_t length;
  ID *id;
  Parent *net;
  Name *name;
  SubnetBitsID *bits;
  LinkedList *endpoints;
  ListNode *list;
} Subnet;

typedef struct Net {
  size_t length;
  ID *id;
  Parent *Netlist;
  LinkedList *subnets;
  Name *name;
  WireVertices *wires;
  ListNode *list;
} Net;

typedef struct NetList {
  size_t length;
  ID *id;
  LinkedList *nets;
} NetList;

typedef struct Subcircuit {
  size_t length;
  ID *id;
  LinkedList *symbols;
  NetlistID *nets;
  ListNode *list;
} Subcircuit;

typedef struct Circuit {
  SubcircuitID top;

  // entities
  uint8_t *generations;
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

  // indices
  TableHeader *header[TYPE_COUNT];
} Circuit;

#endif // STRUCTS_H
