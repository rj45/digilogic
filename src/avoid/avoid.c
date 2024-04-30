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

#include "avoid.h"
#include <assert.h>
#include <stdlib.h>

#include "core/core.h"
#include "handmade_math.h"
#include "routing/routing.h"
#include "stb_ds.h"

typedef uint32_t PointID;
typedef uint32_t BoundingBoxID;
typedef uint32_t PathID;

struct AvoidPort {
  PortID id;
  ComponentID componentID;
  HMM_Vec2 pos;
  PointID anchorID;
};

struct AvoidComponent {
  ComponentID id;
  Box box;
  arr(PortID) ports;
  PointID firstAnchor;
  BoundingBoxID boxID;
};

struct AvoidJunction {
  JunctionID id;
  HMM_Vec2 pos;
  PointID anchorID;
};

struct AvoidWire {
  NetID netID;
  WireID edgeID;
  WireEndID srcID;
  WireEndID dstID;
  PathID pathID;
};

struct AvoidState {
  size_t threadCount;
  size_t vertBufferCapacity;
  RT_Graph *graph;

  arr(struct AvoidComponent) components;
  arr(struct AvoidPort) ports;
  arr(struct AvoidJunction) junctions;
  arr(struct AvoidWire) wires;

  arr(RT_Point) anchors;
  arr(RT_BoundingBox) boundingBoxes;

  arr(RT_PathDef) paths;

  arr(RT_VertexBuffer) vertexBuffers;

  Timer timer;
};

AvoidRouter *avoid_new() {
  struct AvoidState *state = malloc(sizeof(struct AvoidState));
  *state = (struct AvoidState){0};

  RT_Result res = RT_init_thread_pool(&state->threadCount);
  assert(res == RT_RESULT_SUCCESS);

  res = RT_graph_new(&state->graph);
  assert(res == RT_RESULT_SUCCESS);

  state->vertBufferCapacity = 1024;

  for (size_t i = 0; i < state->threadCount; i++) {
    arrput(
      state->vertexBuffers,
      ((struct RT_VertexBuffer){
        .vertices = malloc(state->vertBufferCapacity * sizeof(RT_Vertex)),
        .vertex_count = 0,
      }));
  }

  timer_init(&state->timer);

  return state;
}

void avoid_free(AvoidRouter *a) {
  struct AvoidState *state = a;
  if (state->graph) {
    RT_Result res = RT_graph_free(state->graph);
    assert(res == RT_RESULT_SUCCESS);
  }
  for (size_t i = 0; i < arrlen(state->components); i++) {
    arrfree(state->components[i].ports);
  }
  arrfree(state->components);
  arrfree(state->ports);
  arrfree(state->junctions);
  arrfree(state->anchors);
  arrfree(state->wires);
  arrfree(state->boundingBoxes);
  arrfree(state->paths);
  for (size_t i = 0; i < arrlen(state->vertexBuffers); i++) {
    free(state->vertexBuffers[i].vertices);
  }
  arrfree(state->vertexBuffers);
  free(state);
}

#define RT_PADDING 10.0f

void avoid_add_node(
  AvoidRouter *a, ComponentID nodeID, float x, float y, float w, float h) {
  struct AvoidState *state = a;
  float hw = w / 2;
  float hh = h / 2;
  float cx = x + hw;
  float cy = y + hh;

  Box box = (Box){
    .center = HMM_V2(cx, cy),
    .halfSize = HMM_V2(hw, hh),
  };

  BoundingBoxID boxID = arrlen(state->boundingBoxes);
  arrput(
    state->boundingBoxes, ((RT_BoundingBox){
                            .center = {cx, cy},
                            .half_width = hw,
                            .half_height = hh,
                          }));

  HMM_Vec2 halfSize = HMM_AddV2(HMM_V2(hw, hh), HMM_V2(RT_PADDING, RT_PADDING));
  RT_Point tl = (RT_Point){.x = cx - halfSize.X, .y = cy - halfSize.Y};
  RT_Point tr = (RT_Point){.x = cx + halfSize.X, .y = cy - halfSize.Y};
  RT_Point bl = (RT_Point){.x = cx - halfSize.X, .y = cy + halfSize.Y};
  RT_Point br = (RT_Point){.x = cx + halfSize.X, .y = cy + halfSize.Y};

  PointID anchorID = arrlen(state->anchors);
  arrput(state->anchors, tl);
  arrput(state->anchors, tr);
  arrput(state->anchors, bl);
  arrput(state->anchors, br);

  arrins(
    state->components, nodeID,
    ((struct AvoidComponent){
      .id = nodeID,
      .box = box,
      .firstAnchor = anchorID,
      .boxID = boxID,
    }));
}

