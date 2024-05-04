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
#include "routing/routing.h"
#include "view/view.h"

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

  arr(RT_Point) anchors;

  size_t threadCount;
  size_t vertBufferCapacity;
  RT_Graph *graph;

  arr(RT_VertexBuffer) vertexBuffers;

  Timer timer;
};

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

  RT_Result res = RT_init_thread_pool(&ar->threadCount);
  (void)res;
  // assert(res == RT_RESULT_SUCCESS);

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

  timer_init(&ar->timer);

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
    RT_Point tl = (RT_Point){.x = cx - halfSize.X, .y = cy - halfSize.Y};
    RT_Point tr = (RT_Point){.x = cx + halfSize.X, .y = cy - halfSize.Y};
    RT_Point bl = (RT_Point){.x = cx - halfSize.X, .y = cy + halfSize.Y};
    RT_Point br = (RT_Point){.x = cx + halfSize.X, .y = cy + halfSize.Y};

    arrput(ar->anchors, tl);
    arrput(ar->anchors, tr);
    arrput(ar->anchors, bl);
    arrput(ar->anchors, br);

    PortID portID = comp->portFirst;
    while (portID != NO_PORT) {
      Port *port = circuit_port_ptr(&ar->view->circuit, portID);
      PortView *portView = view_port_ptr(ar->view, portID);

      RT_Point anchor = (RT_Point){
        .x = cx + portView->center.X,
        .y = cy + portView->center.Y,
      };
      arrput(ar->anchors, anchor);

      portID = port->compNext;
    }
  }
  for (int i = 0; i < circuit_junction_len(&ar->view->circuit); i++) {
    JunctionView *junctionView = &ar->view->junctions[i];
    RT_Point anchor = (RT_Point){
      .x = junctionView->pos.X,
      .y = junctionView->pos.Y,
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
    .half_width = compView->box.halfSize.X,
    .half_height = compView->box.halfSize.Y,
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
    .net_id = id,
    .start = points[0],
    .end = points[1],
  };
}

void autoroute_update_junction(AutoRoute *ar, ID id) {
  // todo: do something faster
  autoroute_update_anchors(ar);
}

void autoroute_route(AutoRoute *ar) {
  double start = timer_now(&ar->timer);

  RT_Result res = RT_graph_build(
    ar->graph, ar->anchors, arrlen(ar->anchors), ar->boxes,
    circuit_component_len(&ar->view->circuit));
  if (res != RT_RESULT_SUCCESS) {
    printf("Error building graph: %d\n", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  double graphBuild = timer_now(&ar->timer);

  res = RT_graph_find_paths(
    ar->graph, ar->paths, circuit_wire_len(&ar->view->circuit),
    ar->vertexBuffers, ar->vertBufferCapacity);
  assert(res == RT_RESULT_SUCCESS);

  double pathFind = timer_now(&ar->timer);

  printf(
    "Graph build: %f, Path find: %f\n", graphBuild - start,
    pathFind - graphBuild);
}

size_t autoroute_wire_vertices(
  AutoRoute *ar, WireID wireID, float *coords, size_t maxLen) {
  int index = 0;
  for (int i = 0; i < arrlen(ar->vertexBuffers); i++) {
    RT_VertexBuffer *vb = &ar->vertexBuffers[i];
    for (int j = 0; j < vb->vertex_count; j++) {
      if (vb->vertices[j].net_id == wireID) {
        coords[index++] = vb->vertices[j].x;
        coords[index++] = vb->vertices[j].y;
      }
    }
  }
  return index;
}

void autoroute_get_junction_pos(
  AutoRoute *ar, JunctionID junctionID, float *x, float *y) {
  JunctionView *junctionView = view_junction_ptr(ar->view, junctionID);
  *x = junctionView->pos.X;
  *y = junctionView->pos.Y;
}

void autoroute_draw_debug_lines(
  AutoRoute *ar, void *ctx, float zoom, HMM_Vec2 pan) {}

void autoroute_dump_anchor_boxes(AutoRoute *ar) {}