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
    &view->circuit.sm.wires, (void **)&view->wires, sizeof(*view->wires));
  smap_add_synced_array(
    &view->circuit.sm.junctions, (void **)&view->junctions,
    sizeof(*view->junctions));
  smap_add_synced_array(
    &view->circuit.sm.labels, (void **)&view->labels, sizeof(*view->labels));

  theme_init(&view->theme, font);
}

void view_free(CircuitView *view) {
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

JunctionID view_add_junction(CircuitView *view, HMM_Vec2 position) {
  JunctionID id = circuit_add_junction(&view->circuit);
  JunctionView *junctionView = view_junction_ptr(view, id);
  *junctionView = (JunctionView){.pos = position};
  return id;
}

WireID view_add_wire(CircuitView *view, NetID net, ID from, ID to) {
  WireID id = circuit_add_wire(&view->circuit, net, from, to);
  WireView *wireView = view_wire_ptr(view, id);
  *wireView = (WireView){.vertexStart = 0, .vertexEnd = 0};
  view_fix_wire_end_vertices(view, id);
  return id;
}

void view_add_vertex(CircuitView *view, WireID wire, HMM_Vec2 vertex) {
  WireView *wireView = view_wire_ptr(view, wire);
  arrins(view->vertices, wireView->vertexEnd, vertex);
  wireView->vertexEnd++;
  for (int i = circuit_wire_index(&view->circuit, wire) + 1;
       i < circuit_wire_len(&view->circuit); i++) {
    WireView *wireView = &view->wires[i];
    wireView->vertexStart++;
    wireView->vertexEnd++;
  }
}

void view_rem_vertex(CircuitView *view, WireID wire) {
  WireView *wireView = view_wire_ptr(view, wire);
  assert(wireView->vertexEnd > wireView->vertexStart);
  assert(wireView->vertexEnd <= arrlen(view->vertices));
  wireView->vertexEnd--;
  arrdel(view->vertices, wireView->vertexEnd);
  for (int i = circuit_wire_index(&view->circuit, wire) + 1;
       i < circuit_wire_len(&view->circuit); i++) {
    WireView *wireView = &view->wires[i];
    wireView->vertexStart--;
    wireView->vertexEnd--;
  }
}

void view_set_vertex(
  CircuitView *view, WireID wire, VertexID index, HMM_Vec2 pos) {
  WireView *wireView = view_wire_ptr(view, wire);
  view->vertices[wireView->vertexStart + index] = pos;
}

void view_fix_wire_end_vertices(CircuitView *view, WireID wire) {
  WireView *wireView = view_wire_ptr(view, wire);
  Wire *wireData = circuit_wire_ptr(&view->circuit, wire);

  if ((wireView->vertexEnd - wireView->vertexStart) < 2) {
    return;
  }

  ID ends[2] = {wireData->from, wireData->to};
  VertexID vert[2] = {0, wireView->vertexEnd - wireView->vertexStart - 1};

  for (int i = 0; i < 2; i++) {
    switch (id_type(ends[i])) {
    case ID_NONE:
      break;
    case ID_PORT: {
      Port *port = circuit_port_ptr(&view->circuit, ends[i]);
      ComponentView *componentView = view_component_ptr(view, port->component);
      PortView *portView = view_port_ptr(view, ends[i]);
      HMM_Vec2 pt = HMM_AddV2(componentView->box.center, portView->center);
      view_set_vertex(view, wire, vert[i], pt);
      break;
    }
    case ID_JUNCTION: {
      JunctionView *junctionView = view_junction_ptr(view, ends[i]);
      view_set_vertex(view, wire, vert[i], junctionView->pos);
      break;
    }
    default:
      assert(0);
    }
  }
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

  for (int i = 0; i < circuit_wire_len(&view->circuit); i++) {
    WireView *wireView = &view->wires[i];

    draw_wire(
      view->drawCtx, &view->theme, view->vertices + wireView->vertexStart,
      wireView->vertexEnd - wireView->vertexStart, 0);
  }

  for (int i = 0; i < circuit_junction_len(&view->circuit); i++) {
    JunctionView *junctionView = &view->junctions[i];
    JunctionID id = circuit_junction_id(&view->circuit, i);

    DrawFlags juncFlags = 0;
    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        juncFlags |= DRAW_SELECTED;
        break;
      }
    }

    if (id == view->hovered) {
      juncFlags |= DRAW_HOVERED;
    }

    draw_junction(view->drawCtx, &view->theme, junctionView->pos, juncFlags);
  }
}
