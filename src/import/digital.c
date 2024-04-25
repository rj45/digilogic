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
#include "handmade_math.h"
#include "import.h"
#include "ux/ux.h"
#include "view/view.h"

#define DEBUG_PRINT(...)
#include "lxml.h"
#include "stb_ds.h"

static XMLNode *find_node(XMLNode *node, const char *tag) {
  for (int i = 0; i < node->children.size; i++) {
    XMLNode *child = node->children.data[i];
    if (strcmp(child->tag, tag) == 0) {
      return child;
    }
  }
  return NULL;
}

struct {
  const char *name;
  ComponentDescID desc;
} componentTypes[] = {
  {"In", COMP_INPUT}, {"Out", COMP_OUTPUT}, {"And", COMP_AND},
  {"Or", COMP_OR},    {"Not", COMP_NOT},
};

typedef struct IVec2 {
  int x, y;
} IVec2;

typedef struct WireEnd {
  IVec2 pos;
  bool visited;
  enum { IN_PORT, OUT_PORT, WIRE } type;
  union {
    PortID port;
    IVec2 farSide;
  };
} WireEnd;

static int compare(const void *va, const void *vb) {
  WireEnd *a = (WireEnd *)va;
  WireEnd *b = (WireEnd *)vb;
  if (a->pos.x < b->pos.x) {
    return -1;
  } else if (a->pos.x > b->pos.x) {
    return 1;
  } else if (a->pos.y < b->pos.y) {
    return -1;
  } else if (a->pos.y > b->pos.y) {
    return 1;
  }
  return 0;
}

