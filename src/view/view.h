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

#include "handmade_math.h"
#include <stdbool.h>

#include "core/core.h"

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

typedef struct NetView {
  VertexID vertexStart;
  VertexID vertexEnd;
} NetView;

typedef struct LabelView {
  Box bounds;
} LabelView;

typedef struct CircuitView {
  Circuit circuit;
  Theme theme;

  arr(ComponentView) components;
  arr(PortView) ports;
  arr(NetView) nets;
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
NetID view_add_net(CircuitView *circuit, PortID portFrom, PortID portTo);
void view_add_vertex(CircuitView *view, NetID net, HMM_Vec2 vertex);
void view_rem_vertex(CircuitView *view, NetID net);
void view_set_vertex(
  CircuitView *view, NetID net, VertexID index, HMM_Vec2 pos);
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