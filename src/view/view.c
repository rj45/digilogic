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

#define LOG_LEVEL LL_DEBUG
#include "log.h"

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

void view_augment_label(CircuitView *view, LabelID id, Box bounds) {
  Label *label = circuit_label_ptr(&view->circuit, id);
  label->box = bounds;
  circuit_update_id(&view->circuit, id);
}

static void view_augment_component(void *user, ComponentID id, void *ptr) {
  CircuitView *view = user;
  Component *component = ptr;
  const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

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

    portID = port->next;
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

  component->box.halfSize = HMM_V2(width / 2, height / 2);

  // figure out the position of each port
  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc - height / 2;
  float rightY = rightInc - height / 2;
  float borderWidth = view->theme.borderWidth;

  portID = component->portFirst;
  for (int j = 0; j < desc->numPorts; j++) {
    Port *port = circuit_port_ptr(&view->circuit, portID);

    HMM_Vec2 labelPos = HMM_V2(0, 0);
    HorizAlign horz = ALIGN_CENTER;

    if (desc->ports[j].direction == PORT_IN) {
      port->position = HMM_V2(-width / 2 + borderWidth / 2, leftY);
      leftY += leftInc;

      labelPos = HMM_V2(labelPadding + view->theme.portWidth / 2, 0);
      horz = ALIGN_LEFT;
    } else if (desc->ports[j].direction != PORT_IN) {
      port->position = HMM_V2(width / 2 - borderWidth / 2, rightY);
      rightY += rightInc;

      labelPos = HMM_V2(-labelPadding - view->theme.portWidth / 2, 0);
      horz = ALIGN_RIGHT;
    }

    LabelID labelID = port->label;
    const char *labelText = circuit_label_text(&view->circuit, labelID);
    Box labelBounds = draw_text_bounds(
      view->drawCtx, HMM_V2(labelPos.X, labelPos.Y), labelText,
      strlen(labelText), horz, ALIGN_MIDDLE, view->theme.labelFontSize,
      view->theme.font);
    view_augment_label(view, labelID, labelBounds);

    portID = port->next;
  }
}

static void view_component_deleted(void *user, ComponentID id, void *ptr) {
  CircuitView *view = user;
  for (int i = 0; i < arrlen(view->selected); i++) {
    if (view->selected[i] == id) {
      arrdel(view->selected, i);
      break;
    }
  }
}

static void view_waypoint_deleted(void *user, WaypointID id, void *ptr) {
  CircuitView *view = user;
  for (int i = 0; i < arrlen(view->selected); i++) {
    if (view->selected[i] == id) {
      arrdel(view->selected, i);
      break;
    }
  }
}

static HMM_Vec2 calcTextSize(void *user, const char *text) {
  CircuitView *view = (CircuitView *)user;
  Theme *theme = &view->theme;
  Box box = draw_text_bounds(
    view->drawCtx, HMM_V2(0, 0), text, strlen(text), ALIGN_LEFT, ALIGN_TOP,
    theme->labelFontSize, theme->font);
  return HMM_V2(box.halfSize.X * 2, box.halfSize.Y * 2);
}

void view_init(
  CircuitView *view, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *view = (CircuitView){
    .drawCtx = drawCtx,
    .selectedPort = NO_PORT,
  };
  circuit_init(&view->circuit, componentDescs);
  circ_init(&view->circuit2);
  view->circuit2.oldCircuit = &view->circuit;

  circuit_on_component_create(&view->circuit, view, view_augment_component);
  circuit_on_component_delete(&view->circuit, view, view_component_deleted);
  circuit_on_waypoint_delete(&view->circuit, view, view_waypoint_deleted);

  theme_init(&view->theme, font);

  SymbolLayout layout = (SymbolLayout){
    .portSpacing = view->theme.portSpacing,
    .symbolWidth = view->theme.componentWidth,
    .borderWidth = view->theme.borderWidth,
    .labelPadding = view->theme.labelPadding,
    .user = view,
    .textSize = calcTextSize,
  };
  circ_load_symbol_descs(&view->circuit2, &layout, componentDescs, COMP_COUNT);

  view->circuit2.top = circ_add_module(&view->circuit2);
}

