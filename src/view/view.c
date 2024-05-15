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
#include "render/draw.h"
#include "stb_ds.h"

#include "core/core.h"
#include "view.h"

#include <assert.h>

void theme_init(Theme *theme, FontHandle font) {
  *theme = (Theme){
    .portSpacing = 20.0f,
    .componentWidth = 55.0f,
    .portWidth = 7.0f,
    .borderWidth = 1.0f,
    .componentRadius = 5.0f,
    .wireThickness = 2.0f,
    .gateThickness = 3.0f,
    .font = font,
    .labelPadding = 2.0f,
    .labelFontSize = 12.0f,
    .color =
      {
        .component = HMM_V4(0.5f, 0.5f, 0.5f, 1.0f),
        .componentBorder = HMM_V4(0.8f, 0.8f, 0.8f, 1.0f),
        .port = HMM_V4(0.3f, 0.6f, 0.3f, 1.0f),
        .portBorder = HMM_V4(0.3f, 0.3f, 0.3f, 1.0f),
        .wire = HMM_V4(0.3f, 0.6f, 0.3f, 1.0f),
        .hovered = HMM_V4(0.6f, 0.6f, 0.6f, 1.0f),
        .selected = HMM_V4(0.3f, 0.3f, 0.6f, 1.0f),
        .selectFill = HMM_V4(0.2f, 0.2f, 0.35f, 1.0f),
        .labelColor = HMM_V4(0.0f, 0.0f, 0.0f, 1.0f),
        .nameColor = HMM_V4(0.8f, 0.8f, 0.8f, 1.0f),
      },
  };
}

void view_init(
  CircuitView *view, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *view = (CircuitView){
    .drawCtx = drawCtx,
    .hovered = NO_ID,
    .selectedPort = NO_PORT,
    .hoveredPort = NO_PORT,
  };
  circuit_init(&view->circuit, componentDescs);
  smap_add_synced_array(
    &view->circuit.sm.components, (void **)&view->components,
    sizeof(*view->components));
  smap_add_synced_array(
    &view->circuit.sm.ports, (void **)&view->ports, sizeof(*view->ports));
  smap_add_synced_array(
    &view->circuit.sm.nets, (void **)&view->nets, sizeof(*view->nets));
  smap_add_synced_array(
    &view->circuit.sm.endpoints, (void **)&view->endpoints,
    sizeof(*view->endpoints));
  smap_add_synced_array(
    &view->circuit.sm.waypoints, (void **)&view->waypoints,
    sizeof(*view->waypoints));
  smap_add_synced_array(
    &view->circuit.sm.labels, (void **)&view->labels, sizeof(*view->labels));

  theme_init(&view->theme, font);
}

void view_free(CircuitView *view) {
  arrfree(view->wires);
  arrfree(view->vertices);
  arrfree(view->selected);
  circuit_free(&view->circuit);
}

