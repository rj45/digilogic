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

#ifndef VIEW_H
#define VIEW_H

#include "core/core.h"
#include "render/draw.h"

typedef uint32_t VertexIndex;
#define NO_VERTEX UINT32_MAX

typedef uint32_t WireIndex;
#define NO_VERTEX UINT32_MAX

typedef struct PortView {
  // postion of center of port relative to the component
  HMM_Vec2 center;
} PortView;

typedef struct ComponentView {
  Box box;
} ComponentView;

typedef struct EndpointView {
  HMM_Vec2 pos;
} EndpointView;

typedef struct WaypointView {
  HMM_Vec2 pos;
} WaypointView;

typedef struct LabelView {
  Box bounds;
} LabelView;

typedef struct NetView {
  WireIndex wireOffset;
  uint32_t wireCount;
  VertexIndex vertexOffset;
} NetView;

typedef struct Wire {
  uint16_t vertexCount;
} Wire;

typedef struct CircuitView {
  Circuit circuit;
  Theme theme;
  DrawContext *drawCtx;

  ComponentView *components;
  PortView *ports;
  NetView *nets;
  EndpointView *endpoints;
  WaypointView *waypoints;
  LabelView *labels;

  arr(Wire) wires;
  arr(HMM_Vec2) vertices;

  ID hovered;
  arr(ID) selected;

  PortID hoveredPort;
  PortID selectedPort;

  Box selectionBox;

  bool debugMode;
} CircuitView;

#define view_component_ptr(view, id)                                           \
  (&(view)->components[circuit_component_index(&(view)->circuit, id)])
#define view_port_ptr(view, id)                                                \
  (&(view)->ports[circuit_port_index(&(view)->circuit, id)])
#define view_net_ptr(view, id)                                                 \
  (&(view)->nets[circuit_net_index(&(view)->circuit, id)])
#define view_endpoint_ptr(view, id)                                            \
  (&(view)->endpoints[circuit_endpoint_index(&(view)->circuit, id)])
#define view_waypoint_ptr(view, id)                                            \
  (&(view)->waypoints[circuit_waypoint_index(&(view)->circuit, id)])
#define view_label_ptr(view, id)                                               \
  (&(view)->labels[circuit_label_index(&(view)->circuit, id)])

typedef void *Context;

void view_init(
  CircuitView *view, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font);
void view_free(CircuitView *view);
ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position);
NetID view_add_net(CircuitView *view);
EndpointID
view_add_endpoint(CircuitView *view, NetID net, PortID port, HMM_Vec2 position);
WaypointID view_add_waypoint(CircuitView *view, NetID net, HMM_Vec2 position);

// wires all endpoints in the net together in a star pattern,
// mainly only useful in tests
void view_direct_wire_nets(CircuitView *view);

void view_fix_wire_end_vertices(CircuitView *view, WireIndex wire);
void view_draw(CircuitView *view);

LabelID view_add_label(CircuitView *view, const char *text, Box bounds);
Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize);
const char *view_label_text(CircuitView *view, LabelID id);

#endif // VIEW_H