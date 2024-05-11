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

#include "sokol_time.h"

#include "autoroute/autoroute.h"
#include "core/core.h"
#include "routing/routing.h"
#include "view/view.h"
#include <stdint.h>

#define LOG_LEVEL LL_INFO
#include "log.h"

#define RT_PADDING 10.0f

typedef struct AnchorEnds {
  uint32_t start;
  uint32_t end;
} AnchorEnds;

struct AutoRoute {
  CircuitView *view;

  RT_PathDef *paths;
  RT_BoundingBox *boxes;
  AnchorEnds *anchorEnds;
  RT_PathRange *pathRanges;

  arr(RT_Anchor) anchors;

  uint16_t threadCount;
  size_t vertBufferCapacity;
  RT_Graph *graph;

  arr(RT_VertexBuffer) vertexBuffers;
};

void autoroute_global_init() {
  RT_Result res = RT_init_thread_pool();
  assert(res == RT_RESULT_SUCCESS);
}

AutoRoute *autoroute_create(CircuitView *view) {
  AutoRoute *ar = malloc(sizeof(AutoRoute));
  *ar = (AutoRoute){
    .view = view,
  };
  smap_add_synced_array(
    &view->circuit.sm.components, (void **)&ar->boxes, sizeof(*ar->boxes));
  smap_add_synced_array(
    &view->circuit.sm.components, (void **)&ar->anchorEnds,
    sizeof(*ar->anchorEnds));
  smap_add_synced_array(
    &view->circuit.sm.wires, (void **)&ar->paths, sizeof(*ar->paths));
  smap_add_synced_array(
    &view->circuit.sm.wires, (void **)&ar->pathRanges, sizeof(*ar->pathRanges));

  RT_Result res = RT_get_thread_count(&ar->threadCount);
  assert(res == RT_RESULT_SUCCESS);

  res = RT_graph_new(&ar->graph);
  assert(res == RT_RESULT_SUCCESS);

  ar->vertBufferCapacity = 1024;

  for (size_t i = 0; i < ar->threadCount; i++) {
    arrput(
      ar->vertexBuffers,
      ((struct RT_VertexBuffer){
        .vertices = malloc(ar->vertBufferCapacity * sizeof(RT_Vertex)),
        .vertex_count = 0,
      }));
  }

  return ar;
}

void autoroute_free(AutoRoute *ar) {
  arrfree(ar->anchors);
  free(ar);
}

