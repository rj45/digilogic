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

#include "autoroute/autoroute.h"
#include "core/core.h"
#include "handmade_math.h"
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

void ux_global_init() { autoroute_global_init(); }

void ux_init(
  CircuitUX *ux, const ComponentDesc *componentDescs, FontHandle font) {
  *ux = (CircuitUX){0};
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  view_init(&ux->view, componentDescs, font);
  ux->router = autoroute_create(&ux->view);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  autoroute_free(ux->router);
}

ComponentID
ux_add_component(CircuitUX *ux, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = view_add_component(&ux->view, descID, position);
  autoroute_update_component(ux->router, id);
  return id;
}

NetID ux_add_net(CircuitUX *ux) { return view_add_net(&ux->view); }

JunctionID ux_add_junction(CircuitUX *ux, HMM_Vec2 position) {
  JunctionID id = view_add_junction(&ux->view, position);
  autoroute_update_junction(ux->router, id);
  return id;
}

WireID ux_add_wire(CircuitUX *ux, NetID net, ID from, ID to) {
  WireID id = view_add_wire(&ux->view, net, from, to);
  autoroute_update_wire(ux->router, id);
  return id;
}

void ux_move_component(CircuitUX *ux, ComponentID id, HMM_Vec2 delta) {
  ComponentView *componentView = view_component_ptr(&ux->view, id);
  componentView->box.center = HMM_AddV2(componentView->box.center, delta);

  printf("Move updating component %x\n", id);
  autoroute_update_component(ux->router, id);
  PortID portID = circuit_component_ptr(&ux->view.circuit, id)->portFirst;
  while (portID) {
    Port *port = circuit_port_ptr(&ux->view.circuit, portID);

    // todo: this is slow, maybe store wireID instead of netID on port?
    bool found = false;
    NetID netID = port->net;
    if (netID != NO_NET) {
      WireID wireID = circuit_net_ptr(&ux->view.circuit, netID)->wireFirst;
      while (wireID) {
        Wire *wire = circuit_wire_ptr(&ux->view.circuit, wireID);
        if (wire->from == portID || wire->to == portID) {
          printf("  Move updating wire %x\n", wireID);
          autoroute_update_wire(ux->router, wireID);
          found = true;
        }
        wireID = wire->next;
      }
    } else {
      printf("  Port %x has no net\n", portID);
    }

    if (!found) {
      printf("  Could not find wire for port %x\n", portID);
    }

    portID = port->compNext;
  }
}
