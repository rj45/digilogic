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

#define LOG_LEVEL LL_INFO
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
  const char *digName;
  const char *ourName;
} componentTypes[] = {
  {"In", "IN"}, {"Out", "OUT"}, {"And", "AND"},
  {"Or", "OR"}, {"XOr", "XOR"}, {"Not", "NOT"},
};

typedef struct IVec2 {
  int x, y;
} IVec2;

typedef struct WireEnd {
  IVec2 pos;
  bool visited;
  enum { IN_PORT, OUT_PORT, WIRE, WAYPOINT } type;
  union {
    PortRef portRef;
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
  arr(DigWire) digWires, DigWireHash *digWireEnds, PortRef portRef, IVec2 pos,
  bool in) {
  arr(uint32_t) ends = hmget(digWireEnds, pos);
  assert(arrlen(ends) == 1); // waypoint on pin not supported yet
  for (int i = 0; i < arrlen(ends); i++) {
    DigWire *digWire = &digWires[ends[i]];
    for (int j = 0; j < 2; j++) {
      if (digWire->ends[j].pos.x == pos.x && digWire->ends[j].pos.y == pos.y) {
        assert(digWire->ends[j].type == WIRE);
        digWire->ends[j].type = in ? IN_PORT : OUT_PORT;
        digWire->ends[j].portRef = portRef;
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
                "  Merged %d and %d at %d, %d", i, otherIndex, end->pos.x,
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

ID find_symbol_kind(Circuit *circ, const char *name) {
  CircuitIter it = circ_iter(circ, SymbolKind);
  while (circ_iter_next(&it)) {
    SymbolKind *table = circ_iter_table(&it, SymbolKind);
    for (int i = 0; i < table->length; i++) {
      if (table->name[i] == 0) {
        continue;
      }
      const char *name2 = circ_str_get(circ, table->name[i]);
      if (strcmp(name, name2) == 0) {
        return table->id[i];
      }
    }
  }
  return NO_ID;
}

void import_digital(Circuit *circ, char *buffer) {
  arr(uint32_t) stack = 0;
  arr(WireEnd) inPorts = 0;
  arr(WireEnd) outPorts = 0;
  arr(WireEnd) waypoints = 0;
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

  LXMLNode *circuitNode = find_node(root, "circuit");
  if (!circuitNode) {
    log_debug("No circuit node");
    goto fail;
  }

  LXMLNode *wires = find_node(circuitNode, "wires");
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
            log_debug("Unknown attribute %s", attr->key);
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

  LXMLNode *visualElements = find_node(circuitNode, "visualElements");
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
      ID symbolKindID = NO_ID;
      for (int j = 0; j < sizeof(componentTypes) / sizeof(componentTypes[0]);
           j++) {
        if (strcmp(typeName, componentTypes[j].digName) == 0) {
          symbolKindID = find_symbol_kind(circ, componentTypes[j].ourName);
        }
      }
      if (symbolKindID == NO_ID) {
        log_debug("Unknown symbol kind %s", typeName);
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

      // digital's components are placed relative to the first port
      // find the first port and move the component to the correct position
      ID firstPort = circ_get(circ, symbolKindID, LinkedList).head;
      Position portPos = circ_get(circ, firstPort, Position);
      HMM_Vec2 symPos = HMM_SubV2(HMM_V2(x, y), portPos);

      assert((int)(symPos.X + portPos.X) == x);
      assert((int)(symPos.Y + portPos.Y) == y);

      log_debug("Adding symbol %s at %f, %f", typeName, symPos.X, symPos.Y);
      ID symbolID = circ_add_symbol(circ, circ->top, symbolKindID);
      circ_set_symbol_position(circ, symbolID, symPos);

      bool isInput = strcmp(typeName, "In") == 0;
      bool isOutput = strcmp(typeName, "Out") == 0;

      if (isInput || isOutput) {
        log_debug("  Adding port at %d, %d", x, y);
        replace_wire_end_with_port(
          digWires, digWireEnds, (PortRef){symbolID, firstPort}, (IVec2){x, y},
          isOutput);
      } else {
        IVec2 nextInput = {x, y};
        IVec2 nextOutput = {x + 4 * 20, y + 20};
        if (strcmp(typeName, "Not") == 0) {
          nextOutput = (IVec2){x + 2 * 20, y};
        }
        PortID portID = firstPort;
        while (circ_has(circ, portID)) {
          IVec2 pos = nextInput;
          if (circ_has_tags(circ, portID, TAG_OUT)) {
            pos = nextOutput;
            nextOutput.y += 20;
          } else {
            nextInput.y += 40; // todo: why is this 40?
          }
          const char *portName =
            circ_str_get(circ, circ_get(circ, portID, Name));
          log_debug("Adding port %s at %d, %d", portName, pos.x, pos.y);
          replace_wire_end_with_port(
            digWires, digWireEnds, (PortRef){symbolID, portID}, pos,
            circ_has_tags(circ, portID, TAG_IN));

          portID = circ_get(circ, portID, ListNode).next;
        }
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
          "Invalid wire at %d, %d", digWireEnds[i].key.x, digWireEnds[i].key.y);
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
        log_debug("Wire end %d, %d not in hash", end->pos.x, end->pos.y);
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
          "Invalid wire at %d, %d", digWireEnds[i].key.x, digWireEnds[i].key.y);
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
        log_debug("Wire end %d, %d not in hash", end->pos.x, end->pos.y);
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
            "Trimming free floating wire at %d, %d", digWire->ends[j].pos.x,
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
          "Invalid wire at %d, %d", digWireEnds[i].key.x, digWireEnds[i].key.y);
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
        log_debug("Wire end %d, %d not in hash", end->pos.x, end->pos.y);
        allValid = false;
      }
    }
  }
  assert(allValid);

  // figure out where the waypoints are
  for (int i = 0; i < hmlen(digWireEnds); i++) {
    if (arrlen(digWireEnds[i].value) > 2) {
      log_debug(
        "Junction at %d, %d", digWireEnds[i].key.x, digWireEnds[i].key.y);

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
          arrput(inPorts, *end);
          break;
        case OUT_PORT:
          arrput(outPorts, *end);
          break;
        case WIRE:
          break;
        case WAYPOINT: {
          bool found = false;
          for (int l = 0; l < arrlen(waypoints); l++) {
            if (
              waypoints[l].pos.x == end->pos.x &&
              waypoints[l].pos.y == end->pos.y) {
              found = true;
              break;
            }
          }
          if (!found) {
            WireEnd *otherEnd = &digWire->ends[k == 0 ? 1 : 0];
            PortRef ref = {0};
            if (otherEnd->type == IN_PORT || otherEnd->type == OUT_PORT) {
              ref = otherEnd->portRef;
              log_debug(
                "Waypoint at %d, %d belongs to {%x %x}", end->pos.x, end->pos.y,
                ref.symbol, ref.port);
            } else {
              log_debug(
                "Waypoint at %d, %d has no port", end->pos.x, end->pos.y);
              // todo: pick the closest endpoint and attach the waypoint to it.
              // Ideally we would put these waypoints on the root wire, but we
              // don't know which wire that will be yet.
            }
            arrput(waypoints, ((WireEnd){.pos = end->pos, .portRef = ref}));
          }
          break;
        }
        }
      }
    }

    // determine the two furthest endpoints on the net, those will be the root
    // wire
    float dist = 0;
    WireEnd *rootEnd[2] = {0};
    for (int j = 0; j < arrlen(netWires); j++) {
      DigWire *digWire1 = &digWires[netWires[j]];
      for (int k = 0; k < 2; k++) {
        WireEnd *end1 = &digWire1->ends[k];
        if (end1->type == IN_PORT || end1->type == OUT_PORT) {
          for (int l = 0; l < arrlen(netWires); l++) {
            DigWire *digWire2 = &digWires[netWires[l]];
            for (int m = 0; m < 2; m++) {
              WireEnd *end2 = &digWire2->ends[m];
              if (end2->type == IN_PORT || end2->type == OUT_PORT) {
                float d = HMM_LenV2(
                  HMM_V2(end1->pos.x - end2->pos.x, end1->pos.y - end2->pos.y));
                if (d > dist) {
                  dist = d;
                  rootEnd[0] = end1;
                  rootEnd[1] = end2;
                }
              }
            }
          }
        }
      }
    }

    // find all waypoints without ports attached and attach them to the closest
    // rootEnd
    for (int j = 0; j < arrlen(waypoints); j++) {
      WireEnd *waypoint = &waypoints[j];
      if (waypoint->portRef.symbol == 0) {
        float dist1 = HMM_LenV2(HMM_V2(
          rootEnd[0]->pos.x - waypoint->pos.x,
          rootEnd[0]->pos.y - waypoint->pos.y));
        float dist2 = HMM_LenV2(HMM_V2(
          rootEnd[1]->pos.x - waypoint->pos.x,
          rootEnd[1]->pos.y - waypoint->pos.y));
        if (dist1 < dist2) {
          waypoint->portRef = rootEnd[0]->portRef;
        } else {
          waypoint->portRef = rootEnd[1]->portRef;
        }
        log_debug(
          "Waypoint at %d, %d attached to root {%x %x}", waypoint->pos.x,
          waypoint->pos.y, waypoint->portRef.symbol, waypoint->portRef.port);
      }
    }

    ID netID = circ_add_net(circ, circ->top);
    ID subnetID = circ_add_subnet(circ, netID);
    log_debug("Net %x, Subnet %x", netID, subnetID);

    WireEnd *ports[2] = {inPorts, outPorts};
    for (int k = 0; k < 2; k++) {
      for (int j = 0; j < arrlen(ports[k]); j++) {
        WireEnd end = ports[k][j];
        log_debug(
          "  * %s port {%x, %x}", k == 0 ? "In" : "Out", end.portRef.symbol,
          end.portRef.port);
        ID endpointID = circ_add_endpoint(circ, subnetID);
        circ_connect_endpoint_to_port(
          circ, endpointID, end.portRef.symbol, end.portRef.port);
        for (int j = 0; j < arrlen(waypoints); j++) {
          if (
            waypoints[j].portRef.symbol == end.portRef.symbol &&
            waypoints[j].portRef.port == end.portRef.port) {
            log_debug(
              "    * Waypoint %d %d", waypoints[j].pos.x, waypoints[j].pos.y);
            ID waypointID = circ_add_waypoint(circ, endpointID);
            circ_set_waypoint_position(
              circ, waypointID, HMM_V2(waypoints[j].pos.x, waypoints[j].pos.y));
          }
        }
      }
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