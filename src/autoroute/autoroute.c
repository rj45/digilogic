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

struct AutoRoute {
  CircuitView *view;

  RT_Net *nets;
  RT_Endpoint *endpoints;
  RT_Waypoint *waypoints;
  RT_BoundingBox *boxes;

  arr(RT_Anchor) anchors;

  RT_Graph *graph;
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
    &view->circuit.sm.nets, (void **)&ar->nets, sizeof(*ar->nets));
  smap_add_synced_array(
    &view->circuit.sm.endpoints, (void **)&ar->endpoints,
    sizeof(*ar->endpoints));
  smap_add_synced_array(
    &view->circuit.sm.waypoints, (void **)&ar->waypoints,
    sizeof(*ar->waypoints));

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

      if (port->endpoint != NO_ENDPOINT) {
        RT_Endpoint *endpoint = &ar->endpoints[circuit_endpoint_index(
          &ar->view->circuit, port->endpoint)];
        assert(endpoint->position.x == anchor.position.x);
        assert(endpoint->position.y == anchor.position.y);
      }

      portID = port->compNext;
    }
  }
  for (int i = 0; i < circuit_waypoint_len(&ar->view->circuit); i++) {
    WaypointView *waypointView = &ar->view->waypoints[i];
    RT_Anchor anchor = (RT_Anchor){
      .position =
        {
          .x = waypointView->pos.X,
          .y = waypointView->pos.Y,
        },
      .connect_directions = RT_DIRECTIONS_ALL,
      .bounding_box = RT_INVALID_BOUNDING_BOX_INDEX,
    };
    arrput(ar->anchors, anchor);
  }
}

void autoroute_update_component(AutoRoute *ar, ComponentID id) {
  ComponentView *compView = view_component_ptr(ar->view, id);
  RT_BoundingBox *box =
    &ar->boxes[circuit_component_index(&ar->view->circuit, id)];
  assert(!isnan(compView->box.center.X));
  assert(!isnan(compView->box.center.Y));
  assert(!isnan(compView->box.halfSize.X));
  assert(!isnan(compView->box.halfSize.Y));
  *box = (RT_BoundingBox){
    .center =
      {
        .x = compView->box.center.X,
        .y = compView->box.center.Y,
      },
    .half_width = (uint16_t)(compView->box.halfSize.X + RT_PADDING) - 1,
    .half_height = (uint16_t)(compView->box.halfSize.Y + RT_PADDING) - 1,
  };
  log_debug(
    "Updating component %d to %f %f", id, compView->box.center.X,
    compView->box.center.Y);
}

void autoroute_update_net(AutoRoute *ar, NetID id) {
  Net *net = circuit_net_ptr(&ar->view->circuit, id);
  RT_Net *rtNet = &ar->nets[circuit_net_index(&ar->view->circuit, id)];
  rtNet->first_endpoint = RT_INVALID_ENDPOINT_INDEX;
  if (net->endpointFirst != NO_ENDPOINT) {
    rtNet->first_endpoint =
      circuit_endpoint_index(&ar->view->circuit, net->endpointFirst);
  }
  rtNet->first_waypoint = RT_INVALID_WAYPOINT_INDEX;
  if (net->waypointFirst != NO_WAYPOINT) {
    rtNet->first_waypoint =
      circuit_waypoint_index(&ar->view->circuit, net->waypointFirst);
  }
  log_debug("Updating net %x", id);
}

void autoroute_update_endpoint(AutoRoute *ar, EndpointID id) {
  Endpoint *endpoint = circuit_endpoint_ptr(&ar->view->circuit, id);
  EndpointView *endpointView = view_endpoint_ptr(ar->view, id);

  RT_Endpoint *rtEndpoint =
    &ar->endpoints[circuit_endpoint_index(&ar->view->circuit, id)];

  rtEndpoint->next = RT_INVALID_ENDPOINT_INDEX;
  if (endpoint->next != NO_ENDPOINT) {
    rtEndpoint->next =
      circuit_endpoint_index(&ar->view->circuit, endpoint->next);
  }

  if (endpoint->prev != NO_ENDPOINT) {
    RT_Endpoint *prevEndpoint = &ar->endpoints[circuit_endpoint_index(
      &ar->view->circuit, endpoint->prev)];
    prevEndpoint->next = circuit_endpoint_index(&ar->view->circuit, id);
  }

  log_debug(
    "Setting endpoint %d to %f %f", id, endpointView->pos.X,
    endpointView->pos.Y);
  rtEndpoint->position = (RT_Point){
    .x = endpointView->pos.X,
    .y = endpointView->pos.Y,
  };
  autoroute_update_net(ar, endpoint->net);
}

