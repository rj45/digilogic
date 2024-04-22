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
    .portWidth = 15.0f,
    .borderWidth = 1.0f,
    .componentRadius = 5.0f,
    .wireThickness = 2.0f,
    .font = font,
    .labelPadding = 2.0f,
    .labelFontSize = 8.0f,
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
    .hoveredComponent = NO_COMPONENT,
    .selectedPort = NO_PORT,
    .hoveredPort = NO_PORT,
  };
  circuit_init(&view->circuit, componentDescs);
  theme_init(&view->theme, font);
}

void view_free(CircuitView *view) {
  arrfree(view->components);
  arrfree(view->ports);
  arrfree(view->nets);
  arrfree(view->vertices);
  arrfree(view->selectedComponents);
  circuit_free(&view->circuit);
}

void view_augment_label(CircuitView *view, LabelID id, Box bounds) {
  if (id >= arrlen(view->labels)) {
    arrsetlen(view->labels, id + 1);
  }
  LabelView labelView = {.bounds = bounds};
  view->labels[id] = labelView;
}

ComponentID view_add_component(
  CircuitView *view, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = circuit_add_component(&view->circuit, descID);

  Component *component = &view->circuit.components[id];
  const ComponentDesc *desc = &view->circuit.componentDescs[component->desc];

  ComponentView componentViewTmp = {.box.center = position};
  arrput(view->components, componentViewTmp);
  ComponentView *componentView = &view->components[id];

  float labelPadding = view->theme.labelPadding;
  float width = view->theme.componentWidth;

  // figure out the size of the component
  int numInputPorts = 0;
  int numOutputPorts = 0;
  for (int j = 0; j < desc->numPorts; j++) {
    if (desc->ports[j].direction == PORT_IN) {
      numInputPorts++;
    } else if (desc->ports[j].direction != PORT_IN) {
      numOutputPorts++;
    }

    // figure out the width needed for the label of the port and adjust width if
    // it's too small
    LabelID labelID = view->circuit.ports[component->portStart + j].label;
    const char *labelText = circuit_label_text(&view->circuit, labelID);
    Box labelBounds = draw_text_bounds(
      HMM_V2(0, 0), labelText, strlen(labelText), ALIGN_CENTER, ALIGN_MIDDLE,
      view->theme.labelFontSize, view->theme.font);
    float desiredHalfWidth =
      labelBounds.halfSize.X * 2 + labelPadding * 3 + view->theme.portWidth / 2;
    if (desiredHalfWidth > width / 2) {
      width = desiredHalfWidth * 2;
    }
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

  LabelID nameLabelID = component->nameLabel;
  const char *nameLabelText = circuit_label_text(&view->circuit, nameLabelID);
  Box nameLabelBounds = draw_text_bounds(
    HMM_V2(0, -(height / 2) - labelPadding), nameLabelText,
    strlen(nameLabelText), ALIGN_CENTER, ALIGN_BOTTOM,
    view->theme.labelFontSize, view->theme.font);
  view_augment_label(view, nameLabelID, nameLabelBounds);

  componentView->box.halfSize = HMM_V2(width / 2, height / 2);

  // figure out the position of each port
  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc - height / 2;
  float rightY = rightInc - height / 2;
  float borderWidth = view->theme.borderWidth;

  for (int j = 0; j < desc->numPorts; j++) {
    PortView portView = (PortView){0};

    HMM_Vec2 labelPos = HMM_V2(0, 0);
    HorizAlign horz = ALIGN_CENTER;

    if (desc->ports[j].direction == PORT_IN) {
      portView.center = HMM_V2(-width / 2 + borderWidth / 2, leftY);
      leftY += leftInc;

      labelPos = HMM_V2(labelPadding + view->theme.portWidth / 2, 0);
      horz = ALIGN_LEFT;
    } else if (desc->ports[j].direction != PORT_IN) {
      portView.center = HMM_V2(width / 2 - borderWidth / 2, rightY);
      rightY += rightInc;

      labelPos = HMM_V2(-labelPadding - view->theme.portWidth / 2, 0);
      horz = ALIGN_RIGHT;
    }

    LabelID labelID = view->circuit.ports[component->portStart + j].label;
    const char *labelText = circuit_label_text(&view->circuit, labelID);
    Box labelBounds = draw_text_bounds(
      HMM_V2(labelPos.X, labelPos.Y), labelText, strlen(labelText), horz,
      ALIGN_MIDDLE, view->theme.labelFontSize, view->theme.font);
    view_augment_label(view, labelID, labelBounds);

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

static Box transformBox(CircuitView *view, Box box) {
  HMM_Vec2 center = panZoom(view, box.center);
  HMM_Vec2 halfSize = zoom(view, box.halfSize);
  return (Box){.center = center, .halfSize = halfSize};
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

    HMM_Vec2 center = componentView->box.center;
    HMM_Vec2 pos =
      panZoom(view, HMM_SubV2(center, componentView->box.halfSize));
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

    LabelView *typeLabel = &view->labels[component->typeLabel];
    const char *typeLabelText =
      circuit_label_text(&view->circuit, component->typeLabel);
    Box typeLabelBounds =
      transformBox(view, box_translate(typeLabel->bounds, center));
    draw_text(
      ctx, typeLabelBounds, typeLabelText, strlen(typeLabelText),
      view->theme.labelFontSize * view->zoom, view->theme.font,
      view->theme.color.labelColor, HMM_V4(0, 0, 0, 0));

    LabelView *nameLabel = &view->labels[component->nameLabel];
    const char *nameLabelText =
      circuit_label_text(&view->circuit, component->nameLabel);
    Box nameLabelBounds =
      transformBox(view, box_translate(nameLabel->bounds, center));
    draw_text(
      ctx, nameLabelBounds, nameLabelText, strlen(nameLabelText),
      view->theme.labelFontSize * view->zoom, view->theme.font,
      view->theme.color.nameColor, HMM_V4(0, 0, 0, 0));

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

      LabelView *labelView =
        &view->labels[view->circuit.ports[component->portStart + j].label];
      const char *labelText = circuit_label_text(
        &view->circuit, view->circuit.ports[component->portStart + j].label);
      Box labelBounds = transformBox(
        view,
        box_translate(labelView->bounds, HMM_Add(center, portView->center)));
      draw_text(
        ctx, labelBounds, labelText, strlen(labelText),
        view->theme.labelFontSize * view->zoom, view->theme.font,
        view->theme.color.labelColor, HMM_V4(0, 0, 0, 0));
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

LabelID view_add_label(CircuitView *view, const char *text, Box bounds) {
  LabelID id = circuit_add_label(&view->circuit, text);
  LabelView labelView = {.bounds = bounds};
  arrput(view->labels, labelView);
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