void avoid_move_node(AvoidRouter *a, ComponentID nodeID, float dx, float dy) {
  struct AvoidState *state = a;
  (void)state;
  state->components[nodeID].box.center.X += dx;
  state->components[nodeID].box.center.Y += dy;
  float x = state->components[nodeID].box.center.X;
  float y = state->components[nodeID].box.center.Y;
  HMM_Vec2 halfSize = HMM_AddV2(
    state->components[nodeID].box.halfSize, HMM_V2(RT_PADDING, RT_PADDING));
  state->anchors[state->components[nodeID].firstAnchor + 0] =
    (RT_Point){.x = x - halfSize.X, .y = y - halfSize.Y};
  state->anchors[state->components[nodeID].firstAnchor + 1] =
    (RT_Point){.x = x + halfSize.X, .y = y - halfSize.Y};
  state->anchors[state->components[nodeID].firstAnchor + 2] =
    (RT_Point){.x = x - halfSize.X, .y = y + halfSize.Y};
  state->anchors[state->components[nodeID].firstAnchor + 3] =
    (RT_Point){.x = x + halfSize.X, .y = y + halfSize.Y};

  RT_BoundingBox *box = &state->boundingBoxes[state->components[nodeID].boxID];
  box->center = (RT_Point){.x = x, .y = y};
  for (PortID portID = 0; portID < arrlen(state->ports); portID++) {
    struct AvoidPort *port = &state->ports[portID];

    if (port->componentID != nodeID) {
      continue;
    }

    HMM_Vec2 pos = HMM_AddV2(HMM_V2(x, y), port->pos);
    state->anchors[port->anchorID] = (RT_Point){.x = pos.X, .y = pos.Y};
    // printf(
    //   "Moving port %d to %d %d - dxdy: %f, %f\n", portID,
    //   state->anchors[port->anchorID].x, state->anchors[port->anchorID].y, dx,
    //   dy);
  }
}

void avoid_add_port(
  AvoidRouter *a, PortID portID, ComponentID nodeID, PortSide side,
  float centerX, float centerY) {
  struct AvoidState *state = a;
  struct AvoidComponent *component = &state->components[nodeID];
  PointID anchorID = arrlen(state->anchors);

  centerX -= component->box.halfSize.X;
  centerY -= component->box.halfSize.Y;

  arrput(
    state->anchors, ((RT_Point){
                      .x = centerX + component->box.center.X,
                      .y = centerY + component->box.center.Y}));

  // printf(
  //   "Adding port %d to component %d, pos: %f, %f anchor: %d, %d\n", portID,
  //   nodeID, centerX, centerY, state->anchors[anchorID].x,
  //   state->anchors[anchorID].y);

  arrins(
    state->ports, portID,
    ((struct AvoidPort){
      .id = portID,
      .componentID = nodeID,
      .pos = HMM_V2(centerX, centerY),
      .anchorID = anchorID,
    }));

  arrput(component->ports, portID);
}

void avoid_add_junction(
  AvoidRouter *a, JunctionID junctionID, float x, float y) {
  struct AvoidState *state = a;
  PointID anchorID = arrlen(state->anchors);
  arrput(state->anchors, ((RT_Point){.x = x, .y = y}));

  arrins(
    state->junctions, junctionID,
    ((struct AvoidJunction){
      .id = junctionID,
      .pos = HMM_V2(x, y),
      .anchorID = anchorID,
    }));
}

