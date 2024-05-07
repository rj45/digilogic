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
  CircuitView *view, const ComponentDesc *componentDescs, FontHandle font) {
  *view = (CircuitView){
    .pan = HMM_V2(0.0f, 0.0f),
    .zoom = 1.0f,
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
      HMM_V2(0, 0), labelText, strlen(labelText), ALIGN_CENTER, ALIGN_MIDDLE,
      view->theme.labelFontSize, view->theme.font);
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
    HMM_V2(0, -(height / 2) + labelPadding), typeLabelText,
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
    HMM_V2(0, nameY), nameLabelText, strlen(nameLabelText), ALIGN_CENTER,
    ALIGN_BOTTOM, view->theme.labelFontSize, view->theme.font);
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
      HMM_V2(labelPos.X, labelPos.Y), labelText, strlen(labelText), horz,
      ALIGN_MIDDLE, view->theme.labelFontSize, view->theme.font);
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
    pos, text, strlen(text), horz, vert, fontSize, view->theme.font);
  return bounds;
}

const char *view_label_text(CircuitView *view, LabelID id) {
  return circuit_label_text(&view->circuit, id);
}

static HMM_Vec2 panZoom(CircuitView *view, HMM_Vec2 position) {
  return HMM_AddV2(HMM_MulV2F(position, view->zoom), view->pan);
}

static HMM_Vec2 zoom(CircuitView *view, HMM_Vec2 size) {
  return HMM_MulV2F(size, view->zoom);
}

static Box transformBox(CircuitView *view, Box box) {
  HMM_Vec2 center = panZoom(view, box.center);
  HMM_Vec2 halfSize = zoom(view, box.halfSize);
  return (Box){.center = center, .halfSize = halfSize};
}

static void draw_chip(
  CircuitView *view, Context ctx, int index, bool isHovered, bool isSelected) {
  ComponentView *componentView = &view->components[index];
  Component *component = &view->circuit.components[index];
  HMM_Vec2 center = componentView->box.center;
  HMM_Vec2 pos = panZoom(view, HMM_SubV2(center, componentView->box.halfSize));
  HMM_Vec2 size = zoom(view, HMM_MulV2F(componentView->box.halfSize, 2.0f));

  if (isHovered) {
    draw_filled_rect(
      ctx,
      HMM_SubV2(
        pos, HMM_V2(
               view->theme.borderWidth * view->zoom * 2.0f,
               view->theme.borderWidth * view->zoom * 2.0f)),
      HMM_AddV2(
        size, HMM_V2(
                view->theme.borderWidth * view->zoom * 4.0f,
                view->theme.borderWidth * view->zoom * 4.0f)),
      view->zoom * view->theme.componentRadius, view->theme.color.hovered);
  }

  draw_filled_rect(
    ctx, pos, size, view->zoom * view->theme.componentRadius,
    isSelected ? view->theme.color.selected : view->theme.color.component);
  draw_stroked_rect(
    ctx, pos, size, view->zoom * view->theme.componentRadius,
    view->zoom * view->theme.borderWidth, view->theme.color.componentBorder);

  LabelView *typeLabel = view_label_ptr(view, component->typeLabel);
  const char *typeLabelText =
    circuit_label_text(&view->circuit, component->typeLabel);
  Box typeLabelBounds =
    transformBox(view, box_translate(typeLabel->bounds, center));
  draw_text(
    ctx, typeLabelBounds, typeLabelText, strlen(typeLabelText),
    view->theme.labelFontSize * view->zoom, view->theme.font,
    view->theme.color.labelColor, HMM_V4(0, 0, 0, 0));
}

typedef struct Symbol {
  const char *text;
  HMM_Vec2 offset;
  float scale;
} Symbol;

const Symbol symbolSolid[] = {
  [SHAPE_DEFAULT] = {.text = "", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_AND] = {.text = "\x01", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_OR] = {.text = "\x03", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_XOR] = {.text = "\x05", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_NOT] = {.text = "\x07", .offset = {.X = 0, .Y = 25.5}, .scale = 1.5},
};

const Symbol symbolOutline[] = {
  [SHAPE_DEFAULT] = {.text = "", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_AND] = {.text = "\x02", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_OR] = {.text = "\x04", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_XOR] = {.text = "\x06", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_NOT] = {.text = "\x08", .offset = {.X = 0, .Y = 25.5}, .scale = 1.5},
};

static void draw_symbol(
  Context ctx, CircuitView *view, Box box, HMM_Vec4 color, ShapeType shape,
  bool outline) {

  const Symbol symbol = outline ? symbolOutline[shape] : symbolSolid[shape];

  HMM_Vec2 center = panZoom(view, HMM_AddV2(box.center, symbol.offset));
  HMM_Vec2 hs = zoom(view, HMM_MulV2F(box.halfSize, symbol.scale));

  Box bounds = draw_text_bounds(
    center, symbol.text, 1, ALIGN_CENTER, ALIGN_MIDDLE, hs.Height * 2.0f,
    view->theme.font);

  draw_text(
    ctx, bounds, symbol.text, 1, hs.Height * 2.0f, view->theme.font, color,
    HMM_V4(0, 0, 0, 0));
}

