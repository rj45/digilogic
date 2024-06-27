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

typedef struct RecEvent {
  enum {
    REC_EVENT_BEGIN_PATH_FINDING,
    REC_EVENT_PATH_FINDING_SET_G_SCORE,
    REC_EVENT_PATH_FINDING_PUSH_OPEN_QUEUE,
    REC_EVENT_PATH_FINDING_SET_PREDECESSOR,
    REC_EVENT_PATH_FINDING_POP_OPEN_QUEUE,
    REC_EVENT_PATH_FINDING_CLEAR_STATE,
    REC_EVENT_PATH_FINDING_INSERT_PATH_NODE,
    REC_EVENT_PATH_FINDING_REMOVE_PATH_NODE,
    REC_EVENT_END_PATH_FINDING,
    REC_EVENT_ROUTING_BEGIN_ROOT_WIRE,
    REC_EVENT_ROUTING_BEGIN_BRANCH_WIRE,
    REC_EVENT_ROUTING_PUSH_VERTEX,
    REC_EVENT_ROUTING_END_WIRE_SEGMENT,
    REC_EVENT_ROUTING_END_WIRE,
  } type;
  union {
    struct {
      RT_NodeIndex start_index;
      arr(RT_NodeIndex) end_indices;
      bool visit_all;
    } begin_path_finding;
    struct {
      RT_NodeIndex node;
      uint32_t g_score;
    } path_finding_set_g_score;
    struct {
      RT_NodeIndex node;
      uint32_t f_score;
    } path_finding_pop_open_queue;
    struct {
      RT_NodeIndex node;
      uint32_t f_score;
    } path_finding_push_open_queue;
    struct {
      RT_NodeIndex node;
      RT_NodeIndex predecessor;
    } path_finding_set_predecessor;
    struct {
      size_t index;
      RT_NodeIndex node;
    } path_finding_insert_path_node;
    struct {
      size_t index;
    } path_finding_remove_path_node;
    struct {
      bool found;
    } end_path_finding;
    struct {
      RT_Point start;
      RT_Point end;
    } routing_begin_root_wire;
    struct {
      RT_Point start;
    } routing_begin_branch_wire;
    struct {
      RT_Vertex vertex;
    } routing_push_vertex;
    struct {
      bool ends_in_junction;
    } routing_end_wire_segment;
  };
} RecEvent;

typedef struct RoutePath {
  bool root;
  size_t start;
  size_t end;
} RoutePath;

typedef struct RouteRecording {
  arr(RecEvent) events;

  // playback
  RT_Slice_Node graph;

  bool inPathFinding;
  int currentEvent;
  hashmap(RT_NodeIndex, uint32_t) gScores;
  hashmap(RT_NodeIndex, uint32_t) fScores;
  hashmap(RT_NodeIndex, uint32_t) poppedScores;
  hashmap(RT_NodeIndex, RT_NodeIndex) predecessors;
  arr(RT_NodeIndex) path;
  RT_NodeIndex startNode;
  arr(RT_NodeIndex) endNodes;
  bool visitAll;
  RT_NodeIndex poppedNode;
  RT_NodeIndex pathInsertedNode;
  RT_NodeIndex pathRemovedNode;

  bool rootWireValid;
  RT_Point rootWireStart;
  RT_Point rootWireEnd;

  bool branchWireValid;
  RT_Point branchWireStart;
  RT_Point branchWireEnd;

  arr(RoutePath) routePaths;
  arr(RT_Vertex) routeVertices;

  int currentNetPlusOne;
  int currentVertIndex;
  int currentWireVertCount;
  int currentWireIndex;
} RouteRecording;

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

  // TODO: this is for nudging, so that we can diff the new state from the
  // previous state and know what wires to update in the BVH
  arr(Wire) prevWires;
  arr(HMM_Vec2) prevVertices;

  RT_Graph *graph;

  bool needsRefresh;

  RouteRecording recording;

  int timeIndex;
  int timeLength;
  uint64_t buildTimes[TIME_SAMPLES];
  uint64_t routeTimes[TIME_SAMPLES];
};