void view_free(CircuitView *view) {
  arrfree(view->selected);
  arrfree(view->hovered);
  circuit_free(&view->circuit);
}

Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize) {
  Box bounds = draw_text_bounds(
    view->drawCtx, pos, text, strlen(text), horz, vert, fontSize,
    view->theme.font);
  return bounds;
}

// mainly for tests
void view_direct_wire_nets(CircuitView *view) {
  arrsetlen(view->circuit.wires, 0);
  arrsetlen(view->circuit.vertices, 0);
  int wireOffset = 0;
  int vertexOffset = 0;
  arr(HMM_Vec2) waypoints = NULL;
  for (int i = 0; i < circuit_net_len(&view->circuit); i++) {
    Net *net = &view->circuit.nets[i];
    net->wireCount = 0;
    net->wireOffset = wireOffset;
    net->vertexOffset = vertexOffset;

    arrsetlen(waypoints, 0);
    WaypointID waypointID = net->waypointFirst;
    while (circuit_has(&view->circuit, waypointID)) {
      Waypoint *waypoint = circuit_waypoint_ptr(&view->circuit, waypointID);
      arrput(waypoints, waypoint->position);
      waypointID = waypoint->next;
    }

    HMM_Vec2 centroid = HMM_V2(0, 0);
    int endpointCount = 0;
    EndpointID endpointID = net->endpointFirst;
    while (circuit_has(&view->circuit, endpointID)) {
      Endpoint *endpoint = circuit_endpoint_ptr(&view->circuit, endpointID);
      endpointCount++;
      centroid = HMM_AddV2(centroid, endpoint->position);
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
      arrput(view->circuit.wires, wire);
      wireOffset++;
      net->wireCount++;
      for (int j = 0; j < arrlen(waypoints); j++) {
        // add the vertices
        arrput(view->circuit.vertices, waypoints[j]);
        vertexOffset++;
      }
    }

    if (endpointCount <= 2 && endpointCount > 0) {
      Wire wire = {
        .vertexCount = endpointCount,
      };
      arrput(view->circuit.wires, wire);
      wireOffset++;
      net->wireCount++;
    }

    endpointID = net->endpointFirst;
    while (circuit_has(&view->circuit, endpointID)) {
      Endpoint *endpoint = circuit_endpoint_ptr(&view->circuit, endpointID);

      Port *port = circuit_port_ptr(&view->circuit, endpoint->port);
      Component *component =
        circuit_component_ptr(&view->circuit, port->component);
      HMM_Vec2 pos = HMM_AddV2(component->box.center, port->position);

      endpoint->position = pos;

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
        arrput(view->circuit.wires, wire);
        wireOffset++;
        net->wireCount++;
        arrput(view->circuit.vertices, waypoint);
        vertexOffset++;
      }

      arrput(view->circuit.vertices, pos);
      vertexOffset++;

      endpointID = endpoint->next;
    }
  }
}

static bool view_is_hovered(CircuitView *view, ID id) {
  for (int i = 0; i < arrlen(view->hovered); i++) {
    if (view->hovered[i] == id) {
      return true;
    }
  }
  return false;
}

