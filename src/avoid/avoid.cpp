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

#include "core/core.h"
#include "libavoid/connector.h"
#include "libavoid/junction.h"
#include "libavoid/libavoid.h"
#include "libavoid/router.h"
#include "libavoid/shape.h"
#include <cstdint>

#include "avoid/avoid.h"

struct AvoidPort {
  Avoid::ShapeRef *node;
  Avoid::ShapeConnectionPin *port;
};

struct AvoidState {
  Avoid::Router router;
  std::vector<AvoidPort> ports;
  std::vector<Avoid::ShapeRef *> nodeShapes;
  std::vector<Avoid::ConnRef *> edges;
  std::vector<Avoid::JunctionRef *> junctions;

  AvoidState() : router(Avoid::OrthogonalRouting) {}
};

AvoidRouter *avoid_new() {
  AvoidState *state = new AvoidState();
  state->router.setRoutingOption(Avoid::improveHyperedgeRoutesMovingJunctions, true);
  state->router.setRoutingPenalty(Avoid::shapeBufferDistance, 15.0);
  state->router.setRoutingPenalty(Avoid::crossingPenalty, 200.0);
  return state;
}

void avoid_free(AvoidRouter *a) { delete reinterpret_cast<AvoidState *>(a); }

void avoid_add_node(
  AvoidRouter *a, ComponentID nodeID, float x, float y, float w, float h) {
  auto state = reinterpret_cast<AvoidState *>(a);

  Avoid::Rectangle rectangle(Avoid::Point(x, y), Avoid::Point(x + w, y + h));
  Avoid::ShapeRef *node =
    new Avoid::ShapeRef(&state->router, rectangle, nodeID + 1);

  while (state->nodeShapes.size() <= nodeID) {
    state->nodeShapes.push_back(nullptr);
  }

  state->nodeShapes.insert(state->nodeShapes.begin() + nodeID, node);
}

void avoid_move_node(AvoidRouter *a, ComponentID nodeID, float x, float y) {
  auto state = reinterpret_cast<AvoidState *>(a);

  Avoid::ShapeRef *node = state->nodeShapes[nodeID];
  Avoid::Point p(x, y);

  auto poly = node->polygon();
  poly.translate(x, y);
  state->router.moveShape(node, poly);
}

void avoid_add_port(
  AvoidRouter *a, PortID portID, ComponentID nodeID, PortSide side,
  float centerX, float centerY) {
  auto state = reinterpret_cast<AvoidState *>(a);

  Avoid::ShapeRef *nodeShape = state->nodeShapes[nodeID];
  Avoid::Box bb = nodeShape->polygon().offsetBoundingBox(0);
  Avoid::ShapeConnectionPin *port;

  double w = bb.width();
  double h = bb.height();
  double rx = (double)centerX / w;
  double ry = (double)centerY / h;

  switch (side) {
  case SIDE_TOP:
    port = new Avoid::ShapeConnectionPin(
      nodeShape, portID + 1, rx, Avoid::ATTACH_POS_TOP, 0, Avoid::ConnDirUp);
    break;
  case SIDE_BOTTOM:
    port = new Avoid::ShapeConnectionPin(
      nodeShape, portID + 1, rx, Avoid::ATTACH_POS_BOTTOM, 0,
      Avoid::ConnDirDown);
    break;
  case SIDE_LEFT:
    port = new Avoid::ShapeConnectionPin(
      nodeShape, portID + 1, Avoid::ATTACH_POS_LEFT, ry, 0, Avoid::ConnDirLeft);
    break;
  case SIDE_RIGHT:
    port = new Avoid::ShapeConnectionPin(
      nodeShape, portID + 1, Avoid::ATTACH_POS_RIGHT, ry, 0,
      Avoid::ConnDirRight);
    break;
  }

  port->setExclusive(true);
  state->ports.push_back({nodeShape, port});
}

void
avoid_add_junction(AvoidRouter *a, JunctionID junctionID, float x, float y) {
  auto state = reinterpret_cast<AvoidState *>(a);

  Avoid::JunctionRef *junction =
    new Avoid::JunctionRef(&state->router, Avoid::Point(x, y), junctionID + 1);
  junction->setPositionFixed(false);
  state->junctions.push_back(junction);
}

void avoid_add_edge(
  AvoidRouter *a, WireID edgeID, WireEndID srcID, WireEndID dstID, float x1, float y1, float x2, float y2) {
  auto state = reinterpret_cast<AvoidState *>(a);

  Avoid::ConnEnd ends[2];
  WireEndID endIDs[2] = {srcID, dstID};
  Avoid::Point points[2] = {{x1, y1}, {x2, y2}};

  for (int i = 0; i < 2; i++) {
    switch(wire_end_type(endIDs[i])) {
    case WIRE_END_INVALID:
      assert(0);
      break;
    case WIRE_END_NONE:
      ends[i] = Avoid::ConnEnd(points[i]);
      break;
    case WIRE_END_PORT:{
      AvoidPort port = state->ports[wire_end_index(endIDs[i])];
      ends[i] = Avoid::ConnEnd(port.node, wire_end_index(endIDs[i])+1);
      break;
    }
    case WIRE_END_JUNC:
      ends[i] = Avoid::ConnEnd(state->junctions[wire_end_index(endIDs[i])]);
      break;
    }
  }

  printf("connecting to port %d to %d\n", ends[0].pinClassId(), ends[1].pinClassId());

  Avoid::ConnRef *edge =
    new Avoid::ConnRef(&state->router, ends[0], ends[1], edgeID + 1);
  edge->setRoutingType(Avoid::ConnType_Orthogonal);

  state->edges.push_back(edge);
}

void avoid_route(AvoidRouter *a) {
  auto state = reinterpret_cast<AvoidState *>(a);
  state->router.processTransaction();
  for (auto junction : state->junctions) {
    state->router.moveJunction(junction, junction->recommendedPosition());
  }
}

size_t avoid_get_edge_path(
  AvoidRouter *a, NetID edgeID, float *coords, size_t maxLen) {
  auto state = reinterpret_cast<AvoidState *>(a);
  auto edge = state->edges[edgeID];

  const Avoid::PolyLine route = edge->displayRoute();

  size_t i;
  for (i = 0; i < route.ps.size(); i++) {
    if (i * 2 >= maxLen) {
      break;
    }
    coords[i * 2] = route.ps[i].x;
    coords[i * 2 + 1] = route.ps[i].y;
  }

  return i * 2;
}

void avoid_get_junction_pos(
  AvoidRouter *a, JunctionID junctionID, float *x, float *y) {
  auto state = reinterpret_cast<AvoidState *>(a);
  auto junction = state->junctions[junctionID];
  Avoid::Point pos = junction->position();
  *x = pos.x;
  *y = pos.y;
}