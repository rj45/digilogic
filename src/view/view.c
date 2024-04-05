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
#include "handmade_math.h"
#include "stb_ds.h"

#include "core/core.h"
#include "view.h"

#define PORT_SPACING 20.0f
#define COMPONENT_WIDTH PORT_SPACING * 3
#define PORT_WIDTH 15.0f
#define BORDER_WIDTH 1.0f

void view_init(CircuitView *view, const ComponentDesc *componentDescs) {
  *view = (CircuitView){
    .avoid = avoid_new(),
    .pan = HMM_V2(0.0f, 0.0f),
    .zoom = 1.0f,
  };
  circuit_init(&view->circuit, componentDescs);
}

void view_free(CircuitView *view) {
  arrfree(view->components);
  arrfree(view->ports);
  arrfree(view->nets);
  arrfree(view->vertices);
  avoid_free(view->avoid);
  circuit_free(&view->circuit);
}

ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = circuit_add_component(&view->circuit, descID);

  Component *component = &view->circuit.components[id];
  const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

  ComponentView componentViewTmp = {.position = position};
  arrput(view->components, componentViewTmp);
  ComponentView *componentView = &view->components[id];

  // figure out the size of the component
  int numInputPorts = 0;
  int numOutputPorts = 0;
  for (int j = 0; j < desc->numPorts; j++) {

    if (desc->ports[j].direction == PORT_IN) {
      numInputPorts++;
    } else if (desc->ports[j].direction != PORT_IN) {
      numOutputPorts++;
    }
  }
  float height =
    fmaxf(numInputPorts, numOutputPorts) * PORT_SPACING + PORT_SPACING;
  float width = COMPONENT_WIDTH;
  componentView->size = HMM_V2(width, height);

  avoid_add_node(view->avoid, id, position.X, position.Y, width, height);

  // figure out the position of each port
  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc;
  float rightY = rightInc;

  for (int j = 0; j < desc->numPorts; j++) {
    PortView portView = (PortView){0};
    PortSide side = SIDE_LEFT;

    if (desc->ports[j].direction == PORT_IN) {
      portView.position =
        HMM_V2(-PORT_WIDTH / 2 + BORDER_WIDTH / 2, leftY - PORT_WIDTH / 2);
      leftY += leftInc;
      side = SIDE_LEFT;
    } else if (desc->ports[j].direction != PORT_IN) {
      portView.position = HMM_V2(
        width - PORT_WIDTH / 2 - BORDER_WIDTH / 2, rightY - PORT_WIDTH / 2);
      rightY += rightInc;
      side = SIDE_RIGHT;
    }

    PortID portID = arrlen(view->ports);
    HMM_Vec2 center =
      HMM_Add(portView.position, HMM_V2(PORT_WIDTH / 2, PORT_WIDTH / 2));
    avoid_add_port(view->avoid, portID, id, side, center.X, center.Y);

    arrput(view->ports, portView);
  }

  return id;
}

NetID view_add_net(CircuitView *view, PortID portFrom, PortID portTo) {
  NetID id = circuit_add_net(&view->circuit, portFrom, portTo);
  NetView netView = {0};

  Port *portFromPtr = &view->circuit.ports[portFrom];
  Port *portToPtr = &view->circuit.ports[portTo];

  avoid_add_edge(
    view->avoid, id, portFromPtr->component, portFrom, portToPtr->component,
    portTo);

  arrput(view->nets, netView);
  return id;
}

static HMM_Vec2 panZoom(CircuitView *view, HMM_Vec2 position) {
  return HMM_Add(HMM_MulV2F(position, view->zoom), view->pan);
}

static HMM_Vec2 zoom(CircuitView *view, HMM_Vec2 size) {
  return HMM_MulV2F(size, view->zoom);
}