static void autoroute_replay_free(RouteRecording *rec);
static bool autoroute_replay_play(AutoRoute *ar);
static void rec_callbacks(RT_ReplayCallbacks *callbacks, RouteRecording *rec);

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

  autoroute_replay_free(&ar->recording);
  arrfree(ar->recording.events);

  hmfree(ar->recording.gScores);
  hmfree(ar->recording.fScores);
  hmfree(ar->recording.poppedScores);
  hmfree(ar->recording.predecessors);
  arrfree(ar->recording.path);
  arrfree(ar->recording.endNodes);

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
    arrput(ar->nets, (RT_Net){0});
    arrput(ar->netIDs, netID);

    LinkedListIter subnetit = circ_lliter(ar->circ, netID);
    while (circ_lliter_next(&subnetit)) {
      ID subnetID = subnetit.current;

      LinkedListIter endpointit = circ_lliter(ar->circ, subnetID);
      while (circ_lliter_next(&endpointit)) {
        ID endpointID = endpointit.current;
        Position endpointPos = circ_get(ar->circ, endpointID, Position);

        RT_Endpoint endpoint = {
          .position = {.x = endpointPos.X, .y = endpointPos.Y},
        };
        log_debug(
          "Endpoint %x: %d %d", endpointID, endpoint.position.x,
          endpoint.position.y);

        arrput(ar->endpoints, endpoint);

        RT_Net *currentNet = &ar->nets[arrlen(ar->nets) - 1];
        if (currentNet->endpoint_count == 0) {
          currentNet->endpoint_offset = arrlen(ar->endpoints) - 1;
        }
        currentNet->endpoint_count++;

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

        LinkedListIter waypointit = circ_lliter(ar->circ, endpointID);
        while (circ_lliter_next(&waypointit)) {
          ID waypointID = waypointit.current;
          Position waypointPos = circ_get(ar->circ, waypointID, Position);

          RT_Point waypoint = {.x = waypointPos.X, .y = waypointPos.Y};
          arrput(ar->waypoints, waypoint);

          // Ensure that the waypoint_offset and waypoint_count are set properly
          // in the RT_Endpoint struct
          RT_Endpoint *currentEndpoint =
            &ar->endpoints[arrlen(ar->endpoints) - 1];
          if (currentEndpoint->waypoint_count == 0) {
            currentEndpoint->waypoint_offset = arrlen(ar->waypoints) - 1;
          }
          currentEndpoint->waypoint_count++;

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
    (RT_Slice_Point){ar->waypoints, arrlen(ar->waypoints)},
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
    if (config.recordReplay) {
      RT_ReplayCallbacks callbacks;
      rec_callbacks(&callbacks, &ar->recording);

      res = RT_graph_get_nodes(ar->graph, &ar->recording.graph);
      assert(res == RT_RESULT_SUCCESS);

      autoroute_replay_free(&ar->recording);

      res = RT_graph_connect_nets_replay(
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
        config.performCentering, callbacks);

      autoroute_replay_rewind(ar);
    } else {
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
    }

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

void draw_stroked_line(
  DrawContext *draw, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_stroked_line(
  DrawContext *draw, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);
void draw_filled_circle(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color);
void draw_text(
  DrawContext *draw, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor);
Box draw_text_bounds(
  DrawContext *ctx, HMM_Vec2 pos, const char *text, int len, HorizAlign horz,
  VertAlign vert, float fontSize, FontHandle font);
void draw_stroked_circle(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color);

void autoroute_draw_debug_lines(AutoRoute *ar, DrawContext *ctx) {
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

static void autoroute_replay_free(RouteRecording *rec) {
  for (size_t i = 0; i < arrlen(rec->events); i++) {
    switch (rec->events[i].type) {
    case REC_EVENT_BEGIN_PATH_FINDING:
      arrfree(rec->events[i].begin_path_finding.end_indices);
      break;

    case REC_EVENT_PATH_FINDING_SET_G_SCORE:
    case REC_EVENT_PATH_FINDING_PUSH_OPEN_QUEUE:
    case REC_EVENT_PATH_FINDING_SET_PREDECESSOR:
    case REC_EVENT_PATH_FINDING_POP_OPEN_QUEUE:
    case REC_EVENT_PATH_FINDING_CLEAR_STATE:
    case REC_EVENT_PATH_FINDING_INSERT_PATH_NODE:
    case REC_EVENT_PATH_FINDING_REMOVE_PATH_NODE:
    case REC_EVENT_END_PATH_FINDING:
    case REC_EVENT_ROUTING_BEGIN_ROOT_WIRE:
    case REC_EVENT_ROUTING_BEGIN_BRANCH_WIRE:
    case REC_EVENT_ROUTING_PUSH_VERTEX:
    case REC_EVENT_ROUTING_END_WIRE_SEGMENT:
    case REC_EVENT_ROUTING_END_WIRE:
      break;
    }
  }
  arrsetlen(rec->events, 0);
}

static void autoroute_replay_clear_state(AutoRoute *ar) {
  RouteRecording *rec = &ar->recording;

  while (hmlen(rec->gScores) > 0) {
    hmdel(rec->gScores, rec->gScores[hmlen(rec->gScores) - 1].key);
  }
  while (hmlen(rec->fScores) > 0) {
    hmdel(rec->fScores, rec->fScores[hmlen(rec->fScores) - 1].key);
  }
  while (hmlen(rec->poppedScores) > 0) {
    hmdel(
      rec->poppedScores, rec->poppedScores[hmlen(rec->poppedScores) - 1].key);
  }
  while (hmlen(rec->predecessors) > 0) {
    hmdel(
      rec->predecessors, rec->predecessors[hmlen(rec->predecessors) - 1].key);
  }
}

static void autoroute_replay_clear_all_state(AutoRoute *ar) {
  RouteRecording *rec = &ar->recording;

  autoroute_replay_clear_state(ar);

  arrsetlen(rec->path, 0);
  rec->visitAll = false;
  rec->inPathFinding = false;
  rec->startNode = RT_INVALID_NODE_INDEX;
  rec->poppedNode = RT_INVALID_NODE_INDEX;
  rec->pathInsertedNode = RT_INVALID_NODE_INDEX;
  rec->pathRemovedNode = RT_INVALID_NODE_INDEX;
  arrsetlen(rec->endNodes, 0);
}

void autoroute_replay_rewind(AutoRoute *ar) {
  RouteRecording *rec = &ar->recording;
  rec->currentEvent = 0;
  arrsetlen(rec->routeVertices, 0);
  arrsetlen(rec->routePaths, 0);
  autoroute_replay_clear_all_state(ar);
  rec->rootWireValid = false;
  rec->branchWireValid = false;
  rec->currentNetPlusOne = 0;
  rec->currentVertIndex = 0;
  rec->currentWireVertCount = 0;
  rec->currentWireIndex = 0;
  autoroute_replay_play(ar);
}

static RT_Point
autoroute_replay_closest_point_on_line(RT_Point A, RT_Point B, RT_Point P) {
  RT_Point closest;
  float ABx = (float)B.x - (float)A.x;
  float ABy = (float)B.y - (float)A.y;
  float APx = (float)P.x - (float)A.x;
  float APy = (float)P.y - (float)A.y;
  float AB_AB = ABx * ABx + ABy * ABy;
  float AP_AB = APx * ABx + APy * ABy;
  float t = AP_AB / AB_AB;

  // Clamp t to the range [0, 1] to ensure the closest point is on the segment
  if (t < 0.0)
    t = 0.0;
  if (t > 1.0)
    t = 1.0;

  closest.x = A.x + t * ABx;
  closest.y = A.y + t * ABy;

  return closest;
}

static bool autoroute_replay_play(AutoRoute *ar) {
  if (ar->recording.currentEvent >= arrlen(ar->recording.events)) {
    return false;
  }

  RouteRecording *rec = &ar->recording;

  rec->poppedNode = RT_INVALID_NODE_INDEX;
  rec->pathInsertedNode = RT_INVALID_NODE_INDEX;
  rec->pathRemovedNode = RT_INVALID_NODE_INDEX;

  RecEvent *event = &rec->events[rec->currentEvent];
  switch (event->type) {
  case REC_EVENT_BEGIN_PATH_FINDING:
    rec->inPathFinding = true;
    rec->startNode = event->begin_path_finding.start_index;
    rec->visitAll = event->begin_path_finding.visit_all;
    arrsetlen(rec->endNodes, 0);
    for (size_t i = 0; i < arrlen(event->begin_path_finding.end_indices); i++) {
      arrput(rec->endNodes, event->begin_path_finding.end_indices[i]);
    }

    break;

  case REC_EVENT_PATH_FINDING_SET_G_SCORE:
    hmput(
      rec->gScores, event->path_finding_set_g_score.node,
      event->path_finding_set_g_score.g_score);
    break;

  case REC_EVENT_PATH_FINDING_PUSH_OPEN_QUEUE:
    hmput(
      rec->fScores, event->path_finding_push_open_queue.node,
      event->path_finding_push_open_queue.f_score);
    break;

  case REC_EVENT_PATH_FINDING_SET_PREDECESSOR:
    hmput(
      rec->predecessors, event->path_finding_set_predecessor.node,
      event->path_finding_set_predecessor.predecessor);
    break;

  case REC_EVENT_PATH_FINDING_POP_OPEN_QUEUE: {
    uint32_t fScore =
      hmget(rec->fScores, event->path_finding_pop_open_queue.node);
    hmdel(rec->fScores, event->path_finding_pop_open_queue.node);
    hmput(rec->poppedScores, event->path_finding_pop_open_queue.node, fScore);
    rec->poppedNode = event->path_finding_pop_open_queue.node;
    break;
  }

  case REC_EVENT_PATH_FINDING_CLEAR_STATE:
    autoroute_replay_clear_state(ar);
    break;

  case REC_EVENT_PATH_FINDING_INSERT_PATH_NODE:
    if (
      event->path_finding_insert_path_node.index >= 0 &&
      event->path_finding_insert_path_node.index < arrlen(rec->path)) {
      arrins(
        rec->path, event->path_finding_insert_path_node.index,
        event->path_finding_insert_path_node.node);
    } else if (
      event->path_finding_insert_path_node.index == arrlen(rec->path)) {
      arrput(rec->path, event->path_finding_insert_path_node.node);
    } else {
      log_error(
        "Insert: Invalid path node index %zu / %zu",
        event->path_finding_insert_path_node.index, arrlen(rec->path));
    }

    rec->pathInsertedNode = event->path_finding_insert_path_node.node;
    break;

  case REC_EVENT_PATH_FINDING_REMOVE_PATH_NODE:
    if (
      event->path_finding_remove_path_node.index >= 0 &&
      event->path_finding_remove_path_node.index < arrlen(rec->path)) {
      arrdel(rec->path, event->path_finding_remove_path_node.index);
    } else {
      log_error(
        "Remove: Invalid path node index %zu / %zu",
        event->path_finding_remove_path_node.index, arrlen(rec->path));
    }
    rec->pathRemovedNode = event->path_finding_remove_path_node.index;
    break;

  case REC_EVENT_END_PATH_FINDING:
    autoroute_replay_clear_all_state(ar);
    break;

  case REC_EVENT_ROUTING_BEGIN_ROOT_WIRE: {
    rec->rootWireValid = true;
    rec->branchWireValid = false;
    rec->rootWireStart = event->routing_begin_root_wire.start;
    rec->rootWireEnd = event->routing_begin_root_wire.end;
    RoutePath path = {
      .root = true,
      .start = arrlen(rec->routeVertices),
      .end = arrlen(rec->routeVertices),
    };
    arrput(rec->routePaths, path);

    rec->currentWireIndex = 0;

    rec->currentNetPlusOne++;
    break;
  }

  case REC_EVENT_ROUTING_BEGIN_BRANCH_WIRE: {
    rec->branchWireValid = true;
    rec->branchWireStart = event->routing_begin_branch_wire.start;
    rec->branchWireEnd = autoroute_replay_closest_point_on_line(
      rec->rootWireStart, rec->rootWireEnd, rec->branchWireStart);
    RoutePath path = {
      .root = false,
      .start = arrlen(rec->routeVertices),
      .end = arrlen(rec->routeVertices),
    };
    arrput(rec->routePaths, path);
    break;
  }

  case REC_EVENT_ROUTING_PUSH_VERTEX:
    arrput(rec->routeVertices, event->routing_push_vertex.vertex);
    rec->routePaths[arrlen(rec->routePaths) - 1].end++;

    rec->currentVertIndex++;
    rec->currentWireVertCount++;

    break;

  // TODO: implement these
  case REC_EVENT_ROUTING_END_WIRE_SEGMENT: {
    rec->currentWireIndex++;
    rec->currentWireVertCount = 0;
    break;
  }
  case REC_EVENT_ROUTING_END_WIRE:

    break;
  }
  return true;
}

bool autoroute_replay_forward(AutoRoute *ar) {
  if ((ar->recording.currentEvent + 1) >= arrlen(ar->recording.events)) {
    return false;
  }

  ar->recording.currentEvent++;

  autoroute_replay_play(ar);

  return true;
}

bool autoroute_replay_forward_to(AutoRoute *ar, int event) {
  if (event < 0 || event >= arrlen(ar->recording.events)) {
    return false;
  }

  autoroute_replay_rewind(ar);
  while (ar->recording.currentEvent < event) {
    autoroute_replay_forward(ar);
  }

  return true;
}

bool autoroute_replay_forward_skip_path(AutoRoute *ar) {
  if (ar->recording.currentEvent >= arrlen(ar->recording.events)) {
    return false;
  }

  for (;;) {
    if (!autoroute_replay_forward(ar)) {
      return false;
    }

    if (
      ar->recording.events[ar->recording.currentEvent - 1].type ==
      REC_EVENT_BEGIN_PATH_FINDING) {
      return true;
    }
  }
}

bool autoroute_replay_forward_skip_root(AutoRoute *ar) {
  if (ar->recording.currentEvent >= arrlen(ar->recording.events)) {
    return false;
  }

  for (;;) {
    if (!autoroute_replay_forward(ar)) {
      return false;
    }

    if (
      ar->recording.events[ar->recording.currentEvent].type ==
      REC_EVENT_ROUTING_BEGIN_ROOT_WIRE) {
      return true;
    }
  }
}

bool autoroute_replay_backward(AutoRoute *ar) {
  if (ar->recording.currentEvent == 0) {
    return false;
  }

  int currentEvent = ar->recording.currentEvent - 1;
  autoroute_replay_forward_to(ar, currentEvent);

  return true;
}

bool autoroute_replay_backward_skip_path(AutoRoute *ar) {
  int currentEvent = ar->recording.currentEvent - 1;
  while (currentEvent >= 0) {
    currentEvent--;
    if (
      ar->recording.events[currentEvent].type == REC_EVENT_BEGIN_PATH_FINDING) {
      break;
    }
  }
  if (currentEvent < 0) {
    currentEvent = 0;
  }
  autoroute_replay_forward_to(ar, currentEvent);

  return true;
}

bool autoroute_replay_backward_skip_root(AutoRoute *ar) {
  int currentEvent = ar->recording.currentEvent - 1;
  while (currentEvent >= 0) {
    currentEvent--;
    if (
      ar->recording.events[currentEvent].type ==
      REC_EVENT_ROUTING_BEGIN_ROOT_WIRE) {
      break;
    }
  }
  if (currentEvent < 0) {
    currentEvent = 0;
  }
  autoroute_replay_forward_to(ar, currentEvent);

  return true;
}

int autoroute_replay_current_event(AutoRoute *ar) {
  return ar->recording.currentEvent;
}

int autoroute_replay_event_count(AutoRoute *ar) {
  return arrlen(ar->recording.events);
}

static HMM_Vec2 pt2vec2(RT_Point p) { return HMM_V2(p.x, p.y); }

void autoroute_replay_draw(AutoRoute *ar, DrawContext *ctx, FontHandle font) {
  RouteRecording *rec = &ar->recording;

  if (rec->rootWireValid) {
    draw_stroked_line(
      ctx, pt2vec2(rec->rootWireStart), pt2vec2(rec->rootWireEnd), 1.0,
      HMM_V4(0.5f, 0.0f, 1.0f, 0.5f));
  }

  if (rec->branchWireValid) {
    draw_stroked_line(
      ctx, pt2vec2(rec->branchWireStart), pt2vec2(rec->branchWireEnd), 1.0,
      HMM_V4(0.5f, 1.0f, 0.0f, 0.5f));
  }

  for (size_t i = 0; i < arrlen(rec->routePaths); i++) {
    RoutePath *path = &rec->routePaths[i];
    for (size_t j = path->start + 1; j < path->end; j++) {
      RT_Vertex v1 = rec->routeVertices[j - 1];
      RT_Vertex v2 = rec->routeVertices[j];
      draw_stroked_line(
        ctx, HMM_V2(v1.x, v1.y), HMM_V2(v2.x, v2.y), 1.5,
        path->root ? HMM_V4(0.7f, 1.0f, 0.0f, 0.6f)
                   : HMM_V4(0.0f, 1.0f, 0.0f, 0.6f));
    }
  }

  if (rec->inPathFinding) {
    for (size_t i = 0; i < hmlen(rec->predecessors); i++) {
      RT_NodeIndex node = rec->predecessors[i].key;
      RT_NodeIndex pred = rec->predecessors[i].value;
      RT_Point p1 = rec->graph.ptr[node].position;
      RT_Point p2 = rec->graph.ptr[pred].position;
      draw_stroked_line(
        ctx, pt2vec2(p1), pt2vec2(p2), 0.5, HMM_V4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    for (size_t i = 1; i < arrlen(rec->path); i++) {
      RT_Point p1 = rec->graph.ptr[rec->path[i - 1]].position;
      RT_Point p2 = rec->graph.ptr[rec->path[i]].position;
      draw_stroked_line(
        ctx, pt2vec2(p1), pt2vec2(p2), 0.8, HMM_V4(0.2f, 1.0f, 0.2f, 0.5f));
    }

    RT_Point start = rec->graph.ptr[rec->startNode].position;
    draw_stroked_circle(
      ctx, HMM_V2(start.x - 3.5, start.y - 3.5), HMM_V2(7, 7), 2.0f,
      HMM_V4(1.0f, 0.2f, 0.2f, 0.5f));

    for (size_t i = 0; i < arrlen(rec->endNodes); i++) {
      RT_Point end = rec->graph.ptr[rec->endNodes[i]].position;
      draw_stroked_circle(
        ctx, HMM_V2(end.x - 3.5, end.y - 3.5), HMM_V2(7, 7), 2.0f,
        HMM_V4(0.2f, 0.2f, 1.0f, 0.5f));
    }

    for (ptrdiff_t i = 0; i < hmlen(rec->gScores); i++) {
      RT_NodeIndex node = rec->gScores[i].key;
      uint32_t gScore = rec->gScores[i].value;
      RT_Point p = rec->graph.ptr[node].position;
      draw_filled_circle(
        ctx, HMM_V2((float)p.x - 2.5, (float)p.y - 2.5), HMM_V2(5, 5),
        HMM_V4(0.5f, 1.0f, 1.0f, 0.5f));
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", gScore);

      Box bounds = draw_text_bounds(
        ctx, HMM_V2(p.x + 3, p.y - 3), buf, strlen(buf), ALIGN_LEFT,
        ALIGN_BOTTOM, 4.0f, font);

      draw_text(
        ctx, bounds, buf, strlen(buf), 4.0f, font,
        HMM_V4(0.7f, 0.7f, 0.7f, 1.0f), HMM_V4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    for (ptrdiff_t i = 0; i < hmlen(rec->fScores); i++) {
      RT_NodeIndex node = rec->fScores[i].key;
      uint32_t fScore = rec->fScores[i].value;
      RT_Point p = rec->graph.ptr[node].position;
      draw_filled_circle(
        ctx, HMM_V2((float)p.x - 2.5, (float)p.y - 2.5), HMM_V2(5, 5),
        HMM_V4(1.0f, 0.5f, 1.0f, 0.5f));
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", fScore);

      Box bounds = draw_text_bounds(
        ctx, HMM_V2(p.x + 3, p.y + 3), buf, strlen(buf), ALIGN_LEFT, ALIGN_TOP,
        4.0f, font);

      draw_text(
        ctx, bounds, buf, strlen(buf), 4.0f, font,
        HMM_V4(0.7f, 0.7f, 0.7f, 1.0f), HMM_V4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    for (ptrdiff_t i = 0; i < hmlen(rec->poppedScores); i++) {
      RT_NodeIndex node = rec->poppedScores[i].key;
      uint32_t fScore = rec->poppedScores[i].value;
      RT_Point p = rec->graph.ptr[node].position;
      draw_filled_circle(
        ctx, HMM_V2((float)p.x - 2.5, (float)p.y - 2.5), HMM_V2(5, 5),
        HMM_V4(0.5f, 1.0f, 0.5f, 0.5f));
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", fScore);

      Box bounds = draw_text_bounds(
        ctx, HMM_V2(p.x + 3, p.y + 3), buf, strlen(buf), ALIGN_LEFT, ALIGN_TOP,
        4.0f, font);

      draw_text(
        ctx, bounds, buf, strlen(buf), 4.0f, font,
        HMM_V4(0.7f, 1.0f, 0.7f, 1.0f), HMM_V4(0.0f, 0.0f, 0.0f, 0.0f));
    }
  }
}

void autoroute_replay_event_text(AutoRoute *ar, char *buf, size_t maxlen) {
  RouteRecording *rec = &ar->recording;
  if (rec->currentEvent >= arrlen(rec->events)) {
    snprintf(buf, maxlen, "End of recording");
    return;
  }

  RecEvent *event = &rec->events[rec->currentEvent];
  switch (event->type) {
  case REC_EVENT_BEGIN_PATH_FINDING:
    snprintf(
      buf, maxlen, "Begin path finding from %d",
      event->begin_path_finding.start_index);
    break;

  case REC_EVENT_PATH_FINDING_SET_G_SCORE:
    snprintf(
      buf, maxlen, "Set G score for %d to %d",
      event->path_finding_set_g_score.node,
      event->path_finding_set_g_score.g_score);
    break;

  case REC_EVENT_PATH_FINDING_PUSH_OPEN_QUEUE:
    snprintf(
      buf, maxlen, "Push %d to open queue with F score %d",
      event->path_finding_push_open_queue.node,
      event->path_finding_push_open_queue.f_score);
    break;

  case REC_EVENT_PATH_FINDING_SET_PREDECESSOR:
    snprintf(
      buf, maxlen, "Set predecessor of %d to %d",
      event->path_finding_set_predecessor.node,
      event->path_finding_set_predecessor.predecessor);
    break;

  case REC_EVENT_PATH_FINDING_POP_OPEN_QUEUE:
    snprintf(
      buf, maxlen, "Pop %d from open queue",
      event->path_finding_pop_open_queue.node);
    break;

  case REC_EVENT_PATH_FINDING_CLEAR_STATE:
    snprintf(buf, maxlen, "Clear path finding state");
    break;

  case REC_EVENT_PATH_FINDING_INSERT_PATH_NODE:
    snprintf(
      buf, maxlen, "Insert %d into path at index %zu",
      event->path_finding_insert_path_node.node,
      event->path_finding_insert_path_node.index);
    break;

  case REC_EVENT_PATH_FINDING_REMOVE_PATH_NODE:
    snprintf(
      buf, maxlen, "Remove node at index %zu from path",
      event->path_finding_remove_path_node.index);
    break;

  case REC_EVENT_END_PATH_FINDING:
    snprintf(buf, maxlen, "End path finding");
    break;

  case REC_EVENT_ROUTING_BEGIN_ROOT_WIRE:
    snprintf(
      buf, maxlen, "Begin root wire from (%d, %d) to (%d, %d)",
      event->routing_begin_root_wire.start.x,
      event->routing_begin_root_wire.start.y,
      event->routing_begin_root_wire.end.x,
      event->routing_begin_root_wire.end.y);
    break;

  case REC_EVENT_ROUTING_BEGIN_BRANCH_WIRE:
    snprintf(
      buf, maxlen, "Begin branch wire from %d, %d",
      event->routing_begin_branch_wire.start.x,
      event->routing_begin_branch_wire.start.y);
    break;

  case REC_EVENT_ROUTING_PUSH_VERTEX:
    snprintf(
      buf, maxlen, "Push vertex %.1f, %.1f to wire",
      event->routing_push_vertex.vertex.x, event->routing_push_vertex.vertex.y);
    break;

  case REC_EVENT_ROUTING_END_WIRE_SEGMENT:
    snprintf(buf, maxlen, "End wire segment");
    break;

  case REC_EVENT_ROUTING_END_WIRE:
    snprintf(buf, maxlen, "End wire");
    break;
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
    // TODO: fixme
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
  fprintf(fp, "const WAYPOINTS: &[Point] = &[\n");
  for (size_t i = 0; i < arrlen(ar->waypoints); i++) {
    RT_Point *waypoint = &ar->waypoints[i];
    fprintf(
      fp, "    Point { x: %d, y: %d },\n", (int)(waypoint->x),
      (int)(waypoint->y));
  }
  fprintf(fp, "];\n");
  fclose(fp);
}

static void rec_begin_path_finding(
  void *user, RT_NodeIndex start_index, struct RT_Slice_NodeIndex end_indices,
  bool visit_all) {
  RouteRecording *rec = user;
  arr(RT_NodeIndex) endIndices = NULL;
  arrsetlen(endIndices, end_indices.len);
  for (size_t i = 0; i < end_indices.len; i++) {
    endIndices[i] = end_indices.ptr[i];
  }
  RecEvent event = {
    .type = REC_EVENT_BEGIN_PATH_FINDING,
    .begin_path_finding =
      {
        .start_index = start_index,
        .end_indices = endIndices,
        .visit_all = visit_all,
      },
  };
  arrput(rec->events, event);
}

static void
rec_path_finding_set_g_score(void *user, RT_NodeIndex index, uint32_t g_score) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_SET_G_SCORE,
    .path_finding_set_g_score = {.node = index, .g_score = g_score},
  };
  arrput(rec->events, event);
}

static void rec_path_finding_push_open_queue(
  void *user, RT_NodeIndex node, uint32_t f_score) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_PUSH_OPEN_QUEUE,
    .path_finding_push_open_queue = {.node = node, .f_score = f_score},
  };
  arrput(rec->events, event);
}

static void rec_path_finding_set_predecessor(
  void *user, RT_NodeIndex node, RT_NodeIndex predecessor) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_SET_PREDECESSOR,
    .path_finding_set_predecessor = {.node = node, .predecessor = predecessor},
  };
  arrput(rec->events, event);
}

static void rec_path_finding_pop_open_queue(void *user, RT_NodeIndex node) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_POP_OPEN_QUEUE,
    .path_finding_pop_open_queue = {.node = node},
  };
  arrput(rec->events, event);
}

static void rec_path_finding_clear_state(void *user) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_CLEAR_STATE,
  };
  arrput(rec->events, event);
}

static void rec_path_finding_insert_path_node(
  void *user, size_t insert_index, RT_NodeIndex node) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_INSERT_PATH_NODE,
    .path_finding_insert_path_node = {.index = insert_index, .node = node},
  };
  arrput(rec->events, event);
}