void autoroute_update_waypoint(AutoRoute *ar, WaypointID id) {
  Waypoint *waypoint = circuit_waypoint_ptr(&ar->view->circuit, id);
  WaypointView *waypointView = view_waypoint_ptr(ar->view, id);

  RT_Waypoint *rtWaypoint =
    &ar->waypoints[circuit_waypoint_index(&ar->view->circuit, id)];

  rtWaypoint->next = RT_INVALID_WAYPOINT_INDEX;
  if (waypoint->next != NO_WAYPOINT) {
    rtWaypoint->next =
      circuit_waypoint_index(&ar->view->circuit, waypoint->next);
  }

  if (waypoint->prev != NO_ENDPOINT) {
    RT_Waypoint *prevWaypoint = &ar->waypoints[circuit_waypoint_index(
      &ar->view->circuit, waypoint->prev)];
    prevWaypoint->next = circuit_waypoint_index(&ar->view->circuit, id);
  }

  rtWaypoint->position = (RT_Point){
    .x = waypointView->pos.X,
    .y = waypointView->pos.Y,
  };
  log_debug(
    "Setting waypoint %x to %f %f", id, waypointView->pos.X,
    waypointView->pos.Y);
  autoroute_update_net(ar, waypoint->net);
}

void autoroute_route(AutoRoute *ar, bool betterRoutes) {
  uint64_t start = stm_now();

  autoroute_update_anchors(ar);

  assert(arrlen(ar->anchors) > 0);
  assert(ar->boxes != NULL);
  assert(circuit_component_len(&ar->view->circuit) > 0);

  RT_Result res = RT_graph_build(
    ar->graph, (RT_Slice_Anchor){ar->anchors, arrlen(ar->anchors)},
    (RT_Slice_BoundingBox){
      ar->boxes, circuit_component_len(&ar->view->circuit)},
    betterRoutes);
  if (res != RT_RESULT_SUCCESS) {
    log_error("Error building graph: %d", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  uint64_t graphBuild = stm_since(start);
  uint64_t pathFindStart = stm_now();

  if (arrlen(ar->view->vertices) == 0) {
    arrsetlen(ar->view->vertices, 1024);
  }
  if (arrlen(ar->view->wires) == 0) {
    arrsetlen(ar->view->wires, 1024);
  }

  memset(ar->view->wires, 0, arrlen(ar->view->wires) * sizeof(RT_WireView));

  assert(ar->endpoints);
  // assert(ar->waypoints);
  assert(ar->graph);
  assert(ar->view->wires);
  assert(ar->view->vertices);

  // todo: remove this checking code
  for (int netIdx = 0; netIdx < circuit_net_len(&ar->view->circuit); netIdx++) {
    RT_Net *net = &ar->nets[netIdx];
    assert(net->first_endpoint != RT_INVALID_ENDPOINT_INDEX);
    int count = 0;
    RT_Point prevPoint = ar->endpoints[net->first_endpoint].position;
    int dist = 0;
    RT_EndpointIndex endpointIdx = net->first_endpoint;
    while (endpointIdx != RT_INVALID_ENDPOINT_INDEX) {
      count++;
      dist += HMM_ABS(prevPoint.x - ar->endpoints[endpointIdx].position.x) +
              HMM_ABS(prevPoint.y - ar->endpoints[endpointIdx].position.y);
      prevPoint = ar->endpoints[endpointIdx].position;
      endpointIdx = ar->endpoints[endpointIdx].next;
    }
    assert(count > 1);
    assert(dist > 0);
  }

  for (;;) {
    res = RT_graph_connect_nets(
      ar->graph, (RT_Slice_Net){ar->nets, circuit_net_len(&ar->view->circuit)},
      (RT_Slice_Endpoint){
        ar->endpoints, circuit_endpoint_len(&ar->view->circuit)},
      (RT_Slice_Waypoint){
        ar->waypoints, circuit_waypoint_len(&ar->view->circuit)},
      (RT_MutSlice_Vertex){
        (RT_Vertex *)ar->view->vertices,
        arrlen(ar->view->vertices),
      },
      (RT_MutSlice_WireView){
        (RT_WireView *)ar->view->wires,
        arrlen(ar->view->wires),
      },
      (RT_MutSlice_NetView){
        (RT_NetView *)ar->view->nets,
        circuit_net_len(&ar->view->circuit),
      });
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

      arrsetlen(ar->view->vertices, arrlen(ar->view->vertices) * 2);
      continue;
    case RT_RESULT_WIRE_VIEW_BUFFER_OVERFLOW_ERROR:
      arrsetlen(ar->view->wires, arrlen(ar->view->wires) * 2);
      continue;
    }
    break;
  }
  assert(res == RT_RESULT_SUCCESS);

  uint64_t pathFind = stm_since(pathFindStart);

  log_info(
    "Graph build: %fms, Path find: %fms", stm_ms(graphBuild), stm_ms(pathFind));
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

  for (uint32_t i = 0; i < circuit_component_len(&ar->view->circuit); i++) {
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
  fprintf(fp, "const PORTS: &[Port] = &[\n");
  for (size_t i = 0; i < circuit_port_len(&ar->view->circuit); i++) {
    Port *port = &ar->view->circuit.ports[i];
    ComponentView *compView = view_component_ptr(ar->view, port->component);
    fprintf(fp, "    Port {\n");
    if (port->net == NO_NET) {
      fprintf(fp, "        NetIndex: NetIndex::INVALID,\n");
    } else {
      fprintf(
        fp, "        NetIndex: ni!(%d),\n",
        circuit_net_index(&ar->view->circuit, port->net));
    }
    PortView *portView = &ar->view->ports[i];
    fprintf(
      fp, "        position: Point { x: %d, y: %d },\n",
      (int)(compView->box.center.X + portView->center.X),
      (int)(compView->box.center.Y + portView->center.Y));
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fclose(fp);
}