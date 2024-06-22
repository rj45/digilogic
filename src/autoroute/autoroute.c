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
#include "sokol_time.h"

#include "autoroute/autoroute.h"
#include "core/core.h"
#include "routing/routing.h"
#include "view/view.h"
#include <stdint.h>

#define LOG_LEVEL LL_INFO
#include "log.h"

#define RT_PADDING 10.0f

#define TIME_SAMPLES 120

struct AutoRoute {
  Circuit *circ;

  arr(RT_Net) nets;
  arr(RT_NetView) netViews;
  arr(ID) netIDs;
  arr(RT_Endpoint) endpoints;
  arr(RT_Point) waypoints;
  arr(RT_BoundingBox) boxes;

  arr(RT_Anchor) anchors;

  arr(uint32_t) boxIndices;

  arr(Wire) wires;
  arr(HMM_Vec2) vertices;

  // todo: this is for nudging, so that we can diff the new state from the
  // previous state and know what wires to update in the BVH
  arr(Wire) prevWires;
  arr(HMM_Vec2) prevVertices;

  RT_Graph *graph;

  bool needsRefresh;

  int timeIndex;
  int timeLength;
  uint64_t buildTimes[TIME_SAMPLES];
  uint64_t routeTimes[TIME_SAMPLES];
};

void autoroute_global_init() {
  RT_Result res = RT_init_thread_pool();
  assert(res == RT_RESULT_SUCCESS);
}

AutoRoute *autoroute_create(Circuit *circ) {
  AutoRoute *ar = malloc(sizeof(AutoRoute));
  *ar = (AutoRoute){
    .circ = circ,
  };

  RT_Result res = RT_graph_new(&ar->graph);
  assert(res == RT_RESULT_SUCCESS);

  return ar;
}

void autoroute_free(AutoRoute *ar) {
  arrfree(ar->anchors);

  arrfree(ar->nets);
  arrfree(ar->netViews);
  arrfree(ar->endpoints);
  arrfree(ar->waypoints);
  arrfree(ar->boxes);

  arrfree(ar->netIDs);
  arrfree(ar->boxIndices);

  arrfree(ar->wires);
  arrfree(ar->vertices);
  arrfree(ar->prevWires);
  arrfree(ar->prevVertices);

  RT_Result res = RT_graph_free(ar->graph);
  assert(res == RT_RESULT_SUCCESS);
  free(ar);
}

