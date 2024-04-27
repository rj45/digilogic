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

typedef enum VertAlign {
  ALIGN_TOP,
  ALIGN_MIDDLE,
  ALIGN_BOTTOM,
} VertAlign;

typedef enum HorizAlign {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT,
} HorizAlign;

typedef void *FontHandle;

typedef struct Theme {
  float portSpacing;
  float componentWidth;
  float portWidth;
  float borderWidth;
  float componentRadius;
  float wireThickness;
  float labelPadding;
  float labelFontSize;
  FontHandle font;
  struct {
    HMM_Vec4 component;
    HMM_Vec4 componentBorder;
    HMM_Vec4 port;
    HMM_Vec4 portBorder;
    HMM_Vec4 wire;
    HMM_Vec4 hovered;
    HMM_Vec4 selected;
    HMM_Vec4 selectFill;
    HMM_Vec4 labelColor;
    HMM_Vec4 nameColor;
  } color;
} Theme;

void theme_init(Theme *theme, FontHandle font);

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

  arr(ComponentView) components;
  arr(PortView) ports;
  arr(WireView) wires;
  arr(JunctionView) junctions;
  arr(HMM_Vec2) vertices;
  arr(LabelView) labels;

  HMM_Vec2 pan;
  float zoom;

  ComponentID hoveredComponent;
  arr(ComponentID) selectedComponents;

  PortID hoveredPort;
  PortID selectedPort;

  Box selectionBox;
} CircuitView;

typedef void *Context;

void view_init(
  CircuitView *view, const ComponentDesc *componentDescs, FontHandle font);
void view_free(CircuitView *view);
ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position);
NetID view_add_net(CircuitView *view);
JunctionID view_add_junction(CircuitView *view, HMM_Vec2 position);
WireID
view_add_wire(CircuitView *view, NetID net, WireEndID from, WireEndID to);

void view_add_vertex(CircuitView *view, WireID wire, HMM_Vec2 vertex);
void view_rem_vertex(CircuitView *view, WireID wire);
void view_fix_wire_end_vertices(CircuitView *view, WireID wire);
void view_set_vertex(
  CircuitView *view, WireID wire, VertexID index, HMM_Vec2 pos);
void view_draw(CircuitView *view, Context ctx);

static inline PortID view_port_start(CircuitView *view, ComponentID id) {
  return view->circuit.components[id].portStart;
}

static inline PortID view_port_end(CircuitView *view, ComponentID id) {
  return view->circuit.components[id].portStart +
         view->circuit.componentDescs[view->circuit.components[id].desc]
           .numPorts;
}

LabelID view_add_label(CircuitView *view, const char *text, Box bounds);
Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize);
const char *view_label_text(CircuitView *view, LabelID id);

////////////////////////////////////////
// external interface for drawing the circuit
////////////////////////////////////////

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color);
void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color);
void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color);
void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color);
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_text(
  Context ctx, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor);
Box draw_text_bounds(
  HMM_Vec2 pos, const char *text, int len, HorizAlign horz, VertAlign vert,
  float fontSize, FontHandle font);

#endif // VIEW_H