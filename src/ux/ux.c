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

#define LOG_LEVEL LL_INFO
#include "log.h"

void ux_global_init() { autoroute_global_init(); }

void ux_init(
  CircuitUX *ux, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ux = (CircuitUX){
    .betterRoutes = true,
  };
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  bv_clear_all(ux->input.keysPressed);
  view_init(&ux->view, componentDescs, drawCtx, font);
  ux->router = autoroute_create(&ux->view);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  bv_free(ux->input.keysPressed);
  arrfree(ux->undoStack);
  arrfree(ux->redoStack);
  autoroute_free(ux->router);
}

ComponentID
ux_add_component(CircuitUX *ux, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = circuit_add_component(&ux->view.circuit, descID, position);
  autoroute_update_component(ux->router, id);
  return id;
}

NetID ux_add_net(CircuitUX *ux) {
  NetID id = circuit_add_net(&ux->view.circuit);
  autoroute_update_net(ux->router, id);
  return id;
}

EndpointID
ux_add_endpoint(CircuitUX *ux, NetID net, PortID port, HMM_Vec2 position) {
  log_debug("Adding endpoint to net %x", net);
  EndpointID id = circuit_add_endpoint(&ux->view.circuit, net, port, position);
  autoroute_update_endpoint(ux->router, id);
  return id;
}

WaypointID ux_add_waypoint(CircuitUX *ux, NetID net, HMM_Vec2 position) {
  log_debug("Adding waypoint to net %x", net);
  WaypointID id = circuit_add_waypoint(&ux->view.circuit, net, position);
  autoroute_update_waypoint(ux->router, id);
  return id;
}

void ux_move_component(CircuitUX *ux, ComponentID id, HMM_Vec2 delta) {
  Component *component = circuit_component_ptr(&ux->view.circuit, id);
  assert(!isnan(delta.X));
  assert(!isnan(delta.Y));

  component->box.center = HMM_AddV2(component->box.center, delta);

  log_debug("Move updating component %x", id);
  autoroute_update_component(ux->router, id);

  PortID portID = circuit_component_ptr(&ux->view.circuit, id)->portFirst;
  while (portID) {
    Port *port = circuit_port_ptr(&ux->view.circuit, portID);

    HMM_Vec2 pos = HMM_AddV2(component->box.center, port->position);

    if (port->endpoint != NO_ENDPOINT) {
      Endpoint *endpoint =
        circuit_endpoint_ptr(&ux->view.circuit, port->endpoint);
      endpoint->position = pos;

      autoroute_update_endpoint(ux->router, port->endpoint);
    }

    portID = port->compNext;
  }
}

void ux_move_waypoint(CircuitUX *ux, WaypointID id, HMM_Vec2 delta) {
  Waypoint *waypoint = circuit_waypoint_ptr(&ux->view.circuit, id);
  waypoint->position = HMM_AddV2(waypoint->position, delta);

  autoroute_update_waypoint(ux->router, id);
}

HMM_Vec2 ux_calc_selection_center(CircuitUX *ux) {
  HMM_Vec2 center = HMM_V2(0, 0);
  assert(arrlen(ux->view.selected) > 0);
  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    ID id = ux->view.selected[i];
    if (id_type(id) == ID_COMPONENT) {
      Component *component = circuit_component_ptr(&ux->view.circuit, id);
      center = HMM_AddV2(center, component->box.center);
    } else if (id_type(id) == ID_WAYPOINT) {
      Waypoint *waypoint = circuit_waypoint_ptr(&ux->view.circuit, id);
      center = HMM_AddV2(center, waypoint->position);
    }
  }
  center = HMM_DivV2F(center, (float)arrlen(ux->view.selected));
  return center;
}

void ux_route(CircuitUX *ux) { autoroute_route(ux->router, ux->betterRoutes); }
