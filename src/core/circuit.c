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

#include "stb_ds.h"
#include <assert.h>
#include <stdio.h>

#include "core.h"

#define LOG_LEVEL LL_INFO
#include "log.h"

const ComponentDesc *circuit_component_descs() {
  static PortDesc andPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_IN, .name = "B"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc orPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_IN, .name = "B"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc xorPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_IN, .name = "B"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc notPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc inputPorts[] = {
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc outputPorts[] = {
    {.direction = PORT_IN, .name = "A"},
  };

  static const ComponentDesc descs[] = {
    [COMP_NONE] =
      {
        .typeName = "NONE",
      },
    [COMP_AND] =
      {
        .typeName = "AND",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_AND,
        .ports = andPorts,
      },
    [COMP_OR] =
      {
        .typeName = "OR",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_OR,
        .ports = orPorts,
      },
    [COMP_XOR] =
      {
        .typeName = "XOR",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_XOR,
        .ports = xorPorts,
      },
    [COMP_NOT] =
      {
        .typeName = "NOT",
        .numPorts = 2,
        .namePrefix = 'X',
        .shape = SHAPE_NOT,
        .ports = notPorts,
      },
    [COMP_INPUT] =
      {
        .typeName = "IN",
        .numPorts = 1,
        .namePrefix = 'I',
        .ports = inputPorts,
      },
    [COMP_OUTPUT] =
      {
        .typeName = "OUT",
        .numPorts = 1,
        .namePrefix = 'O',
        .ports = outputPorts,
      },
  };
  return descs;
}

static PortID circuit_add_port(
  Circuit *circuit, ComponentID componentID, ComponentDescID compDesc,
  PortDescID portDesc) {
  LabelID label = circuit_add_label(
    circuit, circuit->componentDescs[compDesc].ports[portDesc].name, (Box){0});

  Component *component = circuit_component_ptr(circuit, componentID);

  PortID id = smap_add(
    &circuit->sm.ports, &(Port){
                          .component = componentID,
                          .desc = portDesc,
                          .net = NO_NET,
                          .label = label,
                          .prev = component->portLast,
                        });

  if (circuit_has(circuit, component->portLast)) {
    circuit_port_ptr(circuit, component->portLast)->next = id;
    circuit_update_id(circuit, component->portLast);
  }
  component->portLast = id;
  if (!circuit_has(circuit, component->portFirst)) {
    component->portFirst = id;
  }
  circuit_update_id(circuit, componentID);

  return id;
}

static void circuit_augment_component(void *user, ComponentID id, void *ptr) {
  Circuit *circuit = user;
  Component *component = ptr;

  ComponentDescID desc = component->desc;
  int numPorts = circuit->componentDescs[desc].numPorts;
  for (int i = 0; i < numPorts; i++) {
    circuit_add_port(circuit, id, desc, i);
  }
}

static void circuit_augment_waypoint(void *user, WaypointID id, void *ptr) {
  Circuit *circuit = user;
  Waypoint *waypoint = ptr;

  Net *net = circuit_net_ptr(circuit, waypoint->net);
  if (!circuit_has(circuit, net->waypointFirst)) {
    net->waypointFirst = id;
  }
  if (circuit_has(circuit, net->waypointLast)) {
    circuit_waypoint_ptr(circuit, net->waypointLast)->next = id;
    circuit_update_id(circuit, net->waypointLast);
  }
  net->waypointLast = id;
  circuit_update_id(circuit, waypoint->net);
}

static void circuit_augment_endpoint(void *user, EndpointID id, void *ptr) {
  Circuit *circuit = user;
  Endpoint *endpoint = ptr;

  if (circuit_has(circuit, endpoint->port)) {
    circuit_port_ptr(circuit, endpoint->port)->endpoint = id;
  }

  Net *net = circuit_net_ptr(circuit, endpoint->net);
  if (!circuit_has(circuit, net->endpointFirst)) {
    net->endpointFirst = id;
  }
  if (circuit_has(circuit, net->endpointLast)) {
    circuit_endpoint_ptr(circuit, net->endpointLast)->next = id;
    circuit_update_id(circuit, net->endpointLast);
  }
  net->endpointLast = id;
  circuit_update_id(circuit, endpoint->net);
}

static void circuit_componented_deleted(void *user, ID id, void *ptr) {
  Circuit *circuit = user;
  Component *component = ptr;

  PortID portID = component->portFirst;
  while (circuit_has(circuit, portID)) {
    Port *port = circuit_port_ptr(circuit, portID);
    PortID next = port->next;
    circuit_del(circuit, portID);
    portID = next;
  }

  circuit_del(circuit, component->typeLabel);
  circuit_del(circuit, component->nameLabel);
}

static void circuit_port_deleted(void *user, ID id, void *ptr) {
  Circuit *circuit = user;
  Port *port = ptr;

  if (circuit_has(circuit, port->prev)) {
    circuit_port_ptr(circuit, port->prev)->next = port->next;
    circuit_update_id(circuit, port->prev);
  }
  if (circuit_has(circuit, port->next)) {
    circuit_port_ptr(circuit, port->next)->prev = port->prev;
    circuit_update_id(circuit, port->next);
  }

  if (circuit_has(circuit, port->endpoint)) {
    Endpoint *endpoint = circuit_endpoint_ptr(circuit, port->endpoint);
    circuit_update_id(circuit, port->endpoint);
    endpoint->port = NO_PORT;
  }

  circuit_del(circuit, port->label);
}

static void circuit_net_deleted(void *user, ID id, void *ptr) {
  Circuit *circuit = user;
  Net *net = ptr;

  WaypointID waypointID = net->waypointFirst;
  while (circuit_has(circuit, waypointID)) {
    Waypoint *waypoint = circuit_waypoint_ptr(circuit, waypointID);
    WaypointID next = waypoint->next;
    circuit_del(circuit, waypointID);
    waypointID = next;
  }

  EndpointID endpointID = net->endpointFirst;
  while (circuit_has(circuit, endpointID)) {
    Endpoint *endpoint = circuit_endpoint_ptr(circuit, endpointID);
    EndpointID next = endpoint->next;
    circuit_del(circuit, endpointID);
    endpointID = next;
  }

  if (circuit_has(circuit, net->label)) {
    circuit_del(circuit, net->label);
  }
}

static void circuit_waypoint_deleted(void *user, ID id, void *ptr) {
  Circuit *circuit = user;
  Waypoint *waypoint = ptr;

  Net *net = circuit_net_ptr(circuit, waypoint->net);
  if (net->waypointFirst == id) {
    net->waypointFirst = waypoint->next;
  }
  if (net->waypointLast == id) {
    net->waypointLast = waypoint->prev;
  }
  if (circuit_has(circuit, waypoint->prev)) {
    circuit_waypoint_ptr(circuit, waypoint->prev)->next = waypoint->next;
    circuit_update_id(circuit, waypoint->prev);
  }
  if (circuit_has(circuit, waypoint->next)) {
    circuit_waypoint_ptr(circuit, waypoint->next)->prev = waypoint->prev;
    circuit_update_id(circuit, waypoint->next);
  }
  circuit_update_id(circuit, waypoint->net);
}

static void circuit_endpoint_deleted(void *user, ID id, void *ptr) {
  Circuit *circuit = user;
  Endpoint *endpoint = ptr;

  Net *net = circuit_net_ptr(circuit, endpoint->net);
  if (net->endpointFirst == id) {
    net->endpointFirst = endpoint->next;
  }
  if (net->endpointLast == id) {
    net->endpointLast = endpoint->prev;
  }
  if (circuit_has(circuit, endpoint->prev)) {
    circuit_endpoint_ptr(circuit, endpoint->prev)->next = endpoint->next;
    circuit_update_id(circuit, endpoint->prev);
  }
  if (circuit_has(circuit, endpoint->next)) {
    circuit_endpoint_ptr(circuit, endpoint->next)->prev = endpoint->prev;
    circuit_update_id(circuit, endpoint->next);
  }
  circuit_update_id(circuit, endpoint->net);

  if (circuit_has(circuit, endpoint->port)) {
    Port *port = circuit_port_ptr(circuit, endpoint->port);
    port->endpoint = NO_ENDPOINT;
    circuit_update_id(circuit, endpoint->port);
  }
}

void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs) {
  *circuit = (Circuit){.componentDescs = componentDescs};

  smap_init(&circuit->sm.components, ID_COMPONENT);
  smap_add_synced_array(
    &circuit->sm.components, (void **)&circuit->components,
    sizeof(*circuit->components));
  circuit_on_component_create(circuit, circuit, circuit_augment_component);
  circuit_on_component_delete(circuit, circuit, circuit_componented_deleted);

  smap_init(&circuit->sm.ports, ID_PORT);
  smap_add_synced_array(
    &circuit->sm.ports, (void **)&circuit->ports, sizeof(*circuit->ports));
  circuit_on_port_delete(circuit, circuit, circuit_port_deleted);

  smap_init(&circuit->sm.nets, ID_NET);
  smap_add_synced_array(
    &circuit->sm.nets, (void **)&circuit->nets, sizeof(*circuit->nets));
  circuit_on_net_delete(circuit, circuit, circuit_net_deleted);

  smap_init(&circuit->sm.waypoints, ID_WAYPOINT);
  smap_add_synced_array(
    &circuit->sm.waypoints, (void **)&circuit->waypoints,
    sizeof(*circuit->waypoints));
  circuit_on_waypoint_create(circuit, circuit, circuit_augment_waypoint);
  circuit_on_waypoint_delete(circuit, circuit, circuit_waypoint_deleted);

  smap_init(&circuit->sm.endpoints, ID_ENDPOINT);
  smap_add_synced_array(
    &circuit->sm.endpoints, (void **)&circuit->endpoints,
    sizeof(*circuit->endpoints));
  circuit_on_endpoint_create(circuit, circuit, circuit_augment_endpoint);
  circuit_on_endpoint_delete(circuit, circuit, circuit_endpoint_deleted);

  smap_init(&circuit->sm.labels, ID_LABEL);
  smap_add_synced_array(
    &circuit->sm.labels, (void **)&circuit->labels, sizeof(*circuit->labels));
}

