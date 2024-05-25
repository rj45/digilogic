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
    .betterRoutes = true,
  };
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  bv_clear_all(ux->input.keysPressed);

  view_init(&ux->view, componentDescs, drawCtx, font);
  bvh_init(&ux->bvh);

  ux->router = autoroute_create(&ux->view.circuit);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  bv_free(ux->input.keysPressed);
  arrfree(ux->undoStack);
  arrfree(ux->redoStack);
  autoroute_free(ux->router);
}

HMM_Vec2 ux_calc_selection_center(CircuitUX *ux) {
  HMM_Vec2 center = HMM_V2(0, 0);
  assert(arrlen(ux->view.selected) > 0);
  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    ID id = ux->view.selected[i];
    if (id_type(id) == ID_COMPONENT) {
      Component *component = circuit_component_ptr(&ux->view.circuit, id);
      center = HMM_AddV2(center, component->box.center);
    } else if (id_type(id) == ID_WAYPOINT) {
      Waypoint *waypoint = circuit_waypoint_ptr(&ux->view.circuit, id);
      center = HMM_AddV2(center, waypoint->position);
    }
  }
  center = HMM_DivV2F(center, (float)arrlen(ux->view.selected));
  return center;
}

void ux_route(CircuitUX *ux) { autoroute_route(ux->router, ux->betterRoutes); }

void ux_select_none(CircuitUX *ux) {
  if (HMM_LenSqrV2(ux->view.selectionBox.halfSize) > 0.001f) {
    ux_do(
      ux, (UndoCommand){
            .verb = UNDO_DESELECT_AREA,
            .area = ux->view.selectionBox,
          });
  } else {
    while (arrlen(ux->view.selected) > 0) {
      ux_do(
        ux, (UndoCommand){
              .verb = UNDO_DESELECT_ITEM,
              .itemID = ux->view.selected[arrlen(ux->view.selected) - 1],
            });
    }
  }
}

void ux_select_all(CircuitUX *ux) {
  HMM_Vec2 min = HMM_V2(FLT_MAX, FLT_MAX);
  HMM_Vec2 max = HMM_V2(-FLT_MAX, -FLT_MAX);
  for (size_t i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
    Component *component = &ux->view.circuit.components[i];
    HMM_Vec2 cmin = box_top_left(component->box);
    HMM_Vec2 cmax = box_bottom_right(component->box);
    min.X = HMM_MIN(min.X, cmin.X);
    min.Y = HMM_MIN(min.Y, cmin.Y);
    max.X = HMM_MAX(max.X, cmax.X);
    max.Y = HMM_MAX(max.Y, cmax.Y);
  }
  for (size_t i = 0; i < circuit_waypoint_len(&ux->view.circuit); i++) {
    Waypoint *waypoint = &ux->view.circuit.waypoints[i];
    HMM_Vec2 cmin = waypoint->position;
    HMM_Vec2 cmax = waypoint->position;
    min.X = HMM_MIN(min.X, cmin.X);
    min.Y = HMM_MIN(min.Y, cmin.Y);
    max.X = HMM_MAX(max.X, cmax.X);
    max.Y = HMM_MAX(max.Y, cmax.Y);
  }
  ux_do(
    ux, (UndoCommand){
          .verb = UNDO_SELECT_AREA,
          .area = box_from_tlbr(min, max),
        });
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
  for (int i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
    Component *component = &ux->view.circuit.components[i];
    bvh_add(
      &ux->bvh, circuit_component_id(&ux->view.circuit, i), component->box);
  }
  HMM_Vec2 portHalfSize =
    HMM_V2(ux->view.theme.portWidth / 2, ux->view.theme.portWidth / 2);
  for (int i = 0; i < circuit_port_len(&ux->view.circuit); i++) {
    Port *port = &ux->view.circuit.ports[i];
    Component *component =
      circuit_component_ptr(&ux->view.circuit, port->component);
    bvh_add(
      &ux->bvh, circuit_port_id(&ux->view.circuit, i),
      (Box){HMM_AddV2(component->box.center, port->position), portHalfSize});
  }

  for (int i = 0; i < circuit_endpoint_len(&ux->view.circuit); i++) {
    Endpoint *endpoint = &ux->view.circuit.endpoints[i];
    bvh_add(
      &ux->bvh, circuit_endpoint_id(&ux->view.circuit, i),
      (Box){endpoint->position, portHalfSize});
  }

  for (int i = 0; i < circuit_waypoint_len(&ux->view.circuit); i++) {
    Waypoint *waypoint = &ux->view.circuit.waypoints[i];
    bvh_add(
      &ux->bvh, circuit_waypoint_id(&ux->view.circuit, i),
      (Box){waypoint->position, portHalfSize});
  }

  for (int netIdx = 0; netIdx < circuit_net_len(&ux->view.circuit); netIdx++) {
    Net *net = &ux->view.circuit.nets[netIdx];

    VertexIndex vertexOffset = net->vertexOffset;
    assert(vertexOffset < arrlen(ux->view.circuit.vertices));

    for (int wireIdx = net->wireOffset;
         wireIdx < net->wireOffset + net->wireCount; wireIdx++) {
      assert(wireIdx < arrlen(ux->view.circuit.wires));
      Wire *wire = &ux->view.circuit.wires[wireIdx];

      for (int vertIdx = 1; vertIdx < wire->vertexCount; vertIdx++) {
        HMM_Vec2 p1 = ux->view.circuit.vertices[vertexOffset + vertIdx - 1];
        HMM_Vec2 p2 = ux->view.circuit.vertices[vertexOffset + vertIdx];
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
        bvh_add(&ux->bvh, circuit_net_id(&ux->view.circuit, netIdx), box);
      }

      vertexOffset += wire->vertexCount;
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
