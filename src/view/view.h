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

typedef struct PortView {
  // postion of center of port relative to the component
  HMM_Vec2 center;
} PortView;

typedef struct ComponentView {
  Box box;
} ComponentView;

typedef struct WireView {
  VertexID vertexStart;
  VertexID vertexEnd;
} WireView;

typedef struct JunctionView {
  HMM_Vec2 pos;
} JunctionView;

typedef struct LabelView {
  Box bounds;
} LabelView;

typedef struct CircuitView {
  Circuit circuit;
  Theme theme;
  DrawContext *drawCtx;

  ComponentView *components;
  PortView *ports;
  WireView *wires;
  JunctionView *junctions;
  LabelView *labels;

  arr(HMM_Vec2) vertices;

  ID hovered;
  arr(ID) selected;

  PortID hoveredPort;
  PortID selectedPort;

  Box selectionBox;
} CircuitView;

#define view_component_ptr(view, id)                                           \
  (&(view)->components[circuit_component_index(&(view)->circuit, id)])
#define view_port_ptr(view, id)                                                \
  (&(view)->ports[circuit_port_index(&(view)->circuit, id)])
#define view_wire_ptr(view, id)                                                \
  (&(view)->wires[circuit_wire_index(&(view)->circuit, id)])
#define view_junction_ptr(view, id)                                            \
  (&(view)->junctions[circuit_junction_index(&(view)->circuit, id)])
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
JunctionID view_add_junction(CircuitView *view, HMM_Vec2 position);
WireID view_add_wire(CircuitView *view, NetID net, ID from, ID to);

void view_add_vertex(CircuitView *view, WireID wire, HMM_Vec2 vertex);
void view_rem_vertex(CircuitView *view, WireID wire);
void view_fix_wire_end_vertices(CircuitView *view, WireID wire);
void view_set_vertex(
  CircuitView *view, WireID wire, VertexID index, HMM_Vec2 pos);
void view_draw(CircuitView *view);

LabelID view_add_label(CircuitView *view, const char *text, Box bounds);
Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize);
const char *view_label_text(CircuitView *view, LabelID id);

#endif // VIEW_H