void circuit_free(Circuit *circuit) {
  smap_free(&circuit->sm.components);
  smap_free(&circuit->sm.ports);
  smap_free(&circuit->sm.nets);
  smap_free(&circuit->sm.waypoints);
  smap_free(&circuit->sm.endpoints);
  smap_free(&circuit->sm.labels);
  arrfree(circuit->text);
  hmfree(circuit->nextName);
  arrfree(circuit->wires);
  arrfree(circuit->vertices);
}

void circuit_clear(Circuit *circuit) {
  smap_clear(&circuit->sm.components);
  smap_clear(&circuit->sm.ports);
  smap_clear(&circuit->sm.nets);
  smap_clear(&circuit->sm.waypoints);
  smap_clear(&circuit->sm.endpoints);
  smap_clear(&circuit->sm.labels);
  arrsetlen(circuit->text, 0);
  for (int i = 0; i < hmlen(circuit->nextName); i++) {
    hmdel(circuit->nextName, circuit->nextName[i].key);
  }
  arrsetlen(circuit->wires, 0);
  arrsetlen(circuit->vertices, 0);
}

void circuit_clone_from(Circuit *dst, Circuit *src) {
  circuit_clear(dst);
  dst->componentDescs = src->componentDescs;
  for (int i = 0; i < ID_TYPE_COUNT; i++) {
    smap_clone_from(&dst->sparsemaps[i], &src->sparsemaps[i]);
  }
  arrsetlen(dst->text, arrlen(src->text));
  memcpy(dst->text, src->text, arrlen(src->text));

  for (int i = 0; i < hmlen(dst->nextName); i++) {
    hmdel(dst->nextName, dst->nextName[i].key);
  }
  for (int i = 0; i < hmlen(src->nextName); i++) {
    hmput(dst->nextName, src->nextName[i].key, src->nextName[i].value);
  }

  arrsetlen(dst->wires, arrlen(src->wires));
  memcpy(dst->wires, src->wires, arrlen(src->wires));

  arrsetlen(dst->vertices, arrlen(src->vertices));
  memcpy(dst->vertices, src->vertices, arrlen(src->vertices));
}