static void rec_path_finding_remove_path_node(void *user, size_t index) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_PATH_FINDING_REMOVE_PATH_NODE,
    .path_finding_remove_path_node = {.index = index},
  };
  arrput(rec->events, event);
}

static void rec_end_path_finding(void *user, bool found) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_END_PATH_FINDING,
    .end_path_finding = {.found = found},
  };
  arrput(rec->events, event);
}

static void rec_routing_begin_root_wire(
  void *user, struct RT_Point start, struct RT_Point end) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_ROUTING_BEGIN_ROOT_WIRE,
    .routing_begin_root_wire = {.start = start, .end = end},
  };
  arrput(rec->events, event);
}

static void rec_routing_begin_branch_wire(void *user, struct RT_Point start) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_ROUTING_BEGIN_BRANCH_WIRE,
    .routing_begin_branch_wire = {.start = start},
  };
  arrput(rec->events, event);
}

static void rec_routing_push_vertex(void *user, struct RT_Vertex vertex) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_ROUTING_PUSH_VERTEX,
    .routing_push_vertex = {.vertex = vertex},
  };
  arrput(rec->events, event);
}

static void rec_routing_end_wire_segment(void *user, bool ends_in_junction) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_ROUTING_END_WIRE_SEGMENT,
    .routing_end_wire_segment = {.ends_in_junction = ends_in_junction},
  };
  arrput(rec->events, event);
}