static void autoroute_update(AutoRoute *ar) {
  ID top = ar->circ->top;

  arrsetlen(ar->boxIndices, circ_len(ar->circ, Symbol));
  arrsetlen(ar->boxes, 0);

  LinkedListIter topit = circ_lliter(ar->circ, top);
  while (circ_lliter_next(&topit)) {
    ID symbolID = topit.current;
    Position symbolPos = circ_get(ar->circ, symbolID, Position);
    SymbolKindID kindID = circ_get(ar->circ, symbolID, SymbolKindID);
    Size size = circ_get(ar->circ, kindID, Size);
    HMM_Vec2 halfSize = HMM_MulV2F(size, 0.5f);
    RT_BoundingBox box = (RT_BoundingBox){
      .center = (RT_Point){.x = symbolPos.X, .y = symbolPos.Y},
      .half_width = (uint16_t)(halfSize.X + RT_PADDING) - 1,
      .half_height = (uint16_t)(halfSize.Y + RT_PADDING) - 1};

    ar->boxIndices[circ_row_for_id(ar->circ, symbolID)] = arrlen(ar->boxes);
    arrput(ar->boxes, box);
  }

  arrsetlen(ar->nets, 0);
  arrsetlen(ar->netIDs, 0);
  arrsetlen(ar->endpoints, 0);
  arrsetlen(ar->anchors, 0);
  arrsetlen(ar->waypoints, 0);

  NetlistID netlistID = circ_get(ar->circ, top, NetlistID);
  LinkedListIter netit = circ_lliter(ar->circ, netlistID);
  while (circ_lliter_next(&netit)) {
    ID netID = netit.current;
    size_t netIndex = arrlen(ar->nets);
    arrput(ar->nets, (RT_Net){.first_endpoint = RT_INVALID_ENDPOINT_INDEX});
    arrput(ar->netIDs, netID);

    LinkedListIter subnetit = circ_lliter(ar->circ, netID);
    while (circ_lliter_next(&subnetit)) {
      ID subnetID = subnetit.current;

      ptrdiff_t endpointIndex = -1;

      LinkedListIter endpointit = circ_lliter(ar->circ, subnetID);
      while (circ_lliter_next(&endpointit)) {
        ID endpointID = endpointit.current;
        Position endpointPos = circ_get(ar->circ, endpointID, Position);

        RT_Endpoint endpoint = (RT_Endpoint){
          .position = (RT_Point){.x = endpointPos.X, .y = endpointPos.Y},
          .first_waypoint = RT_INVALID_WAYPOINT_INDEX,
          .next = RT_INVALID_ENDPOINT_INDEX,
        };
        log_debug(
          "Endpoint %x: %d %d", endpointID, endpoint.position.x,
          endpoint.position.y);

        if (endpointIndex >= 0) {
          ar->endpoints[endpointIndex].next = arrlen(ar->endpoints);
        } else {
          ar->nets[netIndex].first_endpoint = arrlen(ar->endpoints);
        }
        endpointIndex = arrlen(ar->endpoints);
        arrput(ar->endpoints, endpoint);

        PortRef portRef = circ_get(ar->circ, endpointID, PortRef);
        RT_BoundingBoxIndex bbi = RT_INVALID_BOUNDING_BOX_INDEX;
        RT_Directions directions = RT_DIRECTIONS_ALL;

        if (circ_has(ar->circ, portRef.symbol)) {
          bbi = ar->boxIndices[circ_row_for_id(ar->circ, portRef.symbol)];
          directions = circ_has_tags(ar->circ, portRef.port, TAG_IN)
                         ? RT_DIRECTIONS_NEG_X
                         : RT_DIRECTIONS_POS_X;
        }

        RT_Anchor anchor = (RT_Anchor){
          .position = (RT_Point){.x = endpointPos.X, .y = endpointPos.Y},
          .connect_directions = directions,
          .bounding_box = bbi,
        };
        arrput(ar->anchors, anchor);

        log_debug(
          "Anchor %x: %d %d", endpointID, anchor.position.x, anchor.position.y);

        ptrdiff_t waypointIndex = -1;

        LinkedListIter waypointit = circ_lliter(ar->circ, endpointID);
        while (circ_lliter_next(&waypointit)) {
          ID waypointID = waypointit.current;
          Position waypointPos = circ_get(ar->circ, waypointID, Position);

          RT_Waypoint waypoint = (RT_Waypoint){
            .position = (RT_Point){.x = waypointPos.X, .y = waypointPos.Y},
            .next = RT_INVALID_WAYPOINT_INDEX,
          };

          if (waypointIndex >= 0) {
            ar->waypoints[waypointIndex].next = arrlen(ar->waypoints);
          } else {
            ar->endpoints[endpointIndex].first_waypoint = arrlen(ar->waypoints);
          }
          waypointIndex = arrlen(ar->waypoints);
          arrput(ar->waypoints, waypoint);

          RT_Anchor anchor = (RT_Anchor){
            .position = (RT_Point){.x = waypointPos.X, .y = waypointPos.Y},
            .connect_directions = RT_DIRECTIONS_ALL,
            .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
          };
          arrput(ar->anchors, anchor);
        }
      }
    }
  }

  arrsetlen(ar->netViews, arrlen(ar->nets));
}