void avoid_add_edge(
  AvoidRouter *a, NetID netID, WireID edgeID, WireEndID srcID, WireEndID dstID,
  float x1, float y1, float x2, float y2) {
  struct AvoidState *state = a;

  RT_PathDef path = {
    .net_id = edgeID,
    .start = {0},
    .end = {0},
  };
  PathID pathID = arrlen(state->paths);
  arrput(state->paths, path);

  arrput(
    state->wires, ((struct AvoidWire){
                    .netID = netID,
                    .edgeID = edgeID,
                    .srcID = srcID,
                    .dstID = dstID,
                    .pathID = pathID,
                  }));
}

// bool Intersect(
//   HMM_Vec2 p1, HMM_Vec2 p2, Box box) {
//   // d == line halfSize
//   HMM_Vec2 d = HMM_MulV2F(HMM_SubV2(p2, p1), 0.5f);
//   // e = box halfSize
//   HMM_Vec2 e = box.halfSize;

//   // c = (line halfSize + p1) - box center -- centroid?
//   HMM_Vec2 c = HMM_SubV2(HMM_AddV2(p1, d), box.center);
//   HMM_Vec2 ad = HMM_V2(fabs(d.X), fabs(d.Y));
//   if (fabsf(c.X) > e.X + ad.X)
//     return false;
//   if (fabsf(c.Y) > e.Y + ad.Y)
//     return false;
//   if (fabsf(d.Y * c[2] - d[2] * c.Y) > e.Y * ad[2] + e[2] * ad.Y + EPSILON)
//     return false;
//   if (fabsf(d[2] * c.X - d.X * c[2]) > e[2] * ad.X + e.X * ad[2] + EPSILON)
//     return false;
//   if (fabsf(d[0] * c[1] - d[1] * c[0]) > e[0] * ad[1] + e[1] * ad[0] +
//   EPSILON)
//     return false;
//   return true;
// }

void avoid_route(AvoidRouter *a) {
  struct AvoidState *state = a;

  for (size_t i = 0; i < arrlen(state->wires); i++) {
    struct AvoidWire wire = state->wires[i];
    RT_PathDef *path = &state->paths[wire.pathID];
    switch (wire_end_type(wire.srcID)) {
    case WIRE_END_INVALID:
      assert(0);
      break;
    case WIRE_END_NONE:
      break;
    case WIRE_END_PORT:
      path->start =
        state->anchors[state->ports[wire_end_index(wire.srcID)].anchorID];
      break;
    case WIRE_END_JUNC:
      path->start =
        state->anchors[state->junctions[wire_end_index(wire.srcID)].anchorID];
      break;
    }

    switch (wire_end_type(wire.dstID)) {
    case WIRE_END_INVALID:
      assert(0);
      break;
    case WIRE_END_NONE:
      break;
    case WIRE_END_PORT:
      path->end =
        state->anchors[state->ports[wire_end_index(wire.dstID)].anchorID];
      break;
    case WIRE_END_JUNC:
      path->end =
        state->anchors[state->junctions[wire_end_index(wire.dstID)].anchorID];
      break;
    }

    // printf(
    //   "Routing wire %d: %d %d -> %d %d\n", path->net_id, path->start.x,
    //   path->start.y, path->end.x, path->end.y);
  }

  // for (size_t i = 0; i < arrlen(state->anchors); i++) {
  //   printf("Anchor %zu: %d %d\n", i, state->anchors[i].x,
  //   state->anchors[i].y);
  // }

  double start = timer_now(&state->timer);

  RT_Result res = RT_graph_build(
    state->graph, state->anchors, arrlen(state->anchors), state->boundingBoxes,
    arrlen(state->boundingBoxes));
  if (res != RT_RESULT_SUCCESS) {
    printf("Error building graph: %d\n", res);
  }
  assert(res == RT_RESULT_SUCCESS);

  double graphBuild = timer_now(&state->timer);

  res = RT_graph_find_paths(
    state->graph, state->paths, arrlen(state->paths), state->vertexBuffers,
    state->vertBufferCapacity);
  assert(res == RT_RESULT_SUCCESS);

  double pathFind = timer_now(&state->timer);

  printf(
    "Graph build: %f, Path find: %f\n", graphBuild - start,
    pathFind - graphBuild);
  printf(
    "Number of anchors: %zu, boxes: %zu\n", arrlen(state->anchors),
    arrlen(state->boundingBoxes));
}

