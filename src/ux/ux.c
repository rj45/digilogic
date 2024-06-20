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
#include <float.h>

#include "ux.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void ux_global_init() { autoroute_global_init(); }

void ux_init(
  CircuitUX *ux, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ux = (CircuitUX){
    .routingConfig.minimizeGraph = true,
    .routingConfig.performCentering = true,
  };
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  bv_clear_all(ux->input.keysPressed);

  view_init(&ux->view, componentDescs, drawCtx, font);
  bvh_init(&ux->bvh);

  ux->router = autoroute_create(&ux->view.circuit2);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  bv_free(ux->input.keysPressed);
  arrfree(ux->undoStack);
  arrfree(ux->redoStack);
  arrfree(ux->bvhQuery);
  autoroute_free(ux->router);
}

HMM_Vec2 ux_calc_selection_center(CircuitUX *ux) {
  HMM_Vec2 center = HMM_V2(0, 0);
  assert(arrlen(ux->view.selected) > 0);
  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    ID id = ux->view.selected[i];
    if (circ_has_component(&ux->view.circuit2, id, Position)) {
      Position position = circ_get(&ux->view.circuit2, id, Position);
      center = HMM_AddV2(center, position);
    }
  }
  center = HMM_DivV2F(center, (float)arrlen(ux->view.selected));
  return center;
}

void ux_route(CircuitUX *ux) { autoroute_route(ux->router, ux->routingConfig); }

void ux_select_none(CircuitUX *ux) {
  if (HMM_LenSqrV2(ux->view.selectionBox.halfSize) > 0.001f) {
    ux_do(ux, undo_cmd_deselect_area(ux->view.selectionBox));
  } else {
    while (arrlen(ux->view.selected) > 0) {
      ux_do(
        ux, undo_cmd_deselect_item(
              ux->view.selected[arrlen(ux->view.selected) - 1]));
    }
  }
}

void ux_select_all(CircuitUX *ux) {
  HMM_Vec2 min = HMM_V2(FLT_MAX, FLT_MAX);
  HMM_Vec2 max = HMM_V2(-FLT_MAX, -FLT_MAX);
  CircuitIter it = circ_iter(&ux->view.circuit2, Symbol2);
  while (circ_iter_next(&it)) {
    Symbol2 *table = circ_iter_table(&it, Symbol2);
    for (size_t i = 0; i < table->length; i++) {
      Box box = circ_get_symbol_box(&ux->view.circuit2, table->id[i]);
      HMM_Vec2 cmin = HMM_SubV2(box.center, box.halfSize);
      HMM_Vec2 cmax = HMM_AddV2(box.center, box.halfSize);
      min.X = HMM_MIN(min.X, cmin.X);
      min.Y = HMM_MIN(min.Y, cmin.Y);
      max.X = HMM_MAX(max.X, cmax.X);
      max.Y = HMM_MAX(max.Y, cmax.Y);
    }
  }

  it = circ_iter(&ux->view.circuit2, Waypoint2);
  while (circ_iter_next(&it)) {
    Waypoint2 *table = circ_iter_table(&it, Waypoint2);
    for (size_t i = 0; i < table->length; i++) {
      HMM_Vec2 cmin = table->position[i];
      HMM_Vec2 cmax = table->position[i];
      min.X = HMM_MIN(min.X, cmin.X);
      min.Y = HMM_MIN(min.Y, cmin.Y);
      max.X = HMM_MAX(max.X, cmax.X);
      max.Y = HMM_MAX(max.Y, cmax.Y);
    }
  }

  ux_do(ux, undo_cmd_select_area(box_from_tlbr(min, max)));
}

void ux_delete_selected(CircuitUX *ux) {
  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    ID id = ux->view.selected[i];
    if (circ_type_for_id(&ux->view.circuit2, id) == TYPE_SYMBOL) {
      SymbolKindID kind = circ_get(&ux->view.circuit2, id, SymbolKindID);
      Position position = circ_get(&ux->view.circuit2, id, Position);
      ux_do(ux, undo_cmd_del_symbol(position, id, kind));
    } else if (circ_type_for_id(&ux->view.circuit2, id) == TYPE_WAYPOINT) {
      // SymbolKindID kind = circ_get(&ux->view.circuit2, id, SymbolKindID);
      // Position position = circ_get(&ux->view.circuit2, id, Position);
      // ux_do(
      //   ux, undo_cmd_del_waypoint(position, id, kind));
    }
  }
}

static void ux_bvh_draw(BVH *bvh, DrawContext *drawCtx, int drawLevel);

void ux_draw(CircuitUX *ux) {
  view_draw(&ux->view);

  if (ux->rtDebugLines) {
    autoroute_draw_debug_lines(ux->router, ux->view.drawCtx);
  }

  if (ux->bvhDebugLines) {
    ux_bvh_draw(&ux->bvh, ux->view.drawCtx, ux->bvhDebugLevel);
  }
}

