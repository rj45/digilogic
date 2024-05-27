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
  Circuit *circuit;

  RT_Net *nets;
  RT_NetView *netViews;
  RT_Endpoint *endpoints;
  RT_Waypoint *waypoints;
  RT_BoundingBox *boxes;

  arr(RT_Anchor) anchors;

  // todo: this is for nudging, so that we can diff the new state from the
  // previous state and know what wires to update in the BVH
  arr(Wire) prevWires;
  arr(HMM_Vec2) prevVertices;

  RT_Graph *graph;

  int timeIndex;
  int timeLength;
  uint64_t buildTimes[TIME_SAMPLES];
  uint64_t routeTimes[TIME_SAMPLES];
};

void autoroute_global_init() {
  RT_Result res = RT_init_thread_pool();
  assert(res == RT_RESULT_SUCCESS);
}

static void
autoroute_on_component_update(void *user, ComponentID id, void *ptr) {
  AutoRoute *ar = user;
  Component *comp = ptr;

  RT_BoundingBox *box = &ar->boxes[circuit_index(ar->circuit, id)];
  assert(!isnan(comp->box.center.X));
  assert(!isnan(comp->box.center.Y));
  assert(!isnan(comp->box.halfSize.X));
  assert(!isnan(comp->box.halfSize.Y));
  *box = (RT_BoundingBox){
    .center =
      {
        .x = comp->box.center.X,
        .y = comp->box.center.Y,
      },
    .half_width = (uint16_t)(comp->box.halfSize.X + RT_PADDING) - 1,
    .half_height = (uint16_t)(comp->box.halfSize.Y + RT_PADDING) - 1,
  };
  log_debug(
    "Updating component %d to %f %f", id, comp->box.center.X,
    comp->box.center.Y);

  // todo: this belongs in core?
  PortID portID = comp->portFirst;
  while (portID) {
    Port *port = circuit_port_ptr(ar->circuit, portID);

    HMM_Vec2 pos = HMM_AddV2(comp->box.center, port->position);

    if (circuit_has(ar->circuit, port->endpoint)) {
      Endpoint *endpoint = circuit_endpoint_ptr(ar->circuit, port->endpoint);
      endpoint->position = pos;
      circuit_update_id(ar->circuit, port->endpoint);
    }

    portID = port->next;
  }
}

static void autoroute_on_net_update(void *user, NetID id, void *ptr) {
  AutoRoute *ar = user;
  Net *net = ptr;

  RT_Net *rtNet = &ar->nets[circuit_index(ar->circuit, id)];
  rtNet->first_endpoint = RT_INVALID_ENDPOINT_INDEX;
  if (circuit_has(ar->circuit, net->endpointFirst)) {
    rtNet->first_endpoint = circuit_index(ar->circuit, net->endpointFirst);
  }
  rtNet->first_waypoint = RT_INVALID_WAYPOINT_INDEX;
  if (circuit_has(ar->circuit, net->waypointFirst)) {
    rtNet->first_waypoint = circuit_index(ar->circuit, net->waypointFirst);
  }
  log_debug("Updating net %x", id);
}

static void autoroute_on_endpoint_update(void *user, EndpointID id, void *ptr) {
  AutoRoute *ar = user;
  Endpoint *endpoint = ptr;

  RT_Endpoint *rtEndpoint = &ar->endpoints[circuit_index(ar->circuit, id)];

  rtEndpoint->next = RT_INVALID_ENDPOINT_INDEX;
  if (circuit_has(ar->circuit, endpoint->next)) {
    rtEndpoint->next = circuit_index(ar->circuit, endpoint->next);
  }

  if (circuit_has(ar->circuit, endpoint->prev)) {
    RT_Endpoint *prevEndpoint =
      &ar->endpoints[circuit_index(ar->circuit, endpoint->prev)];
    prevEndpoint->next = circuit_index(ar->circuit, id);
  }

  log_debug(
    "Setting endpoint %d to %f %f", id, endpoint->position.X,
    endpoint->position.Y);
  rtEndpoint->position = (RT_Point){
    .x = endpoint->position.X,
    .y = endpoint->position.Y,
  };
  autoroute_on_net_update(
    ar, endpoint->net, circuit_net_ptr(ar->circuit, endpoint->net));
}