void avoid_dump_anchor_boxes(AvoidRouter *a) {
  struct AvoidState *state = a;

  FILE *fp = fopen("dump.rs", "w");
  fprintf(fp, "const ANCHOR_POINTS: &[Point] = &[\n");
  for (size_t i = 0; i < arrlen(state->anchors); i++) {
    fprintf(
      fp, "    Point { x: %d, y: %d },\n", state->anchors[i].x,
      state->anchors[i].y);
  }
  fprintf(fp, "];\n\n");
  fprintf(fp, "const BOUNDING_BOXES: &[BoundingBox] = &[\n");
  for (size_t i = 0; i < arrlen(state->boundingBoxes); i++) {
    fprintf(fp, "    BoundingBox {\n");
    fprintf(
      fp, "        center: Point { x: %d, y: %d },\n",
      state->boundingBoxes[i].center.x, state->boundingBoxes[i].center.y);
    fprintf(
      fp, "        half_width: %d,\n", state->boundingBoxes[i].half_width);
    fprintf(
      fp, "        half_height: %d,\n", state->boundingBoxes[i].half_height);
    fprintf(fp, "    },\n");
  }
  fprintf(fp, "];\n");
  fclose(fp);
}

size_t avoid_get_edge_path(
  AvoidRouter *a, WireID edgeID, float *coords, size_t maxLen) {
  struct AvoidState *state = a;
  int len = 0;
  int totalVerts = 0;
  for (size_t i = 0; i < arrlen(state->vertexBuffers); i++) {
    RT_VertexBuffer *buffer = &state->vertexBuffers[i];
    totalVerts += buffer->vertex_count;
    for (size_t j = 0; j < buffer->vertex_count; j++) {
      if (buffer->vertices[j].net_id == edgeID) {
        coords[len] = buffer->vertices[j].x;
        len++;
        coords[len] = buffer->vertices[j].y;
        len++;

        // printf("  %d: %f %f\n", edgeID, coords[len - 2], coords[len - 1]);
      }
    }
  }
  if (len == 0) {
    printf(
      "No path found for edge %d -- total verts: %d\n", edgeID, totalVerts);
  }
  return len;
}

void avoid_get_junction_pos(
  AvoidRouter *a, JunctionID junctionID, float *x, float *y) {
  struct AvoidState *state = a;
  (void)state;
}

typedef void *Context;
void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color);

static HMM_Vec2 panZoom(RT_Point position, float zoom, HMM_Vec2 pan) {
  return HMM_AddV2(HMM_MulV2F(HMM_V2(position.x, position.y), zoom), pan);
}

void avoid_draw_debug_lines(
  AvoidRouter *a, void *ctx, float zoom, HMM_Vec2 pan) {
  struct AvoidState *state = a;
  const RT_Node *nodes;
  size_t nodeCount;

  RT_Result res = RT_graph_get_nodes(state->graph, &nodes, &nodeCount);
  assert(res == RT_RESULT_SUCCESS);

  for (size_t i = 0; i < nodeCount; i++) {
    const RT_Node *node = &nodes[i];
    RT_Point p1 = node->point;
    for (size_t j = 0; j < 4; j++) {
      if (node->neighbors[j] < nodeCount) {
        RT_Point p2 = nodes[node->neighbors[j]].point;
        draw_stroked_line(
          ctx, panZoom(p1, zoom, pan), panZoom(p2, zoom, pan), 1,
          HMM_V4(0.5f, 0.5f, 0.7f, 0.5f));
      }
    }
  }
}