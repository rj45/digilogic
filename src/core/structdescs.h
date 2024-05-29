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

// This file is used by gen.c to generate structs.h and structdescs.c.
// It uses X-Macros to build a reflection system. See core/structdesc.h

#include <stddef.h>
#include <stdint.h>

#define TYPE_LIST                                                              \
  X(uint32_t, ID,                                                              \
    "An identifier indicating the type and location of something in a "        \
    "Circuit.")                                                                \
  X(uint32_t, ComponentDescID, "An index of a ComponentDesc")                  \
  X(uint32_t, PortDescID, "A index of a PortDesc within a ComponentDesc.")     \
  X(ID, ComponentID, "")                                                       \
  X(ID, NetID, "")                                                             \
  X(ID, PortID, "")                                                            \
  X(ID, EndpointID, "")                                                        \
  X(ID, WaypointID, "")                                                        \
  X(ID, LabelID, "")                                                           \
  X(uint32_t, VertexIndex, "An index into the Circuit's Vertex array.")        \
  X(uint32_t, WireIndex, "An index into the Circuit's Wire array.")

#ifndef TYPE_DESC_DEF
/* One time only definitons */
#define TYPE_DESC_DEF
typedef struct TypeDesc {
  const char *type;
  const char *name;
  const char *desc;
} TypeDesc;
#endif

#define STR_NOEXPAND(A) #A
#define STR(A) STR_NOEXPAND(A)

#define CAT_NOEXPAND(A, B) A##B
#define CAT(A, B) CAT_NOEXPAND(A, B)

#define X(type, name, desc) typedef type name;
TYPE_LIST
#undef X

static const TypeDesc typeList[] = {
#define X(type, name, desc) {STR(type), STR(name), desc},
  TYPE_LIST
#undef X
};

#undef TYPE_LIST
#undef STR_NOEXPAND
#undef STR
#undef CAT_NOEXPAND
#undef CAT

#define STRUCT_DESC "A 2d vector or point."
#define STRUCT_NAME HMM_Vec2
#define STRUCT_FIELDS                                                          \
  X(float, X, "")                                                              \
  X(float, Y, "")
#include "core/structdesc.h"

#define STRUCT_DESC "An axis aligned bounding box."
#define STRUCT_NAME Box
#define STRUCT_FIELDS                                                          \
  X(HMM_Vec2, center, "The center of the axis aligned bounding box.")          \
  X(HMM_Vec2, halfSize, "The dimensions of the box from the center.")
#include "core/structdesc.h"

#define STRUCT_DESC "A wire connecting two points in a Net."
#define STRUCT_NAME Wire
#define STRUCT_FIELDS                                                          \
  X(uint16_t, vertexCount,                                                     \
    "The number of Vertices in the Wire. The most significant bit is a "       \
    "flag\nindicating whether the Wire has a junction.")
#include "core/structdesc.h"

#define STRUCT_DESC "Nothing."
#define STRUCT_NAME None
#define STRUCT_FIELDS
#include "core/structdesc.h"

#define STRUCT_DESC "A circuit component or gate."
#define STRUCT_NAME Component
#define STRUCT_FIELDS                                                          \
  X(Box, box, "Bounding box of the Component.")                                \
  X(ComponentDescID, desc, "The index of the ComponentDesc of the Component.") \
  X(PortID, portFirst, "Head of a linked list of ports.")                      \
  X(PortID, portLast, "Tail of a linked list of ports.")                       \
  X(LabelID, typeLabel, "Label indicating the type of the component.")         \
  X(LabelID, nameLabel, "Label indicating the name of the component.")
#include "core/structdesc.h"