static void autoroute_prepare_routing(AutoRoute *ar, RoutingConfig config) {
  autoroute_update(ar);

  if (arrlen(ar->anchors) == 0) {
    return;
  }

  RT_Result res = RT_graph_build(
    ar->graph, (RT_Slice_Anchor){ar->anchors, arrlen(ar->anchors)},
    (RT_Slice_BoundingBox){ar->boxes, arrlen(ar->boxes)}, config.minimizeGraph);
  if (res != RT_RESULT_SUCCESS) {
    log_error("Error building graph: %d", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  if (arrlen(ar->vertices) == 0) {
    arrsetlen(ar->vertices, 1024);
    arrsetlen(ar->prevVertices, 1024);
  }
  if (arrlen(ar->wires) == 0) {
    arrsetlen(ar->wires, 1024);
    arrsetlen(ar->prevWires, 1024);
  }

  assert(ar->graph);
}

bool autoroute_dump_routing_data(
  AutoRoute *ar, RoutingConfig config, const char *filename) {
  autoroute_prepare_routing(ar, config);
  RT_Result res = RT_graph_serialize_connect_nets_query(
    ar->graph, (RT_Slice_Net){ar->nets, arrlen(ar->nets)},
    (RT_Slice_Endpoint){ar->endpoints, arrlen(ar->endpoints)},
    (RT_Slice_Waypoint){ar->waypoints, arrlen(ar->waypoints)},
    config.performCentering, filename);
  if (res != RT_RESULT_SUCCESS) {
    log_error("Error serializing graph: %d", res);
    return false;
  }
  return true;
}

void autoroute_route(AutoRoute *ar, RoutingConfig config) {
  uint64_t start = stm_now();

  autoroute_prepare_routing(ar, config);

  uint64_t graphBuild = stm_since(start);
  uint64_t pathFindStart = stm_now();

  {
    // swap wires
    arr(Wire) tmp = ar->prevWires;
    ar->prevWires = ar->wires;
    ar->wires = tmp;
  }

  {
    // swap vertices
    arr(HMM_Vec2) tmp = ar->prevVertices;
    ar->prevVertices = ar->vertices;
    ar->vertices = tmp;
  }

  RT_Result res;
  for (;;) {
    res = RT_graph_connect_nets(
      ar->graph, (RT_Slice_Net){ar->nets, arrlen(ar->nets)},
      (RT_Slice_Endpoint){ar->endpoints, arrlen(ar->endpoints)},
      (RT_Slice_Point){ar->waypoints, arrlen(ar->waypoints)},
      (RT_MutSlice_Vertex){
        (RT_Vertex *)ar->vertices,
        arrlen(ar->vertices),
      },
      (RT_MutSlice_WireView){
        (RT_WireView *)ar->wires,
        arrlen(ar->wires),
      },
      (RT_MutSlice_NetView){
        ar->netViews,
        arrlen(ar->netViews),
      },
      config.performCentering);
    switch (res) {
    case RT_RESULT_SUCCESS:
      break;
    case RT_RESULT_NULL_POINTER_ERROR:
      log_error("Null pointer error");
      break;
    case RT_RESULT_INVALID_OPERATION_ERROR:
      log_error("Invalid operation error");
      break;
    case RT_RESULT_UNINITIALIZED_ERROR:
      log_error("Uninitialized error");
      break;
    case RT_RESULT_INVALID_ARGUMENT_ERROR:
      log_error("Invalid argument error");
      break;
    case RT_RESULT_VERTEX_BUFFER_OVERFLOW_ERROR:
      arrsetlen(ar->vertices, arrlen(ar->vertices) * 2);
      arrsetlen(ar->prevVertices, arrlen(ar->prevVertices) * 2);
      continue;
    case RT_RESULT_WIRE_VIEW_BUFFER_OVERFLOW_ERROR:
      arrsetlen(ar->wires, arrlen(ar->wires) * 2);
      arrsetlen(ar->prevWires, arrlen(ar->prevWires) * 2);
      continue;
    }
    break;
  }

  if (res != RT_RESULT_SUCCESS) {
    RT_Result serres = RT_graph_serialize(ar->graph, "graph.dump");
    if (serres != RT_RESULT_SUCCESS) {
      switch (serres) {
      case RT_RESULT_NULL_POINTER_ERROR:
        log_error("error serializing graph: null pointer error");
        break;
      case RT_RESULT_INVALID_OPERATION_ERROR:
        log_error("error serializing graph: serialization failed (invalid "
                  "operation error)");
        break;
      case RT_RESULT_INVALID_ARGUMENT_ERROR:
        log_error("error serializing graph: file path contains illegal UTF-8 "
                  "(invalid argument error)");
        break;
      case RT_RESULT_IO_ERROR:
        log_error("error serializing graph: IO error");
        break;
      default:
        log_error("error serializing graph: %d", serres);
        break;
      }
    }
  }

  assert(res == RT_RESULT_SUCCESS);

  uint64_t pathFind = stm_since(pathFindStart);

  for (int i = 0; i < arrlen(ar->netViews); i++) {
    RT_NetView *rtNetView = &ar->netViews[i];

    WireVertices wireVerts = {
      .wireVertexCounts = (uint16_t *)&ar->wires[rtNetView->wire_offset],
      .vertices = &ar->vertices[rtNetView->vertex_offset],
      .wireCount = rtNetView->wire_count,
    };
    circuit_set_net_wire_vertices(ar->circ, ar->netIDs[i], wireVerts);
  }

  ar->buildTimes[ar->timeIndex] = graphBuild;
  ar->routeTimes[ar->timeIndex] = pathFind;
  ar->timeIndex = (ar->timeIndex + 1) % TIME_SAMPLES;
  if (ar->timeLength < TIME_SAMPLES) {
    ar->timeLength++;
  }

  // RouteTimeStats stats = autoroute_stats(ar);

  // log_info(
  //   "Build: %.3fms min, %.3fms avg, %.3fms max; Pathing: %.3fms min, %.3fms "
  //   "avg, %.3fms max; %d samples",
  //   stm_ms(stats.build.min), stm_ms(stats.build.avg),
  //   stm_ms(stats.build.max), stm_ms(stats.route.min),
  //   stm_ms(stats.route.avg), stm_ms(stats.route.max), ar->timeLength);
}

RouteTimeStats autoroute_stats(AutoRoute *ar) {
  RouteTimeStats stats = {
    .build = {.avg = 0, .min = UINT64_MAX, .max = 0},
    .route = {.avg = 0, .min = UINT64_MAX, .max = 0},
  };

  for (int i = 0; i < ar->timeLength; i++) {
    stats.build.avg += ar->buildTimes[i];
    stats.route.avg += ar->routeTimes[i];
    stats.build.min = HMM_MIN(stats.build.min, ar->buildTimes[i]);
    stats.route.min = HMM_MIN(stats.route.min, ar->routeTimes[i]);
    stats.build.max = HMM_MAX(stats.build.max, ar->buildTimes[i]);
    stats.route.max = HMM_MAX(stats.route.max, ar->routeTimes[i]);
  }

  if (ar->timeLength != 0) {
    stats.build.avg /= ar->timeLength;
    stats.route.avg /= ar->timeLength;
  }

  stats.samples = ar->timeLength;

  return stats;
}

typedef void *Context;
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_filled_circle(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color);

void autoroute_draw_debug_lines(AutoRoute *ar, void *ctx) {
  RT_Slice_Node nodes;

  RT_Result res = RT_graph_get_nodes(ar->graph, &nodes);
  assert(res == RT_RESULT_SUCCESS);

  for (size_t i = 0; i < nodes.len; i++) {
    const RT_Node *node = &nodes.ptr[i];
    RT_Point p1 = node->position;
    RT_NodeIndex neighbors[4] = {
      node->neighbors.pos_x,
      node->neighbors.neg_x,
      node->neighbors.pos_y,
      node->neighbors.neg_y,
    };
    draw_filled_circle(
      ctx, HMM_V2((float)p1.x - 1.5, (float)p1.y - 1.5), HMM_V2(3, 3),
      HMM_V4(0.5f, 1.0f, 1.0f, 0.5f));
    for (size_t j = 0; j < 4; j++) {
      if (neighbors[j] < nodes.len) {
        RT_Point p2 = nodes.ptr[neighbors[j]].position;
        draw_stroked_line(
          ctx, HMM_V2(p1.x, p1.y), HMM_V2(p2.x, p2.y), 0.5,
          HMM_V4(0.5f, 0.5f, 0.7f, 0.5f));
      }
    }
  }

  for (size_t i = 0; i < arrlen(ar->boxes); i++) {
    RT_BoundingBox *box = &ar->boxes[i];
    HMM_Vec2 tl =
      HMM_V2(box->center.x - box->half_width, box->center.y - box->half_height);
    HMM_Vec2 br =
      HMM_V2(box->center.x + box->half_width, box->center.y + box->half_height);
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
  for (size_t i = 0; i < arrlen(ar->boxes); i++) {
    fprintf(fp, "    BoundingBox {\n");
    fprintf(
      fp, "        center: Point { x: %d, y: %d },\n", ar->boxes[i].center.x,
      ar->boxes[i].center.y);
    fprintf(fp, "        half_width: %d,\n", ar->boxes[i].half_width);
    fprintf(fp, "        half_height: %d,\n", ar->boxes[i].half_height);
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fprintf(fp, "const ENDPOINTS: &[Endpoint] = &[\n");
  for (size_t i = 0; i < arrlen(ar->endpoints); i++) {
    RT_Endpoint *endpoint = &ar->endpoints[i];
    fprintf(fp, "    Endpoint {\n");
    // todo: fixme
    // if (!circuit_has(ar->circ, endpoint->net)) {
    //   fprintf(fp, "        NetIndex: NetIndex::INVALID,\n");
    // } else {
    //   fprintf(
    //     fp, "        NetIndex: ni!(%d),\n",
    //     circuit_index(ar->circ, endpoint->net));
    // }
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n", endpoint->position.x,
      endpoint->position.x);
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fprintf(fp, "const WAYPOINTS: &[Waypoint] = &[\n");
  for (size_t i = 0; i < arrlen(ar->waypoints); i++) {
    RT_Waypoint *waypoint = &ar->waypoints[i];
    fprintf(fp, "    Waypoint {\n");
    // todo: fixme
    // if (!circuit_has(ar->circ, waypoint->net)) {
    //   fprintf(fp, "        NetIndex: NetIndex::INVALID,\n");
    // } else {
    //   fprintf(
    //     fp, "        NetIndex: ni!(%d),\n",
    //     circuit_index(ar->circ, waypoint->net));
    // }
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n",
      (int)(waypoint->position.x), (int)(waypoint->position.y));
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fclose(fp);
}