ComponentID circuit_add_component(
  Circuit *circuit, ComponentDescID desc, HMM_Vec2 position) {

  LabelID typeLabel = circuit_add_label(
    circuit, circuit->componentDescs[desc].typeName, (Box){0});

  int num = hmget(circuit->nextName, circuit->componentDescs[desc].namePrefix);
  if (num < 1) {
    num = 1;
  }
  hmput(circuit->nextName, circuit->componentDescs[desc].namePrefix, num + 1);
  char name[256];
  snprintf(
    name, sizeof(name), "%c%d", circuit->componentDescs[desc].namePrefix, num);

  LabelID nameLabel = circuit_add_label(circuit, name, (Box){0});

  ComponentID id = smap_add(
    &circuit->sm.components, &(Component){
                               .desc = desc,
                               .typeLabel = typeLabel,
                               .nameLabel = nameLabel,
                               .box.center = position,
                             });
  // NOTE: Do not add code here to further set up components, add it to
  // circuit_augment_component instead. Otherwise the view on_create callback
  // will not see the changes.
  return id;
}

void circuit_move_component(Circuit *circuit, ComponentID id, HMM_Vec2 delta) {
  Component *component = circuit_component_ptr(circuit, id);
  assert(!isnan(delta.X));
  assert(!isnan(delta.Y));

  circuit_move_component_to(
    circuit, id, HMM_AddV2(component->box.center, delta));
}