void ux_build_bvh(CircuitUX *ux) {
  bvh_clear(&ux->bvh);

  HMM_Vec2 portHalfSize =
    HMM_V2(ux->view.theme.portWidth / 2, ux->view.theme.portWidth / 2);

  ID top = ux->view.circuit2.top;
  LinkedListIter topit = circ_lliter(&ux->view.circuit2, top);
  while (circ_lliter_next(&topit)) {
    ID symbolID = topit.current;
    SymbolKindID kindID = circ_get(&ux->view.circuit2, symbolID, SymbolKindID);
    Position symbolPos = circ_get(&ux->view.circuit2, symbolID, Position);
    Size size = circ_get(&ux->view.circuit2, kindID, Size);
    Box box = (Box){.center = symbolPos, .halfSize = HMM_MulV2F(size, 0.5f)};
    bvh_add(&ux->bvh, symbolID, NO_ID, box);

    LinkedListIter portit = circ_lliter(&ux->view.circuit2, kindID);
    while (circ_lliter_next(&portit)) {
      ID portID = portit.current;
      Position portPos = circ_get(&ux->view.circuit2, portID, Position);
      portPos = HMM_AddV2(symbolPos, portPos);
      Box portBox = (Box){.center = portPos, .halfSize = portHalfSize};
      bvh_add(&ux->bvh, symbolID, portID, portBox);
    }
  }

  NetlistID netlistID = circ_get(&ux->view.circuit2, top, NetlistID);
  LinkedListIter netit = circ_lliter(&ux->view.circuit2, netlistID);
  while (circ_lliter_next(&netit)) {
    ID netID = netit.current;
    LinkedListIter subnetit = circ_lliter(&ux->view.circuit2, netID);
    while (circ_lliter_next(&subnetit)) {
      ID subnetID = subnetit.current;

      LinkedListIter endpointit = circ_lliter(&ux->view.circuit2, subnetID);
      while (circ_lliter_next(&endpointit)) {
        ID endpointID = endpointit.current;
        Position endpointPos =
          circ_get(&ux->view.circuit2, endpointID, Position);
        Box endpointBox =
          (Box){.center = endpointPos, .halfSize = portHalfSize};
        bvh_add(&ux->bvh, endpointID, NO_ID, endpointBox);

        LinkedListIter waypointit = circ_lliter(&ux->view.circuit2, endpointID);
        while (circ_lliter_next(&waypointit)) {
          ID waypointID = waypointit.current;
          Position waypointPos =
            circ_get(&ux->view.circuit2, waypointID, Position);
          Box waypointBox =
            (Box){.center = waypointPos, .halfSize = portHalfSize};
          bvh_add(&ux->bvh, waypointID, NO_ID, waypointBox);
        }
      }
    }

    WireVertices wireVertices =
      circ_get(&ux->view.circuit2, netID, WireVertices);
    size_t vertexOffset = 0;
    for (size_t wireIdx = 0; wireIdx < wireVertices.wireCount; wireIdx++) {
      uint16_t vertCount =
        circuit_wire_vertex_count(wireVertices.wireVertexCounts[wireIdx]);
      for (size_t vertIdx = 1; vertIdx < vertCount; vertIdx++) {
        HMM_Vec2 p1 = wireVertices.vertices[vertexOffset + vertIdx - 1];
        HMM_Vec2 p2 = wireVertices.vertices[vertexOffset + vertIdx];
        Box box;
        if (p1.X == p2.X) {
          box = (Box){
            HMM_V2(p1.X, (p1.Y + p2.Y) / 2),
            HMM_V2(ux->view.theme.wireThickness / 2, HMM_ABS(p1.Y - p2.Y) / 2)};
        } else {
          box = (Box){
            HMM_V2((p1.X + p2.X) / 2, p1.Y),
            HMM_V2(HMM_ABS(p1.X - p2.X) / 2, ux->view.theme.wireThickness / 2)};
        }
        bvh_add(&ux->bvh, netID, id_make(0, 0, wireIdx), box);
      }
      vertexOffset += vertCount;
    }
  }

  log_debug("Added %td items to BVH", arrlen(ux->bvh.leaves));

  bvh_rebuild(&ux->bvh);
}

typedef void *Context;
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_stroked_rect(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color);

static const HMM_Vec4 bvhLevelColors[] = {
  {.R = 1, .G = 0.4, .B = 0.4, .A = 0.5},
  {.R = 0.4, .G = 1, .B = 0.4, .A = 0.5},
  {.R = 0.4, .G = 0.4, .B = 1, .A = 0.5},
  {.R = 1, .G = 1, .B = 0.4, .A = 0.5},
  {.R = 1, .G = 0.4, .B = 1, .A = 0.5},
  {.R = 0.4, .G = 1, .B = 1, .A = 0.5},
  {.R = 1, .G = 1, .B = 1, .A = 0.5},
};

static void ux_bvh_draw_node(
  BVH *bvh, DrawContext *drawCtx, int node, int level, int drawLevel) {
  int left = 2 * node + 1;
  int right = 2 * node + 2;
  if (node >= arrlen(bvh->nodeHeap)) {
    return;
  }

  BVHNode *bvhNode = &bvh->nodeHeap[node];

  if (level == drawLevel) {
    draw_stroked_rect(
      drawCtx, HMM_SubV2(bvhNode->box.center, bvhNode->box.halfSize),
      HMM_MulV2F(bvhNode->box.halfSize, 2), 0, 1,
      bvhLevelColors
        [level % (sizeof(bvhLevelColors) / sizeof(bvhLevelColors[0]))]);
  }

  ux_bvh_draw_node(bvh, drawCtx, left, level + 1, drawLevel);
  ux_bvh_draw_node(bvh, drawCtx, right, level + 1, drawLevel);
}

static void ux_bvh_draw(BVH *bvh, DrawContext *drawCtx, int drawLevel) {
  ux_bvh_draw_node(bvh, drawCtx, 0, 0, drawLevel);
}
