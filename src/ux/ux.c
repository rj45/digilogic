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

#include "avoid/avoid.h"
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

void ux_init(
  CircuitUX *ux, const ComponentDesc *componentDescs, FontHandle font) {
  *ux = (CircuitUX){
    .avoid = avoid_new(),
  };
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  view_init(&ux->view, componentDescs, font);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  avoid_free(ux->avoid);
}

ComponentID
ux_add_component(CircuitUX *ux, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = view_add_component(&ux->view, descID, position);
  Component *component = &ux->view.circuit.components[id];
  Box box = ux->view.components[id].box;
  const ComponentDesc *desc = &ux->view.circuit.componentDescs[component->desc];

  avoid_add_node(
    ux->avoid, id, box.center.X - box.halfSize.X, box.center.Y - box.halfSize.Y,
    box.halfSize.X * 2.0f, box.halfSize.Y * 2.0f);

  PortID portStart = view_port_start(&ux->view, id);
  PortID portEnd = view_port_end(&ux->view, id);

  for (PortID portID = portStart; portID < portEnd; portID++) {
    PortView portView = ux->view.ports[portID];
    PortDirection dir = desc->ports[portID - portStart].direction;
    PortSide side = SIDE_LEFT;
    if (dir != PORT_IN) {
      side = SIDE_RIGHT;
    }
    HMM_Vec2 center = HMM_Add(portView.center, box.halfSize);
    avoid_add_port(ux->avoid, portID, id, side, center.X, center.Y);
  }
  return id;
}

NetID ux_add_net(CircuitUX *ux) { return view_add_net(&ux->view); }

JunctionID ux_add_junction(CircuitUX *ux, HMM_Vec2 position) {
  JunctionID id = view_add_junction(&ux->view, position);
  avoid_add_junction(ux->avoid, id, position.X, position.Y);
  return id;
}

WireID ux_add_wire(CircuitUX *ux, NetID net, WireEndID from, WireEndID to) {
  WireID id = view_add_wire(&ux->view, net, from, to);
  avoid_add_edge(ux->avoid, id, from, to, 0, 0, 0, 0);

  return id;
}

void ux_move_component(CircuitUX *ux, ComponentID id, HMM_Vec2 delta) {
  ComponentView *componentView = &ux->view.components[id];
  componentView->box.center = HMM_Add(componentView->box.center, delta);
  avoid_move_node(ux->avoid, id, delta.X, delta.Y);
}