void view_augment_label(CircuitView *view, LabelID id, Box bounds) {
  int index = circuit_label_index(&view->circuit, id);
  LabelView labelView = {.bounds = bounds};
  view->labels[index] = labelView;
}

ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = circuit_add_component(&view->circuit, descID);

  Component *component = circuit_component_ptr(&view->circuit, id);
  const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

  ComponentView *componentView = view_component_ptr(view, id);
  *componentView = (ComponentView){.box.center = position};

  float labelPadding = view->theme.labelPadding;
  float width = view->theme.componentWidth;

  // figure out the size of the component
  int numInputPorts = 0;
  int numOutputPorts = 0;
  PortID portID = component->portFirst;
  for (int j = 0; j < desc->numPorts; j++) {
    if (desc->ports[j].direction == PORT_IN) {
      numInputPorts++;
    } else if (desc->ports[j].direction != PORT_IN) {
      numOutputPorts++;
    }

    // figure out the width needed for the label of the port and adjust width if
    // it's too small
    Port *port = circuit_port_ptr(&view->circuit, portID);
    LabelID labelID = port->label;
    const char *labelText = circuit_label_text(&view->circuit, labelID);
    Box labelBounds = draw_text_bounds(
      view->drawCtx, HMM_V2(0, 0), labelText, strlen(labelText), ALIGN_CENTER,
      ALIGN_MIDDLE, view->theme.labelFontSize, view->theme.font);
    float desiredHalfWidth =
      labelBounds.halfSize.X * 2 + labelPadding * 3 + view->theme.portWidth / 2;
    if (desiredHalfWidth > width / 2) {
      width = desiredHalfWidth * 2;
    }

    portID = port->compNext;
  }
  float height =
    fmaxf(numInputPorts, numOutputPorts) * view->theme.portSpacing +
    view->theme.portSpacing;

  LabelID typeLabelID = component->typeLabel;
  const char *typeLabelText = circuit_label_text(&view->circuit, typeLabelID);
  Box typeLabelBounds = draw_text_bounds(
    view->drawCtx, HMM_V2(0, -(height / 2) + labelPadding), typeLabelText,
    strlen(typeLabelText), ALIGN_CENTER, ALIGN_TOP, view->theme.labelFontSize,
    view->theme.font);
  if ((typeLabelBounds.halfSize.X + labelPadding) > width / 2) {
    width = typeLabelBounds.halfSize.X * 2 + labelPadding * 2;
  }
  view_augment_label(view, typeLabelID, typeLabelBounds);

  // kludge to make the name label appear at the right place on gate shapes
  float nameY = -(height / 2) + labelPadding;
  if (desc->shape != SHAPE_DEFAULT) {
    nameY += height / 5;
  }

  LabelID nameLabelID = component->nameLabel;
  const char *nameLabelText = circuit_label_text(&view->circuit, nameLabelID);
  Box nameLabelBounds = draw_text_bounds(
    view->drawCtx, HMM_V2(0, nameY), nameLabelText, strlen(nameLabelText),
    ALIGN_CENTER, ALIGN_BOTTOM, view->theme.labelFontSize, view->theme.font);
  view_augment_label(view, nameLabelID, nameLabelBounds);

  componentView->box.halfSize = HMM_V2(width / 2, height / 2);

  // figure out the position of each port
  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc - height / 2;
  float rightY = rightInc - height / 2;
  float borderWidth = view->theme.borderWidth;

  portID = component->portFirst;
  for (int j = 0; j < desc->numPorts; j++) {
    PortView *portView = view_port_ptr(view, portID);
    *portView = (PortView){0};

    HMM_Vec2 labelPos = HMM_V2(0, 0);
    HorizAlign horz = ALIGN_CENTER;

    if (desc->ports[j].direction == PORT_IN) {
      portView->center = HMM_V2(-width / 2 + borderWidth / 2, leftY);
      leftY += leftInc;

      labelPos = HMM_V2(labelPadding + view->theme.portWidth / 2, 0);
      horz = ALIGN_LEFT;
    } else if (desc->ports[j].direction != PORT_IN) {
      portView->center = HMM_V2(width / 2 - borderWidth / 2, rightY);
      rightY += rightInc;

      labelPos = HMM_V2(-labelPadding - view->theme.portWidth / 2, 0);
      horz = ALIGN_RIGHT;
    }

    Port *port = circuit_port_ptr(&view->circuit, portID);
    LabelID labelID = port->label;
    const char *labelText = circuit_label_text(&view->circuit, labelID);
    Box labelBounds = draw_text_bounds(
      view->drawCtx, HMM_V2(labelPos.X, labelPos.Y), labelText,
      strlen(labelText), horz, ALIGN_MIDDLE, view->theme.labelFontSize,
      view->theme.font);
    view_augment_label(view, labelID, labelBounds);

    portID = port->compNext;
  }

  return id;
}

NetID view_add_net(CircuitView *view) {
  return circuit_add_net(&view->circuit);
}

EndpointID
view_add_endpoint(CircuitView *view, NetID net, PortID port, HMM_Vec2 pos) {
  EndpointID id = circuit_add_endpoint(&view->circuit, net, port);
  EndpointView *endpointView = view_endpoint_ptr(view, id);

  HMM_Vec2 position = pos;
  if (port != NO_PORT) {
    PortView *portView = view_port_ptr(view, port);
    ComponentView *componentView = view_component_ptr(
      view, circuit_port_ptr(&view->circuit, port)->component);
    position = HMM_AddV2(componentView->box.center, portView->center);
  }

  *endpointView = (EndpointView){.pos = position};
  return id;
}

WaypointID view_add_waypoint(CircuitView *view, NetID net, HMM_Vec2 position) {
  WaypointID id = circuit_add_waypoint(&view->circuit, net);
  WaypointView *waypointView = view_waypoint_ptr(view, id);
  *waypointView = (WaypointView){.pos = position};
  return id;
}