static void draw_gate(
  CircuitView *view, Context ctx, int index, ShapeType shape, bool isHovered,
  bool isSelected) {
  ComponentView *componentView = &view->components[index];

  if (isHovered) {
    Box hoverBox = (Box){
      .center = HMM_AddV2(
        componentView->box.center,
        HMM_V2(view->theme.borderWidth * 1.0f, view->theme.borderWidth * 3.0f)),
      .halfSize = HMM_AddV2(
        componentView->box.halfSize,
        HMM_V2(
          view->theme.borderWidth * 4.0f, view->theme.borderWidth * 4.0f))};

    draw_symbol(ctx, view, hoverBox, view->theme.color.hovered, shape, false);
  }

  draw_symbol(
    ctx, view, componentView->box,
    isSelected ? view->theme.color.selected : view->theme.color.component,
    shape, false);

  draw_symbol(
    ctx, view, componentView->box, view->theme.color.componentBorder, shape,
    true);
}

void view_draw(CircuitView *view, Context ctx) {
  if (
    view->selectionBox.halfSize.X > 0.001f &&
    view->selectionBox.halfSize.Y > 0.001f) {
    HMM_Vec2 pos = panZoom(
      view, HMM_SubV2(view->selectionBox.center, view->selectionBox.halfSize));
    HMM_Vec2 size = zoom(view, HMM_MulV2F(view->selectionBox.halfSize, 2.0f));

    draw_filled_rect(ctx, pos, size, 0, view->theme.color.selectFill);
  }

  for (int i = 0; i < circuit_component_len(&view->circuit); i++) {
    ComponentID id = circuit_component_id(&view->circuit, i);
    ComponentView *componentView = &view->components[i];
    Component *component = &view->circuit.components[i];
    const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];
    HMM_Vec2 center = componentView->box.center;

    bool isSelected = false;
    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        isSelected = true;
        break;
      }
    }

    bool isHovered = id == view->hovered;

    switch (desc->shape) {
    case SHAPE_DEFAULT:
      draw_chip(view, ctx, i, isHovered, isSelected);
      break;
    default:
      draw_gate(view, ctx, i, desc->shape, isHovered, isSelected);
      break;
    }

    LabelView *nameLabel = view_label_ptr(view, component->nameLabel);
    const char *nameLabelText =
      circuit_label_text(&view->circuit, component->nameLabel);
    Box nameLabelBounds =
      transformBox(view, box_translate(nameLabel->bounds, center));
    draw_text(
      ctx, nameLabelBounds, nameLabelText, strlen(nameLabelText),
      view->theme.labelFontSize * view->zoom, view->theme.font,
      view->theme.color.nameColor, HMM_V4(0, 0, 0, 0));

    PortID portID = component->portFirst;
    while (portID != NO_PORT) {
      PortView *portView = view_port_ptr(view, portID);
      Port *port = circuit_port_ptr(&view->circuit, portID);

      float portWidth = view->theme.portWidth;
      HMM_Vec2 portPosition = panZoom(
        view, HMM_SubV2(
                HMM_AddV2(componentView->box.center, portView->center),
                HMM_V2(portWidth / 2.0f, portWidth / 2.0f)));
      HMM_Vec2 portSize = zoom(view, HMM_V2(portWidth, portWidth));

      if (portID == view->hoveredPort) {
        draw_filled_circle(
          ctx,
          HMM_SubV2(
            portPosition, HMM_V2(
                            view->theme.borderWidth * view->zoom * 2.0f,
                            view->theme.borderWidth * view->zoom * 2.0f)),
          HMM_AddV2(
            portSize, HMM_V2(
                        view->theme.borderWidth * view->zoom * 4.0f,
                        view->theme.borderWidth * view->zoom * 4.0f)),
          view->theme.color.hovered);
      }

      draw_filled_circle(ctx, portPosition, portSize, view->theme.color.port);
      draw_stroked_circle(
        ctx, portPosition, portSize, view->zoom * view->theme.borderWidth,
        view->theme.color.portBorder);

      if (desc->shape == SHAPE_DEFAULT) {
        LabelView *labelView = view_label_ptr(view, port->label);
        const char *labelText = circuit_label_text(&view->circuit, port->label);
        Box labelBounds = transformBox(
          view, box_translate(
                  labelView->bounds, HMM_AddV2(center, portView->center)));
        draw_text(
          ctx, labelBounds, labelText, strlen(labelText),
          view->theme.labelFontSize * view->zoom, view->theme.font,
          view->theme.color.labelColor, HMM_V4(0, 0, 0, 0));
      }

      portID = port->compNext;
    }
  }

  for (int i = 0; i < circuit_wire_len(&view->circuit); i++) {
    WireView *wireView = &view->wires[i];

    if ((wireView->vertexEnd - wireView->vertexStart) < 2) {
      continue;
    }

    HMM_Vec2 pos = panZoom(view, view->vertices[wireView->vertexStart]);

    for (int i = wireView->vertexStart + 1; i < wireView->vertexEnd; i++) {
      HMM_Vec2 vertex = panZoom(view, view->vertices[i]);
      draw_stroked_line(
        ctx, pos, vertex, view->zoom * view->theme.wireThickness,
        view->theme.color.wire);
      pos = vertex;
    }
  }

  for (int i = 0; i < circuit_junction_len(&view->circuit); i++) {
    JunctionView *junctionView = &view->junctions[i];
    JunctionID id = circuit_junction_id(&view->circuit, i);

    bool isSelected = false;
    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == id) {
        isSelected = true;
        break;
      }
    }

    bool isHovered = id == view->hovered;

    float factor = isSelected || isHovered ? 3.0f : 1.5f;

    Box box = transformBox(
      view, (Box){
              .center = junctionView->pos,
              .halfSize = HMM_V2(
                view->theme.wireThickness * factor,
                view->theme.wireThickness * factor)});

    draw_filled_circle(
      ctx, HMM_SubV2(box.center, box.halfSize), HMM_MulV2F(box.halfSize, 2.0f),
      isSelected ? view->theme.color.selected : view->theme.color.wire);
  }
}