static void autoroute_update_anchors(AutoRoute *ar) {
  arrsetlen(ar->anchors, 0);
  for (int i = 0; i < circuit_component_len(&ar->view->circuit); i++) {
    Component *comp = &ar->view->circuit.components[i];
    ComponentView *compView = &ar->view->components[i];
    Box box = compView->box;

    float cx = box.center.X;
    float cy = box.center.Y;

    HMM_Vec2 halfSize = HMM_AddV2(box.halfSize, HMM_V2(RT_PADDING, RT_PADDING));
    RT_Anchor tl = (RT_Anchor){
      .position = {.x = cx - halfSize.X, .y = cy - halfSize.Y},
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    RT_Anchor tr = (RT_Anchor){
      .position = {.x = cx + halfSize.X, .y = cy - halfSize.Y},
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    RT_Anchor bl = (RT_Anchor){
      .position = {.x = cx - halfSize.X, .y = cy + halfSize.Y},
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    RT_Anchor br = (RT_Anchor){
      .position = {.x = cx + halfSize.X, .y = cy + halfSize.Y},
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };

    arrput(ar->anchors, tl);
    arrput(ar->anchors, tr);
    arrput(ar->anchors, bl);
    arrput(ar->anchors, br);

    PortID portID = comp->portFirst;
    while (portID != NO_PORT) {
      Port *port = circuit_port_ptr(&ar->view->circuit, portID);
      PortView *portView = view_port_ptr(ar->view, portID);

      PortDesc *portDesc =
        &ar->view->circuit.componentDescs[comp->desc].ports[port->desc];
      RT_Directions directions = portDesc->direction == PORT_IN
                                   ? RT_DIRECTIONS_NEG_X
                                   : RT_DIRECTIONS_POS_X;

      RT_Anchor anchor = (RT_Anchor){
        .bounding_box = i,
        .position =
          {
            .x = cx + portView->center.X,
            .y = cy + portView->center.Y,
          },
        .connect_directions = directions,
      };
      arrput(ar->anchors, anchor);

      portID = port->compNext;
    }
  }
  for (int i = 0; i < circuit_junction_len(&ar->view->circuit); i++) {
    JunctionView *junctionView = &ar->view->junctions[i];
    RT_Anchor anchor = (RT_Anchor){
      .position =
        {
          .x = junctionView->pos.X,
          .y = junctionView->pos.Y,
        },
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    arrput(ar->anchors, anchor);
  }
}

void autoroute_update_component(AutoRoute *ar, ID id) {
  ComponentView *compView = view_component_ptr(ar->view, id);
  RT_BoundingBox *box =
    &ar->boxes[circuit_component_index(&ar->view->circuit, id)];
  *box = (RT_BoundingBox){
    .center =
      {
        .x = compView->box.center.X,
        .y = compView->box.center.Y,
      },
    .half_width = (uint16_t)(compView->box.halfSize.X + RT_PADDING) - 1,
    .half_height = (uint16_t)(compView->box.halfSize.Y + RT_PADDING) - 1,
  };
  autoroute_update_anchors(ar);
}

void autoroute_update_wire(AutoRoute *ar, ID id) {
  Wire *wire = circuit_wire_ptr(&ar->view->circuit, id);
  RT_PathDef *path = &ar->paths[circuit_wire_index(&ar->view->circuit, id)];
  ID ends[2] = {wire->from, wire->to};
  RT_Point points[2];
  for (int i = 0; i < 2; i++) {
    switch (id_type(ends[i])) {
    case ID_NONE:
      break;
    case ID_PORT: {
      PortView *portView = view_port_ptr(ar->view, ends[i]);
      Port *port = circuit_port_ptr(&ar->view->circuit, ends[i]);

      ComponentView *compView = view_component_ptr(ar->view, port->component);
      points[i] = (RT_Point){
        .x = compView->box.center.X + portView->center.X,
        .y = compView->box.center.Y + portView->center.Y,
      };
    } break;
    case ID_JUNCTION: {
      JunctionView *junctionView = view_junction_ptr(ar->view, ends[i]);
      points[i] = (RT_Point){
        .x = junctionView->pos.X,
        .y = junctionView->pos.Y,
      };
    } break;
    default:
      assert(0);
    }
  }
  *path = (RT_PathDef){
    .start = points[0],
    .end = points[1],
  };
  ar->pathRanges[circuit_wire_index(&ar->view->circuit, id)] =
    (RT_PathRange){0};
}

void autoroute_update_junction(AutoRoute *ar, ID id) {
  // todo: do something faster
  autoroute_update_anchors(ar);
}

void autoroute_route(AutoRoute *ar, bool betterRoutes) {
  uint64_t start = stm_now();

  RT_Result res = RT_graph_build(
    ar->graph, ar->anchors, arrlen(ar->anchors), ar->boxes,
    circuit_component_len(&ar->view->circuit), betterRoutes);
  if (res != RT_RESULT_SUCCESS) {
    log_error("Error building graph: %d", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  uint64_t graphBuild = stm_since(start);
  uint64_t pathFindStart = stm_now();

  res = RT_graph_find_paths(
    ar->graph, ar->paths, ar->pathRanges, circuit_wire_len(&ar->view->circuit),
    ar->vertexBuffers, ar->vertBufferCapacity);
  assert(res == RT_RESULT_SUCCESS);

  uint64_t pathFind = stm_since(pathFindStart);

  log_info(
    "Graph build: %fms, Path find: %fms", stm_ms(graphBuild), stm_ms(pathFind));
}

size_t autoroute_wire_vertices(
  AutoRoute *ar, WireID wireID, float *coords, size_t maxLen) {

  int index = circuit_wire_index(&ar->view->circuit, wireID);
  RT_PathRange *range = &ar->pathRanges[index];
  RT_VertexBuffer *vb = &ar->vertexBuffers[range->vertex_buffer_index];

  // todo: this copy could be avoided
  memcpy(
    coords, &vb->vertices[range->vertex_offset],
    range->vertex_count * sizeof(RT_Vertex));

  return (sizeof(RT_Vertex) * range->vertex_count) / sizeof(float);
}

void autoroute_get_junction_pos(
  AutoRoute *ar, JunctionID junctionID, float *x, float *y) {
  JunctionView *junctionView = view_junction_ptr(ar->view, junctionID);
  *x = junctionView->pos.X;
  *y = junctionView->pos.Y;
}

typedef void *Context;
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);

static HMM_Vec2 panZoom(RT_Point position, float zoom, HMM_Vec2 pan) {
  return HMM_AddV2(HMM_MulV2F(HMM_V2(position.x, position.y), zoom), pan);
}

void autoroute_draw_debug_lines(
  AutoRoute *ar, void *ctx, float zoom, HMM_Vec2 pan) {
  const RT_Node *nodes;
  size_t nodeCount;

  RT_Result res = RT_graph_get_nodes(ar->graph, &nodes, &nodeCount);
  assert(res == RT_RESULT_SUCCESS);

  for (size_t i = 0; i < nodeCount; i++) {
    const RT_Node *node = &nodes[i];
    RT_Point p1 = node->position;
    RT_NodeIndex neighbors[4] = {
      node->neighbors.pos_x,
      node->neighbors.neg_x,
      node->neighbors.pos_y,
      node->neighbors.neg_y,
    };
    for (size_t j = 0; j < 4; j++) {
      if (neighbors[j] < nodeCount) {
        RT_Point p2 = nodes[neighbors[j]].position;
        draw_stroked_line(
          ctx, panZoom(p1, zoom, pan), panZoom(p2, zoom, pan), 1,
          HMM_V4(0.5f, 0.5f, 0.7f, 0.5f));
      }
    }
  }

  for (uint32_t i = 0; i < circuit_component_len(&ar->view->circuit); i++) {
    RT_BoundingBox *box = &ar->boxes[i];
    HMM_Vec2 tl = panZoom(
      (RT_Point){
        .x = box->center.x - box->half_width,
        .y = box->center.y - box->half_height,
      },
      zoom, pan);
    HMM_Vec2 br = panZoom(
      (RT_Point){
        .x = box->center.x + box->half_width,
        .y = box->center.y + box->half_height,
      },
      zoom, pan);
    draw_stroked_line(
      ctx, tl, HMM_V2(br.X, tl.Y), 1, HMM_V4(0.7f, 0.5f, 0.5f, 0.5f));
    draw_stroked_line(
      ctx, HMM_V2(br.X, tl.Y), br, 1, HMM_V4(0.7f, 0.5f, 0.5f, 0.5f));
    draw_stroked_line(
      ctx, br, HMM_V2(tl.X, br.Y), 1, HMM_V4(0.7f, 0.5f, 0.5f, 0.5f));
    draw_stroked_line(
      ctx, HMM_V2(tl.X, br.Y), tl, 1, HMM_V4(0.7f, 0.5f, 0.5f, 0.5f));
  }
}

void autoroute_dump_anchor_boxes(AutoRoute *ar) {
  FILE *fp = fopen("dump.rs", "w");
  fprintf(fp, "const ANCHOR_POINTS: &[Anchor] = &[\n");
  for (size_t i = 0; i < arrlen(ar->anchors); i++) {
    fprintf(fp, "    Anchor {\n");
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n",
      ar->anchors[i].position.x, ar->anchors[i].position.y);
    if (ar->anchors[i].bounding_box == RT_INVALID_BOUNDING_BOX_INDEX) {
      fprintf(fp, "        bounding_box: BoundingBoxIndex::INVALID,\n");
    } else {
      fprintf(
        fp, "        bounding_box: bbi!(%d),\n", ar->anchors[i].bounding_box);
    }
    if (ar->anchors[i].connect_directions == RT_DIRECTIONS_ALL) {
      fprintf(fp, "        connect_directions: Directions::ALL,\n");
    } else {
      fprintf(
        fp, "        connect_directions: Directions::%s,\n",
        ar->anchors[i].connect_directions == RT_DIRECTIONS_POS_X ? "POS_X"
                                                                 : "NEG_X");
    }
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n\n");
  fprintf(fp, "const BOUNDING_BOXES: &[BoundingBox] = &[\n");
  for (size_t i = 0; i < circuit_component_len(&ar->view->circuit); i++) {
    fprintf(fp, "    BoundingBox {\n");
    fprintf(
      fp, "        center: Point { x: %d, y: %d },\n", ar->boxes[i].center.x,
      ar->boxes[i].center.y);
    fprintf(fp, "        half_width: %d,\n", ar->boxes[i].half_width);
    fprintf(fp, "        half_height: %d,\n", ar->boxes[i].half_height);
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fclose(fp);
}