void view_draw(CircuitView *view, Context ctx) {
  for (int i = 0; i < arrlen(view->components); i++) {
    ComponentView *componentView = &view->components[i];
    Component *component = &view->circuit.components[i];
    const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

    HMM_Vec2 pos = panZoom(view, componentView->position);
    HMM_Vec2 size = zoom(view, componentView->size);

    draw_filled_rect(
      ctx, pos, size, view->zoom * 5.0f, HMM_V4(0.5f, 0.5f, 0.5f, 1.0f));
    draw_stroked_rect(
      ctx, pos, size, view->zoom * 5.0f, view->zoom * 1.0f,
      HMM_V4(0.8, 0.8, 0.8, 1.0f));

    for (int j = 0; j < desc->numPorts; j++) {
      PortView *portView = &view->ports[component->portStart + j];
      // Port *port = &view->circuit.ports[component->portStart + j];

      HMM_Vec2 portPosition =
        panZoom(view, HMM_Add(componentView->position, portView->position));
      HMM_Vec2 portSize = zoom(view, HMM_V2(PORT_WIDTH, PORT_WIDTH));

      draw_filled_circle(
        ctx, portPosition, portSize, HMM_V4(0.8, 0.8, 0.8, 1.0f));
      draw_stroked_circle(
        ctx, portPosition, portSize, view->zoom * 1.0f,
        HMM_V4(0.3, 0.3, 0.3, 1.0f));
    }
  }

  for (int i = 0; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];
    Net *net = &view->circuit.nets[i];

    Port *portFrom = &view->circuit.ports[net->portFrom];
    PortView *portViewFrom = &view->ports[net->portFrom];
    ComponentView *componentViewFrom = &view->components[portFrom->component];

    HMM_Vec2 portFromPosition = panZoom(
      view,
      HMM_Add(
        HMM_Add(
          componentViewFrom->position, HMM_V2(PORT_WIDTH / 2, PORT_WIDTH / 2)),
        portViewFrom->position));

    Port *portTo = &view->circuit.ports[net->portTo];
    PortView *portViewTo = &view->ports[net->portTo];
    ComponentView *componentViewTo = &view->components[portTo->component];

    HMM_Vec2 portToPosition = panZoom(
      view,
      HMM_Add(
        HMM_Add(
          componentViewTo->position, HMM_V2(PORT_WIDTH / 2, PORT_WIDTH / 2)),
        portViewTo->position));

    HMM_Vec2 pos = portFromPosition;

    for (int i = netView->vertexStart; i < netView->vertexEnd; i++) {
      HMM_Vec2 vertex = panZoom(view, view->vertices[i]);
      draw_stroked_line(
        ctx, pos, vertex, view->zoom * 2.0f, HMM_V4(0.3, 0.6, 0.3, 1.0f));
      pos = vertex;
    }

    draw_stroked_line(
      ctx, pos, portToPosition, view->zoom * 2.0f, HMM_V4(0.3, 0.6, 0.3, 1.0f));
  }
}

void view_add_vertex(CircuitView *view, NetID net, HMM_Vec2 vertex) {
  NetView *netView = &view->nets[net];
  arrins(view->vertices, netView->vertexEnd, vertex);
  netView->vertexEnd++;
  for (int i = net + 1; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];
    netView->vertexStart++;
    netView->vertexEnd++;
  }
}

void view_rem_vertex(CircuitView *view, NetID net) {
  NetView *netView = &view->nets[net];
  arrdel(view->vertices, netView->vertexEnd);
  netView->vertexEnd--;
  for (int i = net + 1; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];
    netView->vertexStart--;
    netView->vertexEnd--;
  }
}

void view_route(CircuitView *view) {
  avoid_route(view->avoid);

  float coords[1024];

  for (int i = 0; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];

    size_t len = avoid_get_edge_path(
      view->avoid, i, coords, sizeof(coords) / sizeof(coords[0]));
    len /= 2;

    if (len <= 2) {
      continue;
    }

    while ((netView->vertexEnd - netView->vertexStart) < len - 2) {
      view_add_vertex(view, i, HMM_V2(0, 0));
    }
    while ((netView->vertexEnd - netView->vertexStart) > len - 2) {
      view_rem_vertex(view, i);
    }
    for (int j = 0; j < len - 2; j++) {
      view->vertices[netView->vertexStart + j] =
        HMM_V2(coords[(j + 1) * 2], coords[(j + 1) * 2 + 1]);
    }
  }
}
