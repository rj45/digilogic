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

void circuit_init(Circuit *circuit, const ComponentDesc *componentDescs) {
  *circuit = (Circuit){.componentDescs = componentDescs};

  smap_init(&circuit->sm.components, ID_COMPONENT);
  smap_add_synced_array(
    &circuit->sm.components, (void **)&circuit->components,
    sizeof(*circuit->components));

  smap_init(&circuit->sm.ports, ID_PORT);
  smap_add_synced_array(
    &circuit->sm.ports, (void **)&circuit->ports, sizeof(*circuit->ports));
  smap_init(&circuit->sm.nets, ID_NET);
  smap_add_synced_array(
    &circuit->sm.nets, (void **)&circuit->nets, sizeof(*circuit->nets));
  smap_init(&circuit->sm.waypoints, ID_WAYPOINT);
  smap_add_synced_array(
    &circuit->sm.waypoints, (void **)&circuit->waypoints,
    sizeof(*circuit->waypoints));
  smap_init(&circuit->sm.endpoints, ID_ENDPOINT);
  smap_add_synced_array(
    &circuit->sm.endpoints, (void **)&circuit->endpoints,
    sizeof(*circuit->endpoints));
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

PortID circuit_add_port(
  Circuit *circuit, ComponentID componentID, ComponentDescID compDesc,
  PortDescID portDesc) {
  LabelID label = circuit_add_label(
    circuit, circuit->componentDescs[compDesc].ports[portDesc].name);

  Component *component = circuit_component_ptr(circuit, componentID);

  PortID id = smap_add(
    &circuit->sm.ports, &(Port){
                          .component = componentID,
                          .desc = portDesc,
                          .net = NO_NET,
                          .label = label,
                          .compPrev = component->portLast,
                        });

  if (component->portLast != NO_PORT) {
    circuit_port_ptr(circuit, component->portLast)->compNext = id;
    circuit_port_update_id(circuit, component->portLast);
  }
  component->portLast = id;
  if (component->portFirst == NO_PORT) {
    component->portFirst = id;
  }
  circuit_component_update_id(circuit, componentID);

  return id;
}

ComponentID circuit_add_component(
  Circuit *circuit, ComponentDescID desc, HMM_Vec2 position) {

  LabelID typeLabel =
    circuit_add_label(circuit, circuit->componentDescs[desc].typeName);

  int num = hmget(circuit->nextName, circuit->componentDescs[desc].namePrefix);
  if (num < 1) {
    num = 1;
  }
  hmput(circuit->nextName, circuit->componentDescs[desc].namePrefix, num + 1);
  char name[256];
  snprintf(
    name, sizeof(name), "%c%d", circuit->componentDescs[desc].namePrefix, num);

  LabelID nameLabel = circuit_add_label(circuit, name);

  ComponentID id = smap_add(
    &circuit->sm.components, &(Component){
                               .desc = desc,
                               .typeLabel = typeLabel,
                               .nameLabel = nameLabel,
                               .box.center = position,
                             });

  int numPorts = circuit->componentDescs[desc].numPorts;
  for (int i = 0; i < numPorts; i++) {
    circuit_add_port(circuit, id, desc, i);
  }

  return id;
}

NetID circuit_add_net(Circuit *circuit) {
  return smap_add(&circuit->sm.nets, &(Net){0});
}

WaypointID
circuit_add_waypoint(Circuit *circuit, NetID netID, HMM_Vec2 position) {
  assert(netID != NO_NET);
  Net *net = circuit_net_ptr(circuit, netID);

  WaypointID id = smap_add(
    &circuit->sm.waypoints, &(Waypoint){
                              .net = netID,
                              .position = position,
                              .prev = net->waypointLast,
                            });

  if (net->waypointFirst == NO_ENDPOINT) {
    net->waypointFirst = id;
  }
  if (net->waypointLast != NO_ENDPOINT) {
    circuit_waypoint_ptr(circuit, net->waypointLast)->next = id;
    circuit_waypoint_update_id(circuit, net->waypointLast);
  }
  net->waypointLast = id;
  circuit_net_update_id(circuit, netID);
  return id;
}

EndpointID circuit_add_endpoint(
  Circuit *circuit, NetID netID, PortID portID, HMM_Vec2 position) {
  if (!smap_has(&circuit->sm.ports, portID)) {
    portID = NO_PORT;
  }
  assert(netID != NO_NET);
  Net *net = circuit_net_ptr(circuit, netID);

  EndpointID id = smap_add(
    &circuit->sm.endpoints, &(Endpoint){
                              .net = netID,
                              .port = portID,
                              .position = position,
                              .prev = net->endpointLast});

  circuit_port_ptr(circuit, portID)->endpoint = id;

  if (net->endpointFirst == NO_ENDPOINT) {
    net->endpointFirst = id;
  }
  if (net->endpointLast != NO_ENDPOINT) {
    circuit_endpoint_ptr(circuit, net->endpointLast)->next = id;
    circuit_endpoint_update_id(circuit, net->endpointLast);
  }
  net->endpointLast = id;
  circuit_net_update_id(circuit, netID);

  return id;
}

LabelID circuit_add_label(Circuit *circuit, const char *text) {
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

  LabelID id =
    smap_add(&circuit->sm.labels, &(Label){.textOffset = textOffset});

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
    while (startEndpointID != NO_ENDPOINT) {
      Endpoint *startEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);

      if (startEndpoint->port != NO_PORT) {
        Port *port = circuit_port_ptr(circuit, startEndpoint->port);
        snprintf(
          startEndpointText, 256, "c%d:p%d",
          circuit_component_index(circuit, port->component), port->desc);
      } else {
        snprintf(startEndpointText, 256, "f%d", floatPoint++);
      }

      EndpointID endEndpointID = net->endpointFirst;
      while (endEndpointID != NO_ENDPOINT && endEndpointID != startEndpointID) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        endEndpointID = endEndpoint->next;
      }
      if (endEndpointID == startEndpointID) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        endEndpointID = endEndpoint->next;
      }
      while (endEndpointID != NO_ENDPOINT) {
        Endpoint *endEndpoint = circuit_endpoint_ptr(circuit, startEndpointID);
        if (endEndpoint->port != NO_PORT) {
          Port *port = circuit_port_ptr(circuit, endEndpoint->port);
          fprintf(
            file, "  %s -- c%d:p%d;", startEndpointText,
            circuit_component_index(circuit, port->component), port->desc);
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