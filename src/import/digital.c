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

#define LOG_LEVEL LL_DEBUG
#include "log.h"

#define DEBUG_PRINT(...)
#include "lxml.h"
#define STBDS_ASSERT assert
#include "stb_ds.h"
#include <assert.h>

static LXMLNode *find_node(LXMLNode *node, const char *tag) {
  for (int i = 0; i < node->children.size; i++) {
    LXMLNode *child = node->children.data[i];
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
  {"Or", COMP_OR},    {"XOr", COMP_XOR},    {"Not", COMP_NOT},
};

typedef struct IVec2 {
  int x, y;
} IVec2;

typedef struct WireEnd {
  IVec2 pos;
  bool visited;
  enum { IN_PORT, OUT_PORT, WIRE, WAYPOINT } type;
  union {
    PortID port;
    IVec2 farSide;
  };
} WireEnd;

typedef struct DigWire {
  WireEnd ends[2];
  bool valid;
  bool visited;
} DigWire;

typedef struct {
  IVec2 key;
  arr(uint32_t) value;
} DigWireHash;

void replace_wire_end_with_port(
  arr(DigWire) digWires, DigWireHash *digWireEnds, PortID port, IVec2 pos,
  bool in) {
  arr(uint32_t) ends = hmget(digWireEnds, pos);
  assert(arrlen(ends) == 1); // waypoint on pin not supported yet
  for (int i = 0; i < arrlen(ends); i++) {
    DigWire *digWire = &digWires[ends[i]];
    for (int j = 0; j < 2; j++) {
      if (digWire->ends[j].pos.x == pos.x && digWire->ends[j].pos.y == pos.y) {
        assert(digWire->ends[j].type == WIRE);
        digWire->ends[j].type = in ? IN_PORT : OUT_PORT;
        digWire->ends[j].port = port;
      }
    }
  }
}

void remove_from_hash(DigWireHash *digWireEnds, IVec2 pos, uint32_t index) {
  arr(uint32_t) ends = hmget(digWireEnds, pos);
  for (int i = 0; i < arrlen(ends); i++) {
    if (ends[i] == index) {
      arrdel(ends, i);
      break;
    }
  }

  hmput(digWireEnds, pos, ends);
}

static void simplify_wires(arr(DigWire) digWires, DigWireHash *digWireEnds) {
  // simplify wires by merging neighbouring wires
  bool changed = true;
  while (changed) {
    changed = false;
    log_debug("Merge round");
    for (int i = 0; i < arrlen(digWires); i++) {
      DigWire *digWire = &digWires[i];
      if (!digWire->valid) {
        continue;
      }
      for (int j = 0; j < 2; j++) {
        WireEnd *end = &digWire->ends[j];
        if (end->type != WIRE) {
          continue;
        }

        arr(uint32_t) ends = hmget(digWireEnds, end->pos);

        if (arrlen(ends) != 2) {
          continue;
        }

        for (int k = 0; k < 2; k++) {
          uint32_t otherIndex = ends[k];
          if (otherIndex == i) {
            continue;
          }

          DigWire *other = &digWires[otherIndex];
          for (int l = 0; l < 2; l++) {
            WireEnd *otherEnd = &other->ends[l];
            if (otherEnd->type != WIRE) {
              continue;
            }
            WireEnd *otherFarEnd = &other->ends[(l + 1) % 2];
            if (
              otherEnd->pos.x == end->pos.x && otherEnd->pos.y == end->pos.y) {
              // merge the two wires keeping the far ends of each
              log_debug(
                "  Merged %d and %d at %d, %d\n", i, otherIndex, end->pos.x,
                end->pos.y);

              // remove both ends of other from the hash
              remove_from_hash(digWireEnds, otherEnd->pos, otherIndex);
              remove_from_hash(digWireEnds, otherFarEnd->pos, otherIndex);

              // remove the replaced end from the hash
              remove_from_hash(digWireEnds, end->pos, i);

              *end = *otherFarEnd;

              arr(uint32_t) ends = hmget(digWireEnds, end->pos);
              arrput(ends, i);
              hmput(digWireEnds, end->pos, ends);

              other->valid = false;

              changed = true;
              break;
            }
          }
        }
      }
    }
  }
}

void import_digital(CircuitUX *ux, char *buffer) {
  arr(uint32_t) stack = 0;
  arr(PortID) inPorts = 0;
  arr(PortID) outPorts = 0;
  arr(HMM_Vec2) waypoints = 0;
  arr(uint32_t) netWires = 0;

  arr(DigWire) digWires = 0;

  DigWireHash *digWireEnds = 0;

  hmdefault(digWireEnds, 0);

  LXMLDocument doc;
  if (!LXMLDocument_load_memory(&doc, buffer)) {
    log_error("Failed to load XML");
    return;
  }

  LXMLNode *root = doc.root;
  if (!root) {
    log_debug("No root node");
    goto fail;
  }

  LXMLNode *circuit = find_node(root, "circuit");
  if (!circuit) {
    log_debug("No circuit node");
    goto fail;
  }

  LXMLNode *wires = find_node(circuit, "wires");
  if (!wires) {
    log_debug("No wires node");
    goto fail;
  }

  log_debug("Loading wires");

  // load all the wires into digWires
  for (int i = 0; i < wires->children.size; i++) {
    LXMLNode *wire = wires->children.data[i];
    if (strcmp(wire->tag, "wire") == 0) {
      LXMLNode *p1 = find_node(wire, "p1");
      if (!p1) {
        log_debug("No p1 node");
        goto fail;
      }

      LXMLNode *p2 = find_node(wire, "p2");
      if (!p2) {
        log_debug("No p2 node");
        goto fail;
      }

      LXMLNode *nodes[2] = {p1, p2};
      IVec2 positions[2] = {0};

      for (int j = 0; j < 2; j++) {
        LXMLNode *node = nodes[j];
        for (int k = 0; k < node->attributes.size; k++) {
          LXMLAttribute *attr = &node->attributes.data[k];
          if (strcmp(attr->key, "x") == 0) {
            positions[j].x = atoi(attr->value);
          } else if (strcmp(attr->key, "y") == 0) {
            positions[j].y = atoi(attr->value);
          } else {
            log_debug("Unknown attribute %s\n", attr->key);
            goto fail;
          }
        }
      }

      DigWire digWire = {
        .valid = true,
        .ends =
          {
            {
              .pos = positions[0],
              .type = WIRE,
            },
            {
              .pos = positions[1],
              .type = WIRE,
            },
          },
      };

      arrput(digWires, digWire);

      for (int j = 0; j < 2; j++) {
        IVec2 key = digWire.ends[j].pos;
        arr(uint32_t) ends = hmget(digWireEnds, key);

        arrput(ends, arrlen(digWires) - 1);
        hmput(digWireEnds, key, ends);
      }
    }
  }

  log_debug("Loading components");

  LXMLNode *visualElements = find_node(circuit, "visualElements");
  if (!visualElements) {
    log_debug("No visualElements node");
    goto fail;
  }

  for (int i = 0; i < visualElements->children.size; i++) {
    LXMLNode *visualElement = visualElements->children.data[i];
    if (strcmp(visualElement->tag, "visualElement") == 0) {
      LXMLNode *typeNameNode = find_node(visualElement, "elementName");
      if (!typeNameNode) {
        log_debug("No elementName node");
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
        log_debug("Unknown component type %s\n", typeName);
        goto fail;
      }

      LXMLNode *positionNode = find_node(visualElement, "pos");
      if (!positionNode) {
        log_debug("No pos node");
        goto fail;
      }

      int x, y;
      for (int j = 0; j < positionNode->attributes.size; j++) {
        LXMLAttribute *attr = &positionNode->attributes.data[j];
        if (strcmp(attr->key, "x") == 0) {
          x = atoi(attr->value);
        } else if (strcmp(attr->key, "y") == 0) {
          y = atoi(attr->value);
        } else {
          goto fail;
        }
      }

      log_debug("Adding component %s at %d, %d\n", typeName, x, y);
      ComponentID componentID = ux_add_component(ux, descID, HMM_V2(x, y));

      // digital's components are placed relative to the first port
      Component *component =
        circuit_component_ptr(&ux->view.circuit, componentID);
      ComponentView *compView = view_component_ptr(&ux->view, componentID);

      PortID firstPort = component->portFirst;
      PortView *portView = view_port_ptr(&ux->view, firstPort);
      ux_move_component(
        ux, componentID, HMM_SubV2(HMM_V2(0, 0), portView->center));

      HMM_Vec2 portPos = HMM_AddV2(compView->box.center, portView->center);
      log_debug("Moved: %f == %d, %f == %d\n", portPos.X, x, portPos.Y, y);

      const ComponentDesc *desc = &ux->view.circuit.componentDescs[descID];
      switch (descID) {
      case COMP_INPUT:
      case COMP_OUTPUT: {
        PortID portID = firstPort;
        log_debug("Adding port %s at %d, %d\n", desc->ports[0].name, x, y);
        replace_wire_end_with_port(
          digWires, digWireEnds, portID, (IVec2){x, y}, descID == COMP_OUTPUT);
        break;
      }
      case COMP_AND:
      case COMP_OR:
      case COMP_XOR:
      case COMP_NOT: {
        IVec2 nextInput = {x, y};
        IVec2 nextOutput = {x + 4 * 20, y + 20};
        if (descID == COMP_NOT) {
          nextOutput = (IVec2){x + 2 * 20, y};
        }
        PortID portID = component->portFirst;
        int j = 0;
        while (portID != NO_PORT) {
          IVec2 pos = nextInput;
          if (desc->ports[j].direction == PORT_OUT) {
            pos = nextOutput;
            nextOutput.y += 20;
          } else {
            nextInput.y += 40;
          }
          log_debug(
            "Adding port %s at %d, %d\n", desc->ports[j].name, pos.x, pos.y);
          replace_wire_end_with_port(
            digWires, digWireEnds, portID, pos,
            desc->ports[j].direction == PORT_IN);

          portID = circuit_port_ptr(&ux->view.circuit, portID)->compNext;
          j++;
        }
        break;
      }
      default:
        log_debug("Unknown component type %d\n", descID);
        assert(0);
        break;
      }
    }
  }

  // debug code: double check the wire ends are all valid
  bool allValid = true;
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    for (int j = 0; j < arrlen(digWireEnds[i].value); j++) {
      DigWire *digWire = &digWires[digWireEnds[i].value[j]];
      if (!digWire->valid) {
        log_debug(
          "Invalid wire at %d, %d\n", digWireEnds[i].key.x,
          digWireEnds[i].key.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // check all valid wire ends are in the hash
  for (int i = 0; i < arrlen(digWires); i++) {
    DigWire *digWire = &digWires[i];
    if (!digWire->valid) {
      continue;
    }
    for (int j = 0; j < 2; j++) {
      WireEnd *end = &digWire->ends[j];
      arr(uint32_t) ends = hmget(digWireEnds, end->pos);
      bool found = false;
      for (int k = 0; k < arrlen(ends); k++) {
        if (ends[k] == i) {
          found = true;
          break;
        }
      }
      if (!found) {
        log_debug("Wire end %d, %d not in hash\n", end->pos.x, end->pos.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  simplify_wires(digWires, digWireEnds);

  // debug code: double check the wire ends are all valid
  allValid = true;
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    for (int j = 0; j < arrlen(digWireEnds[i].value); j++) {
      DigWire *digWire = &digWires[digWireEnds[i].value[j]];
      if (!digWire->valid) {
        log_debug(
          "Invalid wire at %d, %d\n", digWireEnds[i].key.x,
          digWireEnds[i].key.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // check all valid wire ends are in the hash
  for (int i = 0; i < arrlen(digWires); i++) {
    DigWire *digWire = &digWires[i];
    if (!digWire->valid) {
      continue;
    }
    for (int j = 0; j < 2; j++) {
      WireEnd *end = &digWire->ends[j];
      arr(uint32_t) ends = hmget(digWireEnds, end->pos);
      bool found = false;
      for (int k = 0; k < arrlen(ends); k++) {
        if (ends[k] == i) {
          found = true;
          break;
        }
      }
      if (!found) {
        log_debug("Wire end %d, %d not in hash\n", end->pos.x, end->pos.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // trim free floating wire ends
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    if (arrlen(digWireEnds[i].value) == 1) {
      DigWire *digWire = &digWires[digWireEnds[i].value[0]];
      if (!digWire->valid) {
        continue;
      }
      for (int j = 0; j < 2; j++) {
        if (
          digWire->ends[j].pos.x == digWireEnds[i].key.x &&
          digWire->ends[j].pos.y == digWireEnds[i].key.y &&
          digWire->ends[j].type == WIRE) {
          log_debug(
            "Trimming free floating wire at %d, %d\n", digWire->ends[j].pos.x,
            digWire->ends[j].pos.y);
          digWire->valid = false;
          remove_from_hash(
            digWireEnds, digWireEnds[i].key, digWireEnds[i].value[0]);
          uint32_t otherEnd = (j + 1) % 2;
          remove_from_hash(
            digWireEnds, digWire->ends[otherEnd].pos, digWireEnds[i].value[0]);
        }
      }
    }
  }

  // see if more simplifications are possible
  simplify_wires(digWires, digWireEnds);

  // debug code: double check the wire ends are all valid
  allValid = true;
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    for (int j = 0; j < arrlen(digWireEnds[i].value); j++) {
      DigWire *digWire = &digWires[digWireEnds[i].value[j]];
      if (!digWire->valid) {
        log_debug(
          "Invalid wire at %d, %d\n", digWireEnds[i].key.x,
          digWireEnds[i].key.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // check all valid wire ends are in the hash
  for (int i = 0; i < arrlen(digWires); i++) {
    DigWire *digWire = &digWires[i];
    if (!digWire->valid) {
      continue;
    }
    for (int j = 0; j < 2; j++) {
      WireEnd *end = &digWire->ends[j];
      arr(uint32_t) ends = hmget(digWireEnds, end->pos);
      bool found = false;
      for (int k = 0; k < arrlen(ends); k++) {
        if (ends[k] == i) {
          found = true;
          break;
        }
      }
      if (!found) {
        log_debug("Wire end %d, %d not in hash\n", end->pos.x, end->pos.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // figure out where the waypoints are
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    if (arrlen(digWireEnds[i].value) > 2) {
      log_debug(
        "Waypoint at %d, %d\n", digWireEnds[i].key.x, digWireEnds[i].key.y);

      // WaypointID waypointID =
      //   ux_add_waypoint(ux, HMM_V2(digWireEnds[i].key.x,
      //   digWireEnds[i].key.y));
      bool valid = true;
      for (int j = 0; j < arrlen(digWireEnds[i].value); j++) {
        DigWire *digWire = &digWires[digWireEnds[i].value[j]];
        if (!digWire->valid) {
          valid = false;
          break;
        }
      }
      if (!valid) {
        continue;
      }

      for (int j = 0; j < arrlen(digWireEnds[i].value); j++) {
        DigWire *digWire = &digWires[digWireEnds[i].value[j]];
        for (int k = 0; k < 2; k++) {
          if (
            digWire->ends[k].pos.x == digWireEnds[i].key.x &&
            digWire->ends[k].pos.y == digWireEnds[i].key.y) {
            digWire->ends[k].type = WAYPOINT;
          }
        }
      }
    }
  }

  // recursively follow wires and build nets from them
  for (int i = 0; i < arrlen(digWires); i++) {
    DigWire *digWire = &digWires[i];
    if (!digWire->valid || digWire->visited) {
      continue;
    }

    arrput(stack, i);

    while (arrlen(stack) > 0) {
      int j = arrpop(stack);
      DigWire *digWire = &digWires[j];

      if (!digWire->valid || digWire->visited) {
        continue;
      }
      digWire->visited = true;
      arrput(netWires, j);

      for (int k = 0; k < 2; k++) {
        WireEnd *end = &digWire->ends[k];

        arr(uint32_t) ends = hmget(digWireEnds, end->pos);
        for (int l = 0; l < arrlen(ends); l++) {
          arrput(stack, ends[l]);
        }

        switch (end->type) {
        case IN_PORT:
          arrput(inPorts, end->port);
          break;
        case OUT_PORT:
          arrput(outPorts, end->port);
          break;
        case WIRE:
          break;
        case WAYPOINT: {
          bool found = false;
          for (int l = 0; l < arrlen(waypoints); l++) {
            if (waypoints[l].X == end->pos.x && waypoints[l].Y == end->pos.y) {
              found = true;
              break;
            }
          }
          if (!found) {
            arrput(waypoints, HMM_V2(end->pos.x, end->pos.y));
          }
          break;
        }
        }
      }
    }

    NetID netID = ux_add_net(ux);
    log_debug("Net %d", netID);

    for (int j = 0; j < arrlen(inPorts); j++) {
      log_debug("  * In port %d", inPorts[j]);
      ux_add_endpoint(ux, netID, inPorts[j], HMM_V2(0, 0));
    }
    for (int j = 0; j < arrlen(waypoints); j++) {
      log_debug("  * Waypoint %f %f", waypoints[j].X, waypoints[j].Y);
      ux_add_waypoint(ux, netID, waypoints[j]);
    }
    for (int j = 0; j < arrlen(outPorts); j++) {
      log_debug("  * Out port %d", outPorts[j]);
      ux_add_endpoint(ux, netID, outPorts[j], HMM_V2(0, 0));
    }

    arrsetlen(inPorts, 0);
    arrsetlen(outPorts, 0);
    arrsetlen(waypoints, 0);
    arrsetlen(netWires, 0);
  }

fail:
  arrfree(stack);
  arrfree(inPorts);
  arrfree(outPorts);
  arrfree(netWires);
  arrfree(waypoints);
  arrfree(digWires);
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    arrfree(digWireEnds[i].value);
  }
  hmfree(digWireEnds);
  LXMLDocument_free(&doc);
}