void view_fix_wire_end_vertices(CircuitView *view, WireIndex wire) {
  // todo: implement
}

LabelID view_add_label(CircuitView *view, const char *text, Box bounds) {
  LabelID id = circuit_add_label(&view->circuit, text);
  LabelView *labelView = view_label_ptr(view, id);
  *labelView = (LabelView){.bounds = bounds};
  return id;
}

Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize) {
  Box bounds = draw_text_bounds(
    view->drawCtx, pos, text, strlen(text), horz, vert, fontSize,
    view->theme.font);
  return bounds;
}

const char *view_label_text(CircuitView *view, LabelID id) {
  return circuit_label_text(&view->circuit, id);
}

// mainly for tests
void view_direct_wire_nets(CircuitView *view) {
  arrsetlen(view->wires, 0);
  arrsetlen(view->vertices, 0);
  int wireOffset = 0;
  int vertexOffset = 0;
  arr(HMM_Vec2) waypoints = NULL;
  for (int i = 0; i < circuit_net_len(&view->circuit); i++) {
    NetView *netView = &view->nets[i];
    Net *net = &view->circuit.nets[i];
    netView->wireCount = 0;
    netView->wireOffset = wireOffset;
    netView->vertexOffset = vertexOffset;

    arrsetlen(waypoints, 0);
    WaypointID waypointID = net->waypointFirst;
    while (waypointID != NO_WAYPOINT) {
      Waypoint *waypoint = circuit_waypoint_ptr(&view->circuit, waypointID);
      WaypointView *waypointView = view_waypoint_ptr(view, waypointID);
      arrput(waypoints, waypointView->pos);
      waypointID = waypoint->next;
    }

    HMM_Vec2 centroid = HMM_V2(0, 0);
    int endpointCount = 0;
    EndpointID endpointID = net->endpointFirst;
    while (endpointID != NO_ENDPOINT) {
      Endpoint *endpoint = circuit_endpoint_ptr(&view->circuit, endpointID);
      endpointCount++;
      EndpointView *endpointView = view_endpoint_ptr(view, endpointID);
      centroid = HMM_AddV2(centroid, endpointView->pos);
      endpointID = endpoint->next;
    }
    if (endpointCount > 0) {
      centroid = HMM_DivV2F(centroid, (float)endpointCount);
    }

    // make sure there's at least one waypoint to wire things to
    if (arrlen(waypoints) == 0 && endpointCount > 2) {
      arrput(waypoints, centroid);
    }

    // wire waypoints together
    if (arrlen(waypoints) > 1) {
      Wire wire = {
        .vertexCount = arrlen(waypoints),
      };
      arrput(view->wires, wire);
      wireOffset++;
      netView->wireCount++;
      for (int j = 0; j < arrlen(waypoints); j++) {
        // add the vertices
        arrput(view->vertices, waypoints[j]);
        vertexOffset++;
      }
    }

    if (endpointCount <= 2 && endpointCount > 0) {
      Wire wire = {
        .vertexCount = endpointCount,
      };
      arrput(view->wires, wire);
      wireOffset++;
      netView->wireCount++;
    }

    endpointID = net->endpointFirst;
    while (endpointID != NO_ENDPOINT) {
      Endpoint *endpoint = circuit_endpoint_ptr(&view->circuit, endpointID);
      EndpointView *endpointView = view_endpoint_ptr(view, endpointID);

      Port *port = circuit_port_ptr(&view->circuit, endpoint->port);
      PortView *portView = view_port_ptr(view, endpoint->port);
      ComponentView *componentView = view_component_ptr(view, port->component);
      HMM_Vec2 pos = HMM_AddV2(componentView->box.center, portView->center);

      endpointView->pos = pos;

      if (endpointCount > 2) {
        // find the closest waypoint
        HMM_Vec2 waypoint = waypoints[0];
        float bestDist = HMM_LenSqrV2(HMM_SubV2(pos, waypoint));
        for (int j = 1; j < arrlen(waypoints); j++) {
          float dist = HMM_LenSqrV2(HMM_SubV2(pos, waypoints[j]));
          if (dist < bestDist) {
            waypoint = waypoints[j];
            bestDist = dist;
          }
        }

        // add the wire
        Wire wire = {
          .vertexCount = 2,
        };
        arrput(view->wires, wire);
        wireOffset++;
        netView->wireCount++;
        arrput(view->vertices, waypoint);
        vertexOffset++;
      }

      arrput(view->vertices, pos);
      vertexOffset++;

      endpointID = endpoint->next;
    }
  }
}