void circuit_move_component_to(Circuit *circuit, ComponentID id, HMM_Vec2 pos) {
  Component *component = circuit_component_ptr(circuit, id);
  component->box.center = pos;
  log_debug("Moving component %x to %f %f", id, pos.X, pos.Y);
  circuit_update_id(circuit, id);
}

NetID circuit_add_net(Circuit *circuit) {
  return smap_add(&circuit->sm.nets, &(Net){0});
}

void circuit_move_endpoint_to(
  Circuit *circuit, EndpointID id, HMM_Vec2 position) {
  Endpoint *endpoint = circuit_endpoint_ptr(circuit, id);
  endpoint->position = position;
  circuit_update_id(circuit, id);
}

WaypointID
circuit_add_waypoint(Circuit *circuit, NetID netID, HMM_Vec2 position) {
  assert(circuit_has(circuit, netID));
  Net *net = circuit_net_ptr(circuit, netID);

  WaypointID id = smap_add(
    &circuit->sm.waypoints, &(Waypoint){
                              .net = netID,
                              .position = position,
                              .prev = net->waypointLast,
                            });
  // NOTE: Do not add code here to further set up waypoints, add it to
  // circuit_augment_waypoint instead.
  return id;
}

void circuit_move_waypoint(Circuit *circuit, WaypointID id, HMM_Vec2 delta) {
  Waypoint *waypoint = circuit_waypoint_ptr(circuit, id);
  waypoint->position = HMM_AddV2(waypoint->position, delta);
  circuit_update_id(circuit, id);
}

EndpointID circuit_add_endpoint(
  Circuit *circuit, NetID netID, PortID portID, HMM_Vec2 position) {
  if (!smap_has(&circuit->sm.ports, portID)) {
    portID = NO_PORT;
  } else {
    Port *port = circuit_port_ptr(circuit, portID);
    Component *component = circuit_component_ptr(circuit, port->component);
    position = HMM_AddV2(component->box.center, port->position);
  }
  assert(circuit_has(circuit, netID));
  Net *net = circuit_net_ptr(circuit, netID);

  EndpointID id = smap_add(
    &circuit->sm.endpoints, &(Endpoint){
                              .net = netID,
                              .port = portID,
                              .position = position,
                              .prev = net->endpointLast});

  // NOTE: Do not add code here to further set up endpoints, add it to
  // circuit_augment_endpoint instead.
  return id;
}