static void autoroute_on_waypoint_update(void *user, WaypointID id, void *ptr) {
  AutoRoute *ar = user;
  Waypoint *waypoint = ptr;

  RT_Waypoint *rtWaypoint = &ar->waypoints[circuit_index(ar->circuit, id)];

  rtWaypoint->next = RT_INVALID_WAYPOINT_INDEX;
  if (circuit_has(ar->circuit, waypoint->next)) {
    rtWaypoint->next = circuit_index(ar->circuit, waypoint->next);
  }

  if (circuit_has(ar->circuit, waypoint->prev)) {
    RT_Waypoint *prevWaypoint =
      &ar->waypoints[circuit_index(ar->circuit, waypoint->prev)];
    prevWaypoint->next = circuit_index(ar->circuit, id);
  }

  rtWaypoint->position = (RT_Point){
    .x = waypoint->position.X,
    .y = waypoint->position.Y,
  };
  log_debug(
    "Setting waypoint %x to %f %f", id, waypoint->position.X,
    waypoint->position.Y);
  autoroute_on_net_update(
    ar, waypoint->net, circuit_net_ptr(ar->circuit, waypoint->net));
}

AutoRoute *autoroute_create(Circuit *circuit) {
  AutoRoute *ar = malloc(sizeof(AutoRoute));
  *ar = (AutoRoute){
    .circuit = circuit,
  };
  smap_add_synced_array(
    &circuit->sm.components, (void **)&ar->boxes, sizeof(*ar->boxes));
  circuit_on_component_create(circuit, ar, autoroute_on_component_update);
  circuit_on_component_update(circuit, ar, autoroute_on_component_update);

  smap_add_synced_array(
    &circuit->sm.nets, (void **)&ar->nets, sizeof(*ar->nets));
  smap_add_synced_array(
    &circuit->sm.nets, (void **)&ar->netViews, sizeof(*ar->netViews));
  circuit_on_net_create(circuit, ar, autoroute_on_net_update);
  circuit_on_net_update(circuit, ar, autoroute_on_net_update);

  smap_add_synced_array(
    &circuit->sm.endpoints, (void **)&ar->endpoints, sizeof(*ar->endpoints));
  circuit_on_endpoint_create(circuit, ar, autoroute_on_endpoint_update);
  circuit_on_endpoint_update(circuit, ar, autoroute_on_endpoint_update);

  smap_add_synced_array(
    &circuit->sm.waypoints, (void **)&ar->waypoints, sizeof(*ar->waypoints));
  circuit_on_waypoint_create(circuit, ar, autoroute_on_waypoint_update);
  circuit_on_waypoint_update(circuit, ar, autoroute_on_waypoint_update);

  RT_Result res = RT_graph_new(&ar->graph);
  assert(res == RT_RESULT_SUCCESS);

  return ar;
}

void autoroute_free(AutoRoute *ar) {
  arrfree(ar->anchors);

  RT_Result res = RT_graph_free(ar->graph);
  assert(res == RT_RESULT_SUCCESS);
  free(ar);
}

