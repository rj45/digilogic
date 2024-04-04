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

#include "core/core.h"

typedef struct PortView {
  // postion of port relative to the component
  HMM_Vec2 position;
} PortView;

typedef struct ComponentView {
  // position of the component relative to the circuit
  HMM_Vec2 position;
  // size of the component
  HMM_Vec2 size;
} ComponentView;

typedef struct NetView {
  VertexID vertexStart;
  VertexID vertexEnd;
} NetView;

typedef struct CircuitView {
  Circuit circuit;
  arr(ComponentView) components;
  arr(PortView) ports;
  arr(NetView) nets;
  arr(HMM_Vec2) vertices;

  HMM_Vec2 pan;
  float zoom;
  float zoomExp;
} CircuitView;

typedef void *Context;

void view_init(CircuitView *view, const ComponentDesc *componentDescs);
void view_free(CircuitView *view);
ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position);
NetID view_add_net(CircuitView *circuit, PortID portFrom, PortID portTo);
void view_draw(CircuitView *view, Context ctx);

static inline PortID view_port_start(CircuitView *view, ComponentID id) {
  return view->circuit.components[id].portStart;
}

static inline PortID view_port_end(CircuitView *view, ComponentID id) {
  return view->circuit.components[id].portStart +
         view->circuit.componentDescs[view->circuit.components[id].desc]
           .numPorts;
}

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

#endif // VIEW_H