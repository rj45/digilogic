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
        .ports = orPorts,
      },
    [COMP_XOR] =
      {
        .typeName = "XOR",
        .numPorts = 3,
        .namePrefix = 'X',
        .ports = xorPorts,
      },
    [COMP_NOT] =
      {
        .typeName = "NOT",
        .numPorts = 2,
        .namePrefix = 'X',
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
  smap_init(&circuit->sm.junctions, ID_JUNCTION);
  smap_add_synced_array(
    &circuit->sm.junctions, (void **)&circuit->junctions,
    sizeof(*circuit->junctions));
  smap_init(&circuit->sm.wires, ID_WIRE);
  smap_add_synced_array(
    &circuit->sm.wires, (void **)&circuit->wires, sizeof(*circuit->wires));
  smap_init(&circuit->sm.labels, ID_LABEL);
  smap_add_synced_array(
    &circuit->sm.labels, (void **)&circuit->labels, sizeof(*circuit->labels));
}

void circuit_free(Circuit *circuit) {
  smap_free(&circuit->sm.components);
  smap_free(&circuit->sm.ports);
  smap_free(&circuit->sm.nets);
  smap_free(&circuit->sm.junctions);
  smap_free(&circuit->sm.wires);
  smap_free(&circuit->sm.labels);
  arrfree(circuit->text);
  hmfree(circuit->nextName);
}

PortID circuit_add_port(
  Circuit *circuit, ComponentID componentID, ComponentDescID compDesc,
  PortDescID portDesc) {
  PortID id = smap_alloc(&circuit->sm.ports);

  LabelID label = circuit_add_label(
    circuit, circuit->componentDescs[compDesc].ports[portDesc].name);

  Component *component = circuit_component_ptr(circuit, componentID);

  Port *port = circuit_port_ptr(circuit, id);
  *port = (Port){
    .component = componentID,
    .desc = portDesc,
    .net = NO_NET,
    .label = label,
    .compPrev = component->portLast,
  };
  if (component->portLast != NO_PORT) {
    circuit_port_ptr(circuit, component->portLast)->compNext = id;
  }
  component->portLast = id;
  if (component->portFirst == NO_PORT) {
    component->portFirst = id;
  }

  return id;
}

ComponentID circuit_add_component(Circuit *circuit, ComponentDescID desc) {
  ComponentID id = smap_alloc(&circuit->sm.components);

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

  Component *comp = circuit_component_ptr(circuit, id);

  *comp =
    (Component){.desc = desc, .typeLabel = typeLabel, .nameLabel = nameLabel};

  int numPorts = circuit->componentDescs[desc].numPorts;
  for (int i = 0; i < numPorts; i++) {
    circuit_add_port(circuit, id, desc, i);
  }

  return id;
}

NetID circuit_add_net(Circuit *circuit) {
  NetID id = smap_alloc(&circuit->sm.nets);
  Net *net = circuit_net_ptr(circuit, id);
  *net = (Net){
    .portFirst = NO_PORT,
    .portLast = NO_PORT,
    .label = NO_LABEL,
    .wireFirst = NO_WIRE,
    .wireLast = NO_WIRE};

  return id;
}

JunctionID circuit_add_junction(Circuit *circuit) {
  JunctionID id = smap_alloc(&circuit->sm.junctions);
  Junction *junction = circuit_junction_ptr(circuit, id);
  *junction =
    (Junction){.net = NO_NET, .next = NO_JUNCTION, .prev = NO_JUNCTION};
  return id;
}

WireID circuit_add_wire(Circuit *circuit, NetID netID, ID from, ID to) {
  WireID id = smap_alloc(&circuit->sm.wires);
  Wire *wire = circuit_wire_ptr(circuit, id);
  *wire = (Wire){
    .net = netID, .from = from, .to = to, .next = NO_WIRE, .prev = NO_WIRE};

  assert(netID != NO_NET);

  Net *net = circuit_net_ptr(circuit, netID);
  if (net->wireFirst == NO_WIRE) {
    net->wireFirst = id;
  }
  if (net->wireLast != NO_WIRE) {
    circuit_wire_ptr(circuit, net->wireLast)->next = id;
  }
  wire->prev = net->wireLast;
  net->wireLast = id;

  ID ends[2] = {from, to};

  for (int i = 0; i < 2; i++) {
    switch (id_type(ends[i])) {
    case ID_NONE:
      break;
    case ID_PORT: {
      PortID portID = ends[i];
      Port *port = circuit_port_ptr(circuit, portID);
      assert(port->net == NO_NET || port->net == netID);
      if (port->net == NO_NET) {
        port->net = netID;
        if (net->portFirst == NO_PORT) {
          net->portFirst = portID;
        }
        if (net->portLast != NO_PORT) {
          circuit_port_ptr(circuit, net->portLast)->netNext = portID;
        }
        port->netPrev = net->portLast;
        net->portLast = portID;
      }
      break;
    }
    case ID_JUNCTION: {
      JunctionID juncID = ends[i];
      Junction *junction = circuit_junction_ptr(circuit, juncID);
      assert(junction->net == NO_NET || junction->net == netID);
      if (junction->net == NO_NET) {
        junction->net = netID;
        if (net->junctionFirst == NO_PORT) {
          net->junctionFirst = juncID;
        }
        if (net->junctionLast != NO_PORT) {
          circuit_junction_ptr(circuit, net->junctionLast)->next = juncID;
        }
        junction->prev = net->junctionLast;
        net->junctionLast = juncID;
      }
      break;
    }
    default:
      assert(0);
    }
  }

  return id;
}

LabelID circuit_add_label(Circuit *circuit, const char *text) {
  LabelID id = smap_alloc(&circuit->sm.labels);
  Label *label = circuit_label_ptr(circuit, id);
  *label = (Label){.textOffset = arrlen(circuit->text)};

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

  // char *found =
  // memmem(circuit->text, arrlen(circuit->text), text, strlen(text) + 1);
  if (found) {
    label->textOffset = found - circuit->text;
  } else {
    for (const char *c = text; *c; c++) {
      arrput(circuit->text, *c);
    }
    arrput(circuit->text, '\0');
  }

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

  for (JunctionID i = 0; i < circuit_junction_len(circuit); i++) {
    fprintf(file, "  j%d [shape=\"point\"];\n", i);
  }

  fprintf(file, "\n");

  int floatPoint = 0;

  for (WireID i = 0; i < circuit_wire_len(circuit); i++) {
    Wire wire = circuit->wires[i];

    fprintf(file, "  ");

    for (int j = 0; j < 2; j++) {
      ID end = j == 0 ? wire.from : wire.to;
      switch (id_type(end)) {
      case ID_NONE:
        fprintf(file, "f%d[shape=\"point\"]", floatPoint);
        floatPoint++;
        break;
      case ID_JUNCTION:
        fprintf(file, "j%d", circuit_junction_index(circuit, end));
        break;
      case ID_PORT: {
        Port *port = circuit_port_ptr(circuit, end);
        fprintf(
          file, "c%d:p%d", circuit_component_index(circuit, port->component),
          port->desc);
        break;
      }
      default:
        assert(0);
      }
      if (j == 0) {
        fprintf(file, " -- ");
      }
    }
    fprintf(file, ";\n");
  }

  fprintf(file, "}\n");
}