static void rec_routing_end_wire(void *user) {
  RouteRecording *rec = user;
  RecEvent event = {
    .type = REC_EVENT_ROUTING_END_WIRE,
  };
  arrput(rec->events, event);
}

static void rec_callbacks(RT_ReplayCallbacks *callbacks, RouteRecording *rec) {
  arrsetlen(rec->events, 0);
  *callbacks = (RT_ReplayCallbacks){
    .context = rec,
    .begin_path_finding = rec_begin_path_finding,
    .path_finding_set_g_score = rec_path_finding_set_g_score,
    .path_finding_push_open_queue = rec_path_finding_push_open_queue,
    .path_finding_set_predecessor = rec_path_finding_set_predecessor,
    .path_finding_pop_open_queue = rec_path_finding_pop_open_queue,
    .path_finding_clear_state = rec_path_finding_clear_state,
    .path_finding_insert_path_node = rec_path_finding_insert_path_node,
    .path_finding_remove_path_node = rec_path_finding_remove_path_node,
    .end_path_finding = rec_end_path_finding,
    .routing_begin_root_wire = rec_routing_begin_root_wire,
    .routing_begin_branch_wire = rec_routing_begin_branch_wire,
    .routing_push_vertex = rec_routing_push_vertex,
    .routing_end_wire_segment = rec_routing_end_wire_segment,
    .routing_end_wire = rec_routing_end_wire,
  };
}