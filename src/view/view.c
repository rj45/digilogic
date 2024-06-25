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
#include "routing/routing.h"
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
  };
  circ_init(&view->circuit);

  theme_init(&view->theme, font);

  SymbolLayout layout = (SymbolLayout){
    .portSpacing = view->theme.portSpacing,
    .symbolWidth = view->theme.componentWidth,
    .borderWidth = view->theme.borderWidth,
    .labelPadding = view->theme.labelPadding,
    .user = view,
    .textSize = calcTextSize,
  };
  circ_load_symbol_descs(&view->circuit, &layout, componentDescs, COMP_COUNT);

  view->circuit.top = circ_add_module(&view->circuit);
}

void view_reset(CircuitView *view) {
  arrsetlen(view->selected, 0);
  arrsetlen(view->hovered, 0);
  view->selectionBox = (Box){0};
  draw_reset(view->drawCtx);
  circ_clear(&view->circuit);
}

void view_free(CircuitView *view) {
  arrfree(view->selected);
  arrfree(view->hovered);
  circ_free(&view->circuit);
}

Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize) {
  Box bounds = draw_text_bounds(
    view->drawCtx, pos, text, strlen(text), horz, vert, fontSize,
    view->theme.font);
  return bounds;
}

static bool view_is_hovered(CircuitView *view, ID id) {
  for (int i = 0; i < arrlen(view->hovered); i++) {
    if (view->hovered[i].item == id) {
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

  float labelPadding = view->theme.labelPadding;

  ID moduleID = view->circuit.top;
  LinkedListIter moduleit = circ_lliter(&view->circuit, moduleID);
  while (circ_lliter_next(&moduleit)) {
    ID symbolID = moduleit.current;
    Position symbolPos = circ_get(&view->circuit, symbolID, Position);
    SymbolKindID kindID = circ_get(&view->circuit, symbolID, SymbolKindID);
    Size size = circ_get(&view->circuit, kindID, Size);
    SymbolShape shape = circ_get(&view->circuit, kindID, SymbolShape);

    DrawFlags flags = 0;

    for (int j = 0; j < arrlen(view->selected); j++) {
      if (view->selected[j] == symbolID) {
        flags |= DRAW_SELECTED;
        break;
      }
    }

    if (view_is_hovered(view, symbolID)) {
      flags |= DRAW_HOVERED;
    }

    Box box = (Box){.center = symbolPos, .halfSize = HMM_MulV2F(size, 0.5f)};

    if (shape != SYMSHAPE_DEFAULT) {
      // TODO: move this hack elsewhere

      // newHeight = height - (height * 2.0f / 5.0f);
      // newHeight = (5/5)height - (2/5)height
      // newHeight = (3/5)height
      // newHeight / (3/5) = height
      // newHeight * 5 / 3 = height
      box.halfSize.Height = (box.halfSize.Height * 5.0f) / 3.0f;
    }

    draw_symbol_shape(view->drawCtx, &view->theme, box, shape, flags);

    if (shape == SYMSHAPE_DEFAULT) {
      Name typeLabel = circ_get(&view->circuit, kindID, Name);
      const char *typeLabelText = circ_str_get(&view->circuit, typeLabel);
      Box typeLabelBounds = draw_text_bounds(
        view->drawCtx, HMM_V2(0, -(size.Height / 2) + labelPadding),
        typeLabelText, strlen(typeLabelText), ALIGN_CENTER, ALIGN_TOP,
        view->theme.labelFontSize, view->theme.font);
      draw_label(
        view->drawCtx, &view->theme, box_translate(typeLabelBounds, symbolPos),
        typeLabelText, LABEL_COMPONENT_TYPE, 0);
    }

    Prefix namePrefix = circ_get(&view->circuit, kindID, Prefix);
    Number nameNumber = circ_get(&view->circuit, symbolID, Number);
    char nameLabelText[256];
    snprintf(
      nameLabelText, 256, "%s%d", circ_str_get(&view->circuit, namePrefix),
      nameNumber);

    Box nameLabelBounds = draw_text_bounds(
      view->drawCtx, HMM_V2(0, -(size.Height / 2) + labelPadding),
      nameLabelText, strlen(nameLabelText), ALIGN_CENTER, ALIGN_BOTTOM,
      view->theme.labelFontSize, view->theme.font);

    draw_label(
      view->drawCtx, &view->theme, box_translate(nameLabelBounds, symbolPos),
      nameLabelText, LABEL_COMPONENT_NAME, 0);

    LinkedListIter portit = circ_lliter(&view->circuit, kindID);
    while (circ_lliter_next(&portit)) {
      ID portID = portit.current;
      Position portPos = circ_get(&view->circuit, portID, Position);
      portPos = HMM_AddV2(symbolPos, portPos);

      DrawFlags portFlags = 0;

      if (view_is_hovered(view, portID)) {
        portFlags |= DRAW_HOVERED;
      }
      draw_port(view->drawCtx, &view->theme, portPos, portFlags);

      if (shape == SYMSHAPE_DEFAULT) {
        Name portLabel = circ_get(&view->circuit, portID, Name);
        const char *portLabelText = circ_str_get(&view->circuit, portLabel);

        HMM_Vec2 labelPos = HMM_V2(0, 0);
        HorizAlign horz = ALIGN_CENTER;

        if (circ_has_tags(&view->circuit, portID, TAG_IN)) {
          labelPos =
            HMM_V2((labelPadding * 2.0f) + view->theme.portWidth / 2, 0);
          horz = ALIGN_LEFT;
        } else if (!circ_has_tags(&view->circuit, portID, TAG_IN)) {
          labelPos = HMM_V2(-labelPadding - view->theme.portWidth / 2, 0);
          horz = ALIGN_RIGHT;
        }

        Box labelBounds = draw_text_bounds(
          view->drawCtx, labelPos, portLabelText, strlen(portLabelText), horz,
          ALIGN_MIDDLE, view->theme.labelFontSize, view->theme.font);

        draw_label(
          view->drawCtx, &view->theme, box_translate(labelBounds, portPos),
          portLabelText, LABEL_PORT, portFlags);
      }
    }
  }

  if (view->hideNets) {
    return;
  }

  NetlistID netlistID = circ_get(&view->circuit, view->circuit.top, NetlistID);
  LinkedListIter it = circ_lliter(&view->circuit, netlistID);
  while (circ_lliter_next(&it)) {
    ID netID = it.current;
    bool netIsHovered = view_is_hovered(view, netID);

    WireVertices wireVerts = circ_get(&view->circuit, netID, WireVertices);
    HMM_Vec2 *vertices = wireVerts.vertices;
    for (size_t j = 0; j < wireVerts.wireCount; j++) {
      uint16_t wireVertCount =
        RT_WireView_vertex_count(wireVerts.wireVertexCounts[j]);

      DrawFlags flags = 0;
      if (
        view->debugMode && RT_WireView_is_root(wireVerts.wireVertexCounts[j])) {
        flags |= DRAW_DEBUG;
      }
      if (netIsHovered) {
        flags |= DRAW_HOVERED;
      }

      draw_wire(view->drawCtx, &view->theme, vertices, wireVertCount, flags);

      if (RT_WireView_ends_in_junction(wireVerts.wireVertexCounts[j])) {
        draw_junction(
          view->drawCtx, &view->theme, vertices[wireVertCount - 1], flags);
      }
      vertices += wireVertCount;
    }

    LinkedListIter subnetit = circ_lliter(&view->circuit, netID);
    while (circ_lliter_next(&subnetit)) {
      LinkedListIter endpointit = circ_lliter(&view->circuit, subnetit.current);
      while (circ_lliter_next(&endpointit)) {
        LinkedListIter waypointit =
          circ_lliter(&view->circuit, endpointit.current);
        while (circ_lliter_next(&waypointit)) {
          ID waypointID = waypointit.current;
          DrawFlags flags = 0;
          Position waypointPos = circ_get(&view->circuit, waypointID, Position);
          if (view_is_hovered(view, waypointID)) {
            flags |= DRAW_HOVERED;
          }

          for (int j = 0; j < arrlen(view->selected); j++) {
            if (view->selected[j] == waypointID) {
              flags |= DRAW_SELECTED;
              break;
            }
          }

          if (netIsHovered || flags & DRAW_SELECTED) {
            draw_waypoint(view->drawCtx, &view->theme, waypointPos, flags);
          }
        }
      }
    }
  }
}
