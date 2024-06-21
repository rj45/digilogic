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

UTEST(View, view_add_symbols) {
  CircuitView view = {0};

  DrawContext *draw = draw_create();

  view_init(&view, circuit_component_descs(), draw, NULL);

  SymbolKindID andKindID = circ_get_symbol_kind_by_name(&view.circuit2, "AND");
  SymbolKindID orKindID = circ_get_symbol_kind_by_name(&view.circuit2, "OR");

  ID andID = circ_add_symbol(&view.circuit2, view.circuit2.top, andKindID);
  circ_set_symbol_position(&view.circuit2, andID, HMM_V2(100, 150));
  ID orID = circ_add_symbol(&view.circuit2, view.circuit2.top, orKindID);
  circ_set_symbol_position(&view.circuit2, orID, HMM_V2(200, 250));

  ASSERT_EQ(circ_len(&view.circuit2, Symbol2), 2);

  Position pos = circ_get(&view.circuit2, andID, Position);
  ASSERT_EQ(pos.X, 100);
  ASSERT_EQ(pos.Y, 150);

  pos = circ_get(&view.circuit2, orID, Position);
  ASSERT_EQ(pos.X, 200);
  ASSERT_EQ(pos.Y, 250);

  view_free(&view);
  draw_free(draw);
}

UTEST(View, view_draw_symbols) {
  CircuitView view = {0};
  DrawContext *draw = draw_create();

  view_init(&view, circuit_component_descs(), draw, NULL);

  SymbolKindID orKindID = circ_get_symbol_kind_by_name(&view.circuit2, "OR");
  ID orID = circ_add_symbol(&view.circuit2, view.circuit2.top, orKindID);
  circ_set_symbol_position(&view.circuit2, orID, HMM_V2(100, 100));
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

  SymbolKindID xorKindID = circ_get_symbol_kind_by_name(&view.circuit2, "XOR");
  SymbolKindID orKindID = circ_get_symbol_kind_by_name(&view.circuit2, "OR");

  ID xorID = circ_add_symbol(&view.circuit2, view.circuit2.top, xorKindID);
  circ_set_symbol_position(&view.circuit2, xorID, HMM_V2(100, 100));

  ID orID = circ_add_symbol(&view.circuit2, view.circuit2.top, orKindID);
  circ_set_symbol_position(&view.circuit2, orID, HMM_V2(200, 200));

  // Get the output port of XOR (assuming it's the last port)
  ID xorPortID = circ_get(&view.circuit2, xorID, LinkedList).tail;

  // Get the first input port of OR
  ID orPortID = circ_get(&view.circuit2, xorID, LinkedList).head;

  // Create a net and add endpoints
  ID netID = circ_add_net(&view.circuit2, view.circuit2.top);
  ID xorEndpointID = circ_add_endpoint(&view.circuit2, netID);
  circ_connect_endpoint_to_port(
    &view.circuit2, xorEndpointID, xorID, xorPortID);
  ID orEndpointID = circ_add_endpoint(&view.circuit2, netID);
  circ_connect_endpoint_to_port(&view.circuit2, orEndpointID, orID, orPortID);

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
