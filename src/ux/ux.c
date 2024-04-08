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

#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#define MAX_ZOOM 20.0f

void ux_init(CircuitUX *ux, const ComponentDesc *componentDescs) {
  *ux = (CircuitUX){
    .avoid = avoid_new(),
  };
  bv_setlen(ux->input.keys, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keys);
  view_init(&ux->view, componentDescs);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keys);
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

NetID ux_add_net(CircuitUX *ux, PortID portFrom, PortID portTo) {
  NetID id = view_add_net(&ux->view, portFrom, portTo);

  Port *portFromPtr = &ux->view.circuit.ports[portFrom];
  Port *portToPtr = &ux->view.circuit.ports[portTo];
  avoid_add_edge(
    ux->avoid, id, portFromPtr->component, portFrom, portToPtr->component,
    portTo);

  return id;
}

void ux_add_vertex(CircuitUX *ux, NetID net, HMM_Vec2 vertex) {
  view_add_vertex(&ux->view, net, vertex);
}

void ux_rem_vertex(CircuitUX *ux, NetID net) {
  view_rem_vertex(&ux->view, net);
}

void ux_set_vertex(CircuitUX *ux, NetID net, VertexID index, HMM_Vec2 pos) {
  view_set_vertex(&ux->view, net, index, pos);
}

static void ux_zoom(CircuitUX *ux) {
  // calculate the new zoom
  ux->zoomExp += ux->input.scroll.Y * 0.5f;
  if (ux->zoomExp < -MAX_ZOOM) {
    ux->zoomExp = -MAX_ZOOM;
  } else if (ux->zoomExp > MAX_ZOOM) {
    ux->zoomExp = MAX_ZOOM;
  }
  float newZoom = powf(1.1f, ux->zoomExp);
  float oldZoom = ux->view.zoom;
  ux->view.zoom = newZoom;

  // figure out where the mouse was in "world coords" with the old zoom
  HMM_Vec2 originalMousePos =
    HMM_DivV2F(HMM_Sub(ux->input.mousePos, ux->view.pan), oldZoom);

  // figure out where the mouse is in "world coords" with the new zoom
  HMM_Vec2 newMousePos =
    HMM_DivV2F(HMM_Sub(ux->input.mousePos, ux->view.pan), newZoom);

  // figure out the correction to the pan so that the zoom is centred on the
  // mouse position
  HMM_Vec2 correction =
    HMM_MulV2F(HMM_Sub(newMousePos, originalMousePos), newZoom);
  ux->view.pan = HMM_Add(ux->view.pan, correction);
}

void ux_draw(CircuitUX *ux, Context ctx) {
  float dt = (float)ux->input.frameDuration;
  if (bv_is_set(ux->input.keys, KEYCODE_W)) {
    ux->view.pan.Y -= 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keys, KEYCODE_A)) {
    ux->view.pan.X -= 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keys, KEYCODE_S)) {
    ux->view.pan.Y += 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keys, KEYCODE_D)) {
    ux->view.pan.X += 600.0f * dt * ux->view.zoom;
  }

  if (ux->input.scroll.Y > 0.001 || ux->input.scroll.Y < -0.001) {
    ux_zoom(ux);
  }

  view_draw(&ux->view, ctx);
}

void ux_route(CircuitUX *ux) {
  avoid_route(ux->avoid);

  float coords[1024];

  for (int i = 0; i < arrlen(ux->view.nets); i++) {
    NetView *netView = &ux->view.nets[i];

    size_t len = avoid_get_edge_path(
      ux->avoid, i, coords, sizeof(coords) / sizeof(coords[0]));
    len /= 2;

    if (len <= 2) {
      continue;
    }

    while ((netView->vertexEnd - netView->vertexStart) < len - 2) {
      ux_add_vertex(ux, i, HMM_V2(0, 0));
    }
    while ((netView->vertexEnd - netView->vertexStart) > len - 2) {
      ux_rem_vertex(ux, i);
    }
    for (int j = 0; j < len - 2; j++) {
      ux_set_vertex(
        ux, i, j, HMM_V2(coords[(j + 1) * 2], coords[(j + 1) * 2 + 1]));
    }
  }
}