void import_digital(CircuitUX *ux, const char *filename) {
  arr(WireEnd) wireEnds = 0;
  arr(uint32_t) stack = 0;
  arr(PortID) inPorts = 0;
  arr(PortID) outPorts = 0;
  arr(uint32_t) netEnds = 0;

  XMLDocument doc;
  if (!XMLDocument_load(&doc, filename)) {
    printf("Failed to load %s\n", filename);
    return;
  }

  XMLNode *root = doc.root;
  if (!root) {
    printf("No root node\n");
    goto fail;
  }

  XMLNode *circuit = find_node(root, "circuit");
  if (!circuit) {
    printf("No circuit node\n");
    goto fail;
  }

  XMLNode *visualElements = find_node(circuit, "visualElements");
  if (!visualElements) {
    printf("No visualElements node\n");
    goto fail;
  }

  for (int i = 0; i < visualElements->children.size; i++) {
    XMLNode *visualElement = visualElements->children.data[i];
    if (strcmp(visualElement->tag, "visualElement") == 0) {
      XMLNode *typeNameNode = find_node(visualElement, "elementName");
      if (!typeNameNode) {
        printf("No elementName node\n");
        goto fail;
      }

      char *typeName = typeNameNode->inner_text;
      ComponentDescID descID = (ComponentDescID)-1;
      for (int j = 0; j < sizeof(componentTypes) / sizeof(componentTypes[0]);
           j++) {
        if (strcmp(typeName, componentTypes[j].name) == 0) {
          descID = componentTypes[j].desc;
        }
      }
      if (descID == (ComponentDescID)-1) {
        printf("Unknown component type %s\n", typeName);
        goto fail;
      }

      XMLNode *positionNode = find_node(visualElement, "pos");
      if (!positionNode) {
        printf("No pos node\n");
        goto fail;
      }

      int x, y;
      for (int j = 0; j < positionNode->attributes.size; j++) {
        XMLAttribute *attr = &positionNode->attributes.data[j];
        if (strcmp(attr->key, "x") == 0) {
          x = atoi(attr->value);
        } else if (strcmp(attr->key, "y") == 0) {
          y = atoi(attr->value);
        } else {
          goto fail;
        }
      }

      printf("Adding component %s at %d, %d\n", typeName, x, y);
      ComponentID componentID = ux_add_component(ux, descID, HMM_V2(x, y));

      // digital's components are placed relative to the first port
      PortID firstPort = view_port_start(&ux->view, componentID);
      ux_move_component(
        ux, componentID,
        HMM_SubV2(HMM_V2(0, 0), ux->view.ports[firstPort].center));

      HMM_Vec2 portPos = HMM_AddV2(
        ux->view.components[componentID].box.center,
        ux->view.ports[firstPort].center);
      printf("Moved: %f == %d, %f == %d\n", portPos.X, x, portPos.Y, y);

      const ComponentDesc *desc = &ux->view.circuit.componentDescs[descID];
      switch (descID) {
      case COMP_INPUT:
      case COMP_OUTPUT: {
        PortID portID = view_port_start(&ux->view, componentID);
        WireEnd end = {
          .pos = {x, y},
          .type = desc->ports[0].direction == PORT_OUT ? OUT_PORT : IN_PORT,
          .port = portID,
        };
        printf(
          "Adding port %s at %d, %d\n", desc->ports[0].name, end.pos.x,
          end.pos.y);
        arrput(wireEnds, end);
        break;
      }
      case COMP_AND:
      case COMP_OR:
      case COMP_NOT: {
        IVec2 nextInput = {x, y};
        IVec2 nextOutput = {x + 4 * 20, y + 20};
        if (descID == COMP_NOT) {
          nextOutput = (IVec2){x + 2 * 20, y};
        }
        for (int j = 0; j < desc->numPorts; j++) {
          PortID portID = view_port_start(&ux->view, componentID) + j;
          WireEnd end = {
            .pos = nextInput,
            .type = IN_PORT,
            .port = portID,
          };
          if (desc->ports[j].direction == PORT_OUT) {
            end.pos = nextOutput;
            end.type = OUT_PORT;
            nextOutput.y += 20;
          } else {
            nextInput.y += 40;
          }
          printf(
            "Adding port %s at %d, %d\n", desc->ports[j].name, end.pos.x,
            end.pos.y);
          arrput(wireEnds, end);
        }
        break;
      }
      }
    }
  }

  XMLNode *wires = find_node(circuit, "wires");
  if (!wires) {
    printf("No wires node\n");
    goto fail;
  }

  for (int i = 0; i < wires->children.size; i++) {
    XMLNode *wire = wires->children.data[i];
    if (strcmp(wire->tag, "wire") == 0) {
      XMLNode *p1 = find_node(wire, "p1");
      if (!p1) {
        printf("No p1 node\n");
        goto fail;
      }

      XMLNode *p2 = find_node(wire, "p2");
      if (!p2) {
        printf("No p2 node\n");
        goto fail;
      }

      XMLNode *nodes[2] = {p1, p2};
      IVec2 positions[2] = {0};

      for (int j = 0; j < 2; j++) {
        XMLNode *node = nodes[j];
        for (int k = 0; k < node->attributes.size; k++) {
          XMLAttribute *attr = &node->attributes.data[k];
          if (strcmp(attr->key, "x") == 0) {
            positions[j].x = atoi(attr->value);
          } else if (strcmp(attr->key, "y") == 0) {
            positions[j].y = atoi(attr->value);
          } else {
            printf("Unknown attribute %s\n", attr->key);
            goto fail;
          }
        }
      }

      WireEnd start = {
        .pos = positions[0],
        .type = WIRE,
        .farSide = positions[1],
      };
      WireEnd end = {
        .pos = positions[1],
        .type = WIRE,
        .farSide = positions[0],
      };

      arrput(wireEnds, start);
      arrput(wireEnds, end);
    }
  }

  // sort first by X, then by Y
  qsort(wireEnds, arrlen(wireEnds), sizeof(WireEnd), compare);

  for (int i = 0; i < arrlen(wireEnds); i++) {
    WireEnd *end = &wireEnds[i];
    printf(
      "WireEnd %d: %s %d, %d\n", i,
      end->type == IN_PORT ? "IN  " : (end->type == OUT_PORT ? "OUT " : "WIRE"),
      end->pos.x, end->pos.y);
  }

  // recursively follow wires and build nets from them
  for (int i = 0; i < arrlen(wireEnds); i++) {
    WireEnd *firstEnd = &wireEnds[i];
    if (firstEnd->visited) {
      continue;
    }

    arrput(stack, i);

    while (arrlen(stack) > 0) {
      int j = arrpop(stack);
      WireEnd *end = &wireEnds[j];

      if (end->visited) {
        continue;
      }
      end->visited = true;

      arrput(netEnds, j);

      for (int k = j + 1; k < arrlen(wireEnds); k++) {
        WireEnd *other = &wireEnds[k];
        if (other->pos.x == end->pos.x && other->pos.y == end->pos.y) {
          arrput(stack, k);
        } else {
          break;
        }
      }

      switch (end->type) {
      case IN_PORT:
        arrput(inPorts, end->port);
        break;
      case OUT_PORT:
        arrput(outPorts, end->port);
        break;
      case WIRE:
        for (int k = 0; k < arrlen(wireEnds); k++) {
          WireEnd *other = &wireEnds[k];
          if (
            other->pos.x == end->farSide.x && other->pos.y == end->farSide.y) {
            arrput(stack, k);
            break;
          }
        }
        break;
      }
    }

    NetID netID = ux_add_net(ux);

    if (arrlen(inPorts) == 1 && arrlen(outPorts) == 1) {
      // simple case
      printf("Adding wire from %d to %d\n", inPorts[0], outPorts[0]);
      ux_add_wire(
        ux, netID, wire_end_make(WIRE_END_PORT, inPorts[0]),
        wire_end_make(WIRE_END_PORT, outPorts[0]));
    } else {

      float junctionX = 0;
      float junctionY = 0;

      // find junctions
      for (int j = 0; j < arrlen(netEnds); j++) {
        WireEnd *end = &wireEnds[netEnds[j]];
        if (end->type != WIRE) {
          continue;
        }

        int count = 0;

        for (int k = 0; k < arrlen(netEnds); k++) {
          WireEnd *other = &wireEnds[netEnds[k]];
          if (other->type != WIRE) {
            continue;
          }

          if (end->pos.x == other->pos.x && end->pos.y == other->pos.y) {
            count++;
          }
        }

        if (count > 2) {
          printf(
            "Juction location? %d : %d %d\n", count, end->pos.x, end->pos.y);
          junctionX = end->pos.x;
          junctionY = end->pos.y;
          for (int k = 0; k < arrlen(netEnds); k++) {
            WireEnd *other = &wireEnds[netEnds[k]];
            if (other->type != WIRE) {
              continue;
            }
            if (end->pos.x != other->pos.x || end->pos.y != other->pos.y) {
              continue;
            }
            printf(
              "   WireEnd %d: %s %d, %d --> %d, %d\n", i,
              other->type == IN_PORT
                ? "IN  "
                : (other->type == OUT_PORT ? "OUT " : "WIRE"),
              other->pos.x, other->pos.y, other->farSide.x, other->farSide.y);
          }
        }
      }

      // add junction
      JunctionID junctionID = ux_add_junction(ux, HMM_V2(junctionX, junctionY));

      for (int i = 0; i < arrlen(outPorts); i++) {
        printf("Adding net from %d to j%d\n", outPorts[i], junctionID);
        ux_add_wire(
          ux, netID, wire_end_make(WIRE_END_PORT, outPorts[i]),
          wire_end_make(WIRE_END_JUNC, junctionID));
      }
      for (int j = 0; j < arrlen(inPorts); j++) {
        printf("Adding net from j%d to %d\n", junctionID, inPorts[j]);
        ux_add_wire(
          ux, netID, wire_end_make(WIRE_END_JUNC, junctionID),
          wire_end_make(WIRE_END_PORT, inPorts[j]));
      }
    }
    arrsetlen(inPorts, 0);
    arrsetlen(outPorts, 0);
    arrsetlen(netEnds, 0);
  }

fail:
  arrfree(stack);
  arrfree(inPorts);
  arrfree(outPorts);
  arrfree(wireEnds);
  arrfree(netEnds);
  XMLDocument_free(&doc);
}