void circuit_endpoint_connect(
  Circuit *circuit, EndpointID endpointID, PortID portID) {
  log_debug("Connecting endpoint %x to port %x", endpointID, portID);

  Endpoint *endpoint = circuit_endpoint_ptr(circuit, endpointID);

  endpoint->port = portID;

  Port *port = circuit_port_ptr(circuit, portID);
  port->endpoint = endpointID;

  Component *component = circuit_component_ptr(circuit, port->component);
  endpoint->position = HMM_AddV2(component->box.center, port->position);

  circuit_update_id(circuit, endpointID);
}

LabelID circuit_add_label(Circuit *circuit, const char *text, Box bounds) {
  char *found = NULL;
  int searchlen = strlen(text) + 1;
  int matches = 0;
  for (int i = 0; i < arrlen(circuit->text); i++) {
    if (circuit->text[i] == text[matches]) {
      matches++;
      if (matches == searchlen) {
        found = &circuit->text[i - matches + 1];
        break;
      }
    } else {
      matches = 0;
    }
  }

  uint32_t textOffset = arrlen(circuit->text);
  if (found) {
    textOffset = found - circuit->text;
  } else {
    for (const char *c = text; *c; c++) {
      arrput(circuit->text, *c);
    }
    arrput(circuit->text, '\0');
  }

  LabelID id = smap_add(
    &circuit->sm.labels, &(Label){.textOffset = textOffset, .box = bounds});

  return id;
}

const char *circuit_label_text(Circuit *circuit, LabelID id) {
  return circuit->text + circuit_label_ptr(circuit, id)->textOffset;
}

void circuit_write_dot(Circuit *circuit, FILE *file) {
  fprintf(file, "graph {\n");
  fprintf(file, "  rankdir=LR;\n");
  fprintf(file, "  node [shape=record];\n");

  for (ComponentID i = 0; i < circuit_component_len(circuit); i++) {
    Component comp = circuit->components[i];
    const ComponentDesc *desc = &circuit->componentDescs[comp.desc];
    // const char *typeName = circuit_label_text(circuit, comp.typeLabel);
    const char *name = circuit_label_text(circuit, comp.nameLabel);
    fprintf(file, "  c%d [label=\"%s %s|", i, desc->typeName, name);

    for (int j = 0; j < desc->numPorts; j++) {
      PortDesc *portDesc = &desc->ports[j];
      fprintf(file, "<p%d>%s", j, portDesc->name);
      if (j < desc->numPorts - 1) {
        fprintf(file, "|");
      }
    }

    fprintf(file, "\"];\n");
  }

  fprintf(file, "\n");

  int floatPoint = 0;
  char startEndpointText[256];

  for (int netIndex = 0; netIndex < circuit_net_len(circuit); netIndex++) {
    Net *net = &circuit->nets[netIndex];
    EndpointID startEndpointID = net->endpointFirst;
    while (circuit_has(circuit, startEndpointID)) {
      Endpoint *startEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);

      if (circuit_has(circuit, startEndpoint->port)) {
        Port *port = circuit_port_ptr(circuit, startEndpoint->port);
        snprintf(
          startEndpointText, 256, "c%d:p%d",
          circuit_index(circuit, port->component), port->desc);
      } else {
        snprintf(startEndpointText, 256, "f%d", floatPoint++);
      }

      EndpointID endEndpointID = net->endpointFirst;
      while (circuit_has(circuit, endEndpointID) &&
             endEndpointID != startEndpointID) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        endEndpointID = endEndpoint->next;
      }
      if (endEndpointID == startEndpointID) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        endEndpointID = endEndpoint->next;
      }
      while (circuit_has(circuit, endEndpointID)) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        if (circuit_has(circuit, endEndpoint->port)) {
          Port *port = circuit_port_ptr(circuit, endEndpoint->port);
          fprintf(
            file, "  %s -- c%d:p%d;", startEndpointText,
            circuit_index(circuit, port->component), port->desc);
        } else {
          fprintf(file, "  %s -- f%d", startEndpointText, floatPoint++);
        }
        endEndpointID = endEndpoint->next;
      }
      startEndpointID = startEndpoint->next;
    }
  }

  fprintf(file, "}\n");
}