void view_draw(CircuitView *view) {
  if (
    view->selectionBox.halfSize.X > 0.001f &&
    view->selectionBox.halfSize.Y > 0.001f) {
    draw_selection_box(view->drawCtx, &view->theme, view->selectionBox, 0);
  }

  for (int i = 0; i < circuit_component_len(&view->circuit); i++) {
    ComponentID id = circuit_component_id(&view->circuit, i);
    Component *component = &view->circuit.components[i];
    const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];
    HMM_Vec2 center = component->box.center;

    DrawFlags flags = 0;

    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        flags |= DRAW_SELECTED;
        break;
      }
    }

    if (view_is_hovered(view, id)) {
      flags |= DRAW_HOVERED;
    }

    draw_component_shape(
      view->drawCtx, &view->theme, component->box, desc->shape, flags);

    if (desc->shape == SHAPE_DEFAULT) {
      Label *typeLabel =
        circuit_label_ptr(&view->circuit, component->typeLabel);
      const char *typeLabelText =
        circuit_label_text(&view->circuit, component->typeLabel);
      draw_label(
        view->drawCtx, &view->theme, box_translate(typeLabel->box, center),
        typeLabelText, LABEL_COMPONENT_TYPE, 0);
    }

    Label *nameLabel = circuit_label_ptr(&view->circuit, component->nameLabel);
    const char *nameLabelText =
      circuit_label_text(&view->circuit, component->nameLabel);
    draw_label(
      view->drawCtx, &view->theme, box_translate(nameLabel->box, center),
      nameLabelText, LABEL_COMPONENT_NAME, 0);

    PortID portID = component->portFirst;
    while (circuit_has(&view->circuit, portID)) {
      Port *port = circuit_port_ptr(&view->circuit, portID);

      HMM_Vec2 portPosition = HMM_AddV2(component->box.center, port->position);

      DrawFlags portFlags = 0;

      if (view_is_hovered(view, portID)) {
        portFlags |= DRAW_HOVERED;
      }
      draw_port(view->drawCtx, &view->theme, portPosition, portFlags);

      if (desc->shape == SHAPE_DEFAULT) {
        Label *label = circuit_label_ptr(&view->circuit, port->label);
        const char *labelText = circuit_label_text(&view->circuit, port->label);

        Box labelBounds =
          box_translate(label->box, HMM_AddV2(center, port->position));
        draw_label(
          view->drawCtx, &view->theme, labelBounds, labelText, LABEL_PORT,
          portFlags);
      }

      portID = port->next;
    }
  }

  for (int netIdx = 0; netIdx < circuit_net_len(&view->circuit); netIdx++) {
    Net *net = &view->circuit.nets[netIdx];

    bool netIsHovered =
      view_is_hovered(view, circuit_net_id(&view->circuit, netIdx));

    VertexIndex vertexOffset = net->vertexOffset;
    assert(vertexOffset < arrlen(view->circuit.vertices));

    for (int wireIdx = net->wireOffset;
         wireIdx < net->wireOffset + net->wireCount; wireIdx++) {
      assert(wireIdx < arrlen(view->circuit.wires));
      Wire *wire = &view->circuit.wires[wireIdx];

      DrawFlags flags = 0;
      if (wireIdx == net->wireOffset && view->debugMode) {
        flags |= DRAW_DEBUG;
      }
      if (netIsHovered) {
        flags |= DRAW_HOVERED;
      }

      draw_wire(
        view->drawCtx, &view->theme, view->circuit.vertices + vertexOffset,
        circuit_wire_vertex_count(wire->vertexCount), flags);

      if (circuit_wire_ends_in_junction(wire->vertexCount)) {
        draw_junction(
          view->drawCtx, &view->theme,
          view->circuit.vertices
            [vertexOffset + circuit_wire_vertex_count(wire->vertexCount) - 1],
          flags);
      }

      vertexOffset += circuit_wire_vertex_count(wire->vertexCount);
    }
  }
  for (int i = 0; i < circuit_waypoint_len(&view->circuit); i++) {
    Waypoint *waypoint = &view->circuit.waypoints[i];
    WaypointID id = circuit_waypoint_id(&view->circuit, i);
    DrawFlags flags = 0;

    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        flags |= DRAW_SELECTED;
        break;
      }
    }

    if (view_is_hovered(view, id)) {
      flags |= DRAW_HOVERED;
    }

    if (
      (flags & (DRAW_HOVERED | DRAW_SELECTED)) ||
      view_is_hovered(view, waypoint->net)) {
      draw_waypoint(view->drawCtx, &view->theme, waypoint->position, flags);
    }
  }
}
