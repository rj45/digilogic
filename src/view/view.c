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

#include "handmade_math.h"
#include "stb_ds.h"

#include "core/core.h"
#include "view.h"

#include <assert.h>

void theme_init(Theme *theme) {
  *theme = (Theme){
    .portSpacing = 20.0f,
    .componentWidth = 20.0f * 3,
    .portWidth = 15.0f,
    .borderWidth = 1.0f,
    .componentRadius = 5.0f,
    .wireThickness = 2.0f,
    .color =
      {
        .component = HMM_V4(0.5f, 0.5f, 0.5f, 1.0f),
        .componentBorder = HMM_V4(0.8f, 0.8f, 0.8f, 1.0f),
        .port = HMM_V4(0.8f, 0.8f, 0.8f, 1.0f),
        .portBorder = HMM_V4(0.3f, 0.3f, 0.3f, 1.0f),
        .wire = HMM_V4(0.3f, 0.6f, 0.3f, 1.0f),
        .hovered = HMM_V4(0.6f, 0.6f, 0.6f, 1.0f),
        .selected = HMM_V4(0.3f, 0.3f, 0.6f, 1.0f),
        .selectFill = HMM_V4(0.2f, 0.2f, 0.35f, 1.0f),
      },
  };
}

void view_init(CircuitView *view, const ComponentDesc *componentDescs) {
  *view = (CircuitView){
    .pan = HMM_V2(0.0f, 0.0f),
    .zoom = 1.0f,
    .hoveredComponent = NO_COMPONENT,
    .selectedPort = NO_PORT,
    .hoveredPort = NO_PORT,
  };
  circuit_init(&view->circuit, componentDescs);
  theme_init(&view->theme);
}

void view_free(CircuitView *view) {
  arrfree(view->components);
  arrfree(view->ports);
  arrfree(view->nets);
  arrfree(view->vertices);
  arrfree(view->selectedComponents);
  circuit_free(&view->circuit);
}

ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = circuit_add_component(&view->circuit, descID);

  Component *component = &view->circuit.components[id];
  const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

  ComponentView componentViewTmp = {.box.center = position};
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
    fmaxf(numInputPorts, numOutputPorts) * view->theme.portSpacing +
    view->theme.portSpacing;
  float width = view->theme.componentWidth;

  componentView->box.halfSize = HMM_V2(width / 2, height / 2);

  // figure out the position of each port
  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc - height / 2;
  float rightY = rightInc - height / 2;
  float borderWidth = view->theme.borderWidth;

  for (int j = 0; j < desc->numPorts; j++) {
    PortView portView = (PortView){0};

    if (desc->ports[j].direction == PORT_IN) {
      portView.center = HMM_V2(-width / 2 + borderWidth / 2, leftY);
      leftY += leftInc;
    } else if (desc->ports[j].direction != PORT_IN) {
      portView.center = HMM_V2(width / 2 - borderWidth / 2, rightY);
      rightY += rightInc;
    }

    arrput(view->ports, portView);
  }

  return id;
}