static void autoroute_update_anchors(AutoRoute *ar) {
  arrsetlen(ar->anchors, 0);
  for (int i = 0; i < circuit_component_len(ar->circuit); i++) {
    Component *comp = &ar->circuit->components[i];
    Box box = comp->box;

    float cx = box.center.X;
    float cy = box.center.Y;

    PortID portID = comp->portFirst;
    while (circuit_has(ar->circuit, portID)) {
      Port *port = circuit_port_ptr(ar->circuit, portID);

      PortDesc *portDesc =
        &ar->circuit->componentDescs[comp->desc].ports[port->desc];
      RT_Directions directions = portDesc->direction == PORT_IN
                                   ? RT_DIRECTIONS_NEG_X
                                   : RT_DIRECTIONS_POS_X;

      RT_Anchor anchor = (RT_Anchor){
        .bounding_box = i,
        .position =
          {
            .x = cx + port->position.X,
            .y = cy + port->position.Y,
          },
        .connect_directions = directions,
      };
      arrput(ar->anchors, anchor);
      assert(!isnan(cx + port->position.X));
      assert(!isnan(cy + port->position.Y));
      assert(anchor.position.x != 0 || anchor.position.y != 0);

      if (circuit_has(ar->circuit, port->endpoint)) {
        RT_Endpoint *endpoint =
          &ar->endpoints[circuit_index(ar->circuit, port->endpoint)];
        assert(endpoint->position.x == anchor.position.x);
        assert(endpoint->position.y == anchor.position.y);
      }

      portID = port->next;
    }
  }
  for (int i = 0; i < circuit_endpoint_len(ar->circuit); i++) {
    Endpoint *endpoint = &ar->circuit->endpoints[i];
    if (circuit_has(ar->circuit, endpoint->port)) {
      continue;
    }

    RT_BoundingBoxIndex boundingBox = RT_INVALID_BOUNDING_BOX_INDEX;
    RT_Directions connectDirections = RT_DIRECTIONS_ALL;

    // for (int j = 0; j < circuit_component_len(ar->circuit); j++) {
    //   Component *comp = &ar->circuit->components[j];
    //   Box box = comp->box;

    //   if (box_intersect_point(box, endpoint->position)) {
    //     boundingBox = j;

    //     if (endpoint->position.X < box.center.X) {
    //       connectDirections = RT_DIRECTIONS_NEG_X;
    //     } else {
    //       connectDirections = RT_DIRECTIONS_POS_X;
    //     }

    //     break;
    //   }
    // }

    RT_Anchor anchor = (RT_Anchor){
      .position =
        {
          .x = endpoint->position.X,
          .y = endpoint->position.Y,
        },
      .connect_directions = connectDirections,
      .bounding_box = boundingBox,
    };
    arrput(ar->anchors, anchor);
    assert(!isnan(endpoint->position.X));
    assert(!isnan(endpoint->position.Y));
    assert(anchor.position.x != 0 || anchor.position.y != 0);
  }
  for (int i = 0; i < circuit_waypoint_len(ar->circuit); i++) {
    Waypoint *waypoint = &ar->circuit->waypoints[i];
    RT_Anchor anchor = (RT_Anchor){
      .position =
        {
          .x = waypoint->position.X,
          .y = waypoint->position.Y,
        },
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    arrput(ar->anchors, anchor);
  }
}

static void autoroute_prepare_routing(AutoRoute *ar, RoutingConfig config) {
  autoroute_update_anchors(ar);

  if (arrlen(ar->anchors) == 0) {
    return;
  }

  RT_Result res = RT_graph_build(
    ar->graph, (RT_Slice_Anchor){ar->anchors, arrlen(ar->anchors)},
    (RT_Slice_BoundingBox){ar->boxes, circuit_component_len(ar->circuit)},
    config.minimizeGraph);
  if (res != RT_RESULT_SUCCESS) {
    log_error("Error building graph: %d", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  if (arrlen(ar->circuit->vertices) == 0) {
    arrsetlen(ar->circuit->vertices, 1024);
    arrsetlen(ar->prevVertices, 1024);
  }
  if (arrlen(ar->circuit->wires) == 0) {
    arrsetlen(ar->circuit->wires, 1024);
    arrsetlen(ar->prevWires, 1024);
  }

  assert(ar->graph);
  assert(ar->circuit->wires);
  assert(ar->circuit->vertices);

  // todo: remove this checking code
  for (int netIdx = 0; netIdx < circuit_net_len(ar->circuit); netIdx++) {
    RT_Net *net = &ar->nets[netIdx];
    assert(net->first_endpoint != RT_INVALID_ENDPOINT_INDEX);
    int count = 0;
    // RT_Point prevPoint = ar->endpoints[net->first_endpoint].position;
    // int dist = 0;
    RT_EndpointIndex endpointIdx = net->first_endpoint;
    while (endpointIdx != RT_INVALID_ENDPOINT_INDEX) {
      count++;
      // dist += HMM_ABS(prevPoint.x - ar->endpoints[endpointIdx].position.x) +
      //         HMM_ABS(prevPoint.y - ar->endpoints[endpointIdx].position.y);
      // prevPoint = ar->endpoints[endpointIdx].position;
      endpointIdx = ar->endpoints[endpointIdx].next;
    }
    assert(count > 1);
    // assert(dist > 0);
  }
}

bool autoroute_dump_routing_data(
  AutoRoute *ar, RoutingConfig config, const char *filename) {
  autoroute_prepare_routing(ar, config);
  RT_Result res = RT_graph_serialize_connect_nets_query(
    ar->graph, (RT_Slice_Net){ar->nets, circuit_net_len(ar->circuit)},
    (RT_Slice_Endpoint){ar->endpoints, circuit_endpoint_len(ar->circuit)},
    (RT_Slice_Waypoint){ar->waypoints, circuit_waypoint_len(ar->circuit)},
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
    ar->prevWires = ar->circuit->wires;
    ar->circuit->wires = tmp;
  }

  {
    // swap vertices
    arr(HMM_Vec2) tmp = ar->prevVertices;
    ar->prevVertices = ar->circuit->vertices;
    ar->circuit->vertices = tmp;
  }

  RT_Result res;
  for (;;) {
    res = RT_graph_connect_nets(
      ar->graph, (RT_Slice_Net){ar->nets, circuit_net_len(ar->circuit)},
      (RT_Slice_Endpoint){ar->endpoints, circuit_endpoint_len(ar->circuit)},
      (RT_Slice_Waypoint){ar->waypoints, circuit_waypoint_len(ar->circuit)},
      (RT_MutSlice_Vertex){
        (RT_Vertex *)ar->circuit->vertices,
        arrlen(ar->circuit->vertices),
      },
      (RT_MutSlice_WireView){
        (RT_WireView *)ar->circuit->wires,
        arrlen(ar->circuit->wires),
      },
      (RT_MutSlice_NetView){
        ar->netViews,
        circuit_net_len(ar->circuit),
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
      arrsetlen(ar->circuit->vertices, arrlen(ar->circuit->vertices) * 2);
      arrsetlen(ar->prevVertices, arrlen(ar->prevVertices) * 2);
      continue;
    case RT_RESULT_WIRE_VIEW_BUFFER_OVERFLOW_ERROR:
      arrsetlen(ar->circuit->wires, arrlen(ar->circuit->wires) * 2);
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

  for (int i = 0; i < circuit_net_len(ar->circuit); i++) {
    RT_NetView *rtNetView = &ar->netViews[i];
    Net *net = &ar->circuit->nets[i];

    net->wireOffset = rtNetView->wire_offset;
    net->wireCount = rtNetView->wire_count;
    net->vertexOffset = rtNetView->vertex_offset;
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

  for (uint32_t i = 0; i < circuit_component_len(ar->circuit); i++) {
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
  for (size_t i = 0; i < circuit_component_len(ar->circuit); i++) {
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
  for (size_t i = 0; i < circuit_endpoint_len(ar->circuit); i++) {
    Endpoint *endpoint = &ar->circuit->endpoints[i];
    fprintf(fp, "    Endpoint {\n");
    if (!circuit_has(ar->circuit, endpoint->net)) {
      fprintf(fp, "        NetIndex: NetIndex::INVALID,\n");
    } else {
      fprintf(
        fp, "        NetIndex: ni!(%d),\n",
        circuit_index(ar->circuit, endpoint->net));
    }
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n",
      (int)(endpoint->position.X), (int)(endpoint->position.Y));
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fprintf(fp, "const WAYPOINTS: &[Waypoint] = &[\n");
  for (size_t i = 0; i < circuit_waypoint_len(ar->circuit); i++) {
    Waypoint *waypoint = &ar->circuit->waypoints[i];
    fprintf(fp, "    Waypoint {\n");
    if (!circuit_has(ar->circuit, waypoint->net)) {
      fprintf(fp, "        NetIndex: NetIndex::INVALID,\n");
    } else {
      fprintf(
        fp, "        NetIndex: ni!(%d),\n",
        circuit_index(ar->circuit, waypoint->net));
    }
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n",
      (int)(waypoint->position.X), (int)(waypoint->position.Y));
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fclose(fp);
}