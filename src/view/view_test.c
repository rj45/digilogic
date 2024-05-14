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

#include "core/core.h"
#include "render/draw.h"
#include "render/draw_test.h"

#include "handmade_math.h"
#include "stb_ds.h"
#include "utest.h"

#include "view.h"
#include <stdbool.h>

UTEST(View, view_add_component) {
  CircuitView view = {0};

  DrawContext *draw = draw_create();

  view_init(&view, circuit_component_descs(), draw, NULL);

  view_add_component(&view, COMP_AND, HMM_V2(100, 100));
  view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  ASSERT_EQ(circuit_component_len(&view.circuit), 2);
  ASSERT_EQ(view.components[0].box.center.X, 100);
  ASSERT_EQ(view.components[0].box.center.Y, 100);
  ASSERT_EQ(view.components[1].box.center.X, 200);
  ASSERT_EQ(view.components[1].box.center.Y, 200);

  view_free(&view);
  draw_free(draw);
}

UTEST(View, view_draw_components) {
  CircuitView view = {0};
  DrawContext *draw = draw_create();

  view_init(&view, circuit_component_descs(), draw, NULL);

  view_add_component(&view, COMP_OR, HMM_V2(100, 100));

  view_draw(&view);

  ASSERT_STREQ(
    "component(OR, v0, -)\n"
    "label(component_name, v1, 'X1', -)\n"
    "port(v2, -)\n"
    "port(v3, -)\n"
    "port(v4, -)\n",
    draw_get_build_string(draw));

  view_free(&view);
  draw_free(draw);
}

UTEST(View, view_draw_component_with_wires) {
  CircuitView view = {0};
  DrawContext *draw = draw_create();

  view_init(&view, circuit_component_descs(), draw, NULL);
  ComponentID and = view_add_component(&view, COMP_XOR, HMM_V2(100, 100));
  ComponentID or = view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  Component *andComp = circuit_component_ptr(&view.circuit, and);
  PortID from = circuit_port_ptr(
                  &view.circuit,
                  circuit_port_ptr(&view.circuit, andComp->portFirst)->compNext)
                  ->compNext;

  Component *orComp = circuit_component_ptr(&view.circuit, or);
  PortID to = orComp->portFirst;

  NetID net = view_add_net(&view);
  view_add_endpoint(&view, net, from, HMM_V2(0, 0));
  view_add_endpoint(&view, net, to, HMM_V2(0, 0));
  view_direct_wire_nets(&view);

  view_draw(&view);

  ASSERT_STREQ(
    "component(XOR, v0, -)\n"
    "label(component_name, v1, 'X1', -)\n"
    "port(v2, -)\n"
    "port(v3, -)\n"
    "port(v4, -)\n"
    "component(OR, v5, -)\n"
    "label(component_name, v6, 'X2', -)\n"
    "port(v7, -)\n"
    "port(v8, -)\n"
    "port(v9, -)\n"
    "wire(v4, v7, -)\n",
    draw_get_build_string(draw));

  view_free(&view);
  draw_free(draw);
}