NetID view_add_net(CircuitView *view, PortID portFrom, PortID portTo) {
  NetID id = circuit_add_net(&view->circuit, portFrom, portTo);
  NetView netView = {0};

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
  if (
    view->selectionBox.halfSize.X > 0.001f &&
    view->selectionBox.halfSize.Y > 0.001f) {
    HMM_Vec2 pos = panZoom(
      view, HMM_SubV2(view->selectionBox.center, view->selectionBox.halfSize));
    HMM_Vec2 size = zoom(view, HMM_MulV2F(view->selectionBox.halfSize, 2.0f));

    draw_filled_rect(ctx, pos, size, 0, view->theme.color.selected);
  }

  for (int i = 0; i < arrlen(view->components); i++) {
    ComponentView *componentView = &view->components[i];
    Component *component = &view->circuit.components[i];
    const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

    HMM_Vec2 pos = panZoom(
      view, HMM_SubV2(componentView->box.center, componentView->box.halfSize));
    HMM_Vec2 size = zoom(view, HMM_MulV2F(componentView->box.halfSize, 2.0f));

    if (i == view->hoveredComponent) {
      draw_filled_rect(
        ctx,
        HMM_Sub(
          pos, HMM_V2(
                 view->theme.borderWidth * view->zoom * 2.0f,
                 view->theme.borderWidth * view->zoom * 2.0f)),
        HMM_Add(
          size, HMM_V2(
                  view->theme.borderWidth * view->zoom * 4.0f,
                  view->theme.borderWidth * view->zoom * 4.0f)),
        view->zoom * view->theme.componentRadius, view->theme.color.hovered);
    }

    bool isSelected = false;
    for (int j = 0; j < arrlen(view->selectedComponents); j++) {
      if (view->selectedComponents[j] == i) {
        isSelected = true;
        break;
      }
    }

    draw_filled_rect(
      ctx, pos, size, view->zoom * view->theme.componentRadius,
      isSelected ? view->theme.color.selected : view->theme.color.component);
    draw_stroked_rect(
      ctx, pos, size, view->zoom * view->theme.componentRadius,
      view->zoom * view->theme.borderWidth, view->theme.color.componentBorder);

    for (int j = 0; j < desc->numPorts; j++) {
      PortView *portView = &view->ports[component->portStart + j];

      float portWidth = view->theme.portWidth;
      HMM_Vec2 portPosition = panZoom(
        view, HMM_Sub(
                HMM_Add(componentView->box.center, portView->center),
                HMM_V2(portWidth / 2.0f, portWidth / 2.0f)));
      HMM_Vec2 portSize = zoom(view, HMM_V2(portWidth, portWidth));

      if (component->portStart + j == view->hoveredPort) {
        draw_filled_circle(
          ctx,
          HMM_Sub(
            portPosition, HMM_V2(
                            view->theme.borderWidth * view->zoom * 2.0f,
                            view->theme.borderWidth * view->zoom * 2.0f)),
          HMM_Add(
            portSize, HMM_V2(
                        view->theme.borderWidth * view->zoom * 4.0f,
                        view->theme.borderWidth * view->zoom * 4.0f)),
          view->theme.color.hovered);
      }

      draw_filled_circle(ctx, portPosition, portSize, view->theme.color.port);
      draw_stroked_circle(
        ctx, portPosition, portSize, view->zoom * view->theme.borderWidth,
        view->theme.color.portBorder);
    }
  }

  for (int i = 0; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];
    Net *net = &view->circuit.nets[i];

    Port *portFrom = &view->circuit.ports[net->portFrom];
    PortView *portViewFrom = &view->ports[net->portFrom];
    ComponentView *componentViewFrom = &view->components[portFrom->component];

    HMM_Vec2 portFromPosition = panZoom(
      view, HMM_Add(componentViewFrom->box.center, portViewFrom->center));

    Port *portTo = &view->circuit.ports[net->portTo];
    PortView *portViewTo = &view->ports[net->portTo];
    ComponentView *componentViewTo = &view->components[portTo->component];

    HMM_Vec2 portToPosition =
      panZoom(view, HMM_Add(componentViewTo->box.center, portViewTo->center));

    HMM_Vec2 pos = portFromPosition;

    for (int i = netView->vertexStart; i < netView->vertexEnd; i++) {
      HMM_Vec2 vertex = panZoom(view, view->vertices[i]);
      draw_stroked_line(
        ctx, pos, vertex, view->zoom * view->theme.wireThickness,
        view->theme.color.wire);
      pos = vertex;
    }

    draw_stroked_line(
      ctx, pos, portToPosition, view->zoom * view->theme.wireThickness,
      view->theme.color.wire);
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
  assert(netView->vertexEnd > netView->vertexStart);
  assert(netView->vertexEnd <= arrlen(view->vertices));
  netView->vertexEnd--;
  arrdel(view->vertices, netView->vertexEnd);
  for (int i = net + 1; i < arrlen(view->nets); i++) {
    NetView *netView = &view->nets[i];
    netView->vertexStart--;
    netView->vertexEnd--;
  }
}

void view_set_vertex(
  CircuitView *view, NetID net, VertexID index, HMM_Vec2 pos) {
  NetView *netView = &view->nets[net];
  view->vertices[netView->vertexStart + index] = pos;
}