void view_draw(CircuitView *view) {
  if (
    view->selectionBox.halfSize.X > 0.001f &&
    view->selectionBox.halfSize.Y > 0.001f) {
    draw_selection_box(view->drawCtx, &view->theme, view->selectionBox, 0);
  }

  for (int i = 0; i < circuit_component_len(&view->circuit); i++) {
    ComponentID id = circuit_component_id(&view->circuit, i);
    ComponentView *componentView = &view->components[i];
    Component *component = &view->circuit.components[i];
    const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];
    HMM_Vec2 center = componentView->box.center;

    DrawFlags flags = 0;

    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        flags |= DRAW_SELECTED;
        break;
      }
    }

    if (id == view->hovered) {
      flags |= DRAW_HOVERED;
    }

    draw_component_shape(
      view->drawCtx, &view->theme, componentView->box, desc->shape, flags);

    if (desc->shape == SHAPE_DEFAULT) {
      LabelView *typeLabel = view_label_ptr(view, component->typeLabel);
      const char *typeLabelText =
        circuit_label_text(&view->circuit, component->typeLabel);
      draw_label(
        view->drawCtx, &view->theme, box_translate(typeLabel->bounds, center),
        typeLabelText, LABEL_COMPONENT_TYPE, 0);
    }

    LabelView *nameLabel = view_label_ptr(view, component->nameLabel);
    const char *nameLabelText =
      circuit_label_text(&view->circuit, component->nameLabel);
    draw_label(
      view->drawCtx, &view->theme, box_translate(nameLabel->bounds, center),
      nameLabelText, LABEL_COMPONENT_NAME, 0);

    PortID portID = component->portFirst;
    while (portID != NO_PORT) {
      PortView *portView = view_port_ptr(view, portID);
      Port *port = circuit_port_ptr(&view->circuit, portID);

      HMM_Vec2 portPosition =
        HMM_AddV2(componentView->box.center, portView->center);

      DrawFlags portFlags = 0;
      if (portID == view->hoveredPort) {
        portFlags |= DRAW_HOVERED;
      }
      draw_port(view->drawCtx, &view->theme, portPosition, portFlags);

      if (desc->shape == SHAPE_DEFAULT) {
        LabelView *labelView = view_label_ptr(view, port->label);
        const char *labelText = circuit_label_text(&view->circuit, port->label);

        Box labelBounds =
          box_translate(labelView->bounds, HMM_AddV2(center, portView->center));
        draw_label(
          view->drawCtx, &view->theme, labelBounds, labelText, LABEL_PORT,
          portFlags);
      }

      portID = port->compNext;
    }
  }

  for (int netIdx = 0; netIdx < circuit_net_len(&view->circuit); netIdx++) {
    NetView *netView = &view->nets[netIdx];

    VertexIndex vertexOffset = netView->vertexOffset;
    assert(vertexOffset < arrlen(view->vertices));

    for (int wireIdx = netView->wireOffset;
         wireIdx < netView->wireOffset + netView->wireCount; wireIdx++) {
      assert(wireIdx < arrlen(view->wires));
      Wire *wire = &view->wires[wireIdx];

      if (wireIdx != netView->wireOffset) {
        draw_junction(
          view->drawCtx, &view->theme, view->vertices[vertexOffset], 0);
      }
      DrawFlags flags = 0;
      if (wireIdx == netView->wireOffset && view->debugMode) {
        flags |= DRAW_DEBUG;
      }

      draw_wire(
        view->drawCtx, &view->theme, view->vertices + vertexOffset,
        wire->vertexCount, flags);
      vertexOffset += wire->vertexCount;
    }
  }
  for (int i = 0; i < circuit_waypoint_len(&view->circuit); i++) {
    WaypointView *waypointView = &view->waypoints[i];
    WaypointID id = circuit_waypoint_id(&view->circuit, i);
    DrawFlags flags = 0;

    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        flags |= DRAW_SELECTED;
        break;
      }
    }

    if (id == view->hovered) {
      flags |= DRAW_HOVERED;
    }

    draw_waypoint(view->drawCtx, &view->theme, waypointView->pos, flags);
  }
}