#define STRUCT_DESC "A module port or component pin."
#define STRUCT_NAME Port
#define STRUCT_FIELDS                                                          \
  X(HMM_Vec2, position, "Position of the port relative to its component.")     \
  X(ComponentID, component, "ID of the Component the Port belongs to.")        \
  X(PortDescID, desc,                                                          \
    "The index of the PortDesc in the Component's PortDesc list.")             \
  X(LabelID, label, "The ID of the Label with the port's name.")               \
  X(PortID, next,                                                              \
    "The ID of the next Port in a linked list of the component's Ports.")      \
  X(PortID, prev,                                                              \
    "The ID of the previous Port in a linked list of the component's Ports.")  \
  X(NetID, net, "The ID of the Net the Port is connected to.")                 \
  X(EndpointID, endpoint, "The ID of the Endpoint connected to this Port.")
#include "core/structdesc.h"

#define STRUCT_DESC "A connection point for a Net."
#define STRUCT_NAME Endpoint
#define STRUCT_FIELDS                                                          \
  X(HMM_Vec2, position, "The position of the Endpoint.")                       \
  X(NetID, net, "The ID of the Net the Endpoint is a part of.")                \
  X(PortID, port,                                                              \
    "The ID of the Port the Endpoint is connected to.\nThis is optional, an "  \
    "endpoint may be floating.")                                               \
  X(EndpointID, next,                                                          \
    "The ID of the next Endpoint in a linked list of the Net's Endpoints.")    \
  X(EndpointID, prev,                                                          \
    "The ID of the previous Endpoint in a linked list of the Net's "           \
    "Endpoints.")
#include "core/structdesc.h"

#define STRUCT_DESC "A routing waypoint for influencing the autorouter."
#define STRUCT_NAME Waypoint
#define STRUCT_FIELDS                                                          \
  X(HMM_Vec2, position, "The position of the Waypoint.")                       \
  X(NetID, net, "The ID of the Net the Waypoint is a part of.")                \
  X(WaypointID, next,                                                          \
    "The ID of the next Waypoint in a linked list of the Net's Waypoints.")    \
  X(WaypointID, prev,                                                          \
    "The ID of the previous Waypoint in a linked list of the Net's "           \
    "Waypoints.")
#include "core/structdesc.h"

#define STRUCT_DESC "Represents a network of wires connecting Ports together."
#define STRUCT_NAME Net
#define STRUCT_FIELDS                                                          \
  X(EndpointID, endpointFirst,                                                 \
    "The ID of the first Endpoint in a linked list of the Net's Endpoints.")   \
  X(EndpointID, endpointLast,                                                  \
    "The ID of the last Endpoint in a linked list of the Net's Endpoints.")    \
  X(WaypointID, waypointFirst,                                                 \
    "The ID of the first Waypoint in a linked list of the Net's Waypoints.")   \
  X(WaypointID, waypointLast,                                                  \
    "The ID of the last Waypoint in a linked list of the Net's Waypoints.")    \
  X(LabelID, label, "The ID of the Label with the net's name.")                \
  X(WireIndex, wireOffset,                                                     \
    "The index of the first Wire in the Circuit's Wire array.")                \
  X(uint32_t, wireCount, "The number of Wires in the Net.")                    \
  X(VertexIndex, vertexOffset,                                                 \
    "The index of the first Vertex in the Circuit's Vertex array.")
#include "core/structdesc.h"

#define STRUCT_DESC "A label for a Componet, Port or Net."
#define STRUCT_NAME Label
#define STRUCT_FIELDS                                                          \
  X(Box, box, "The bounding box of the text of the Label.")                    \
  X(uint32_t, textOffset,                                                      \
    "Offset into the Circuit's text array where the text of the Label "        \
    "starts.")
#include "core/structdesc.h"

static const char *externTypes[] = {
  "HMM_Vec2",
};

static const StructDesc helperDescs[] = {
  HMM_Vec2Desc,
  BoxDesc,
  WireDesc,
};

// NOTE: this must be in the same order as ID_TYPE
static const StructDesc structDescs[] = {
  NoneDesc,     ComponentDesc, PortDesc,  NetDesc,
  EndpointDesc, WaypointDesc,  LabelDesc,
};