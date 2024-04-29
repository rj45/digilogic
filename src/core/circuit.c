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
}

void circuit_free(Circuit *circuit) {
  arrfree(circuit->components);
  arrfree(circuit->ports);
  arrfree(circuit->nets);
  arrfree(circuit->labels);
  arrfree(circuit->wires);
  arrfree(circuit->junctions);
  arrfree(circuit->text);
  hmfree(circuit->nextName);
}

ComponentID circuit_add_component(Circuit *circuit, ComponentDescID desc) {
  ComponentID id = arrlen(circuit->components);
  PortID portStart = arrlen(circuit->ports);

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

  Component comp = {
    .desc = desc,
    .portStart = portStart,
    .typeLabel = typeLabel,
    .nameLabel = nameLabel};
  arrput(circuit->components, comp);

  int numPorts = circuit->componentDescs[desc].numPorts;
  for (int i = 0; i < numPorts; i++) {
    LabelID label =
      circuit_add_label(circuit, circuit->componentDescs[desc].ports[i].name);

    Port port = {
      .component = id,
      .desc = i,
      .net = NO_NET,
      .label = label,
      .next = NO_PORT,
      .prev = NO_PORT};
    arrput(circuit->ports, port);
  }

  return id;
}

NetID circuit_add_net(Circuit *circuit) {
  NetID id = arrlen(circuit->nets);
  Net net = {
    .portFirst = NO_PORT,
    .portLast = NO_PORT,
    .label = NO_LABEL,
    .wireFirst = NO_WIRE,
    .wireLast = NO_WIRE};
  arrput(circuit->nets, net);

  return id;
}

JunctionID circuit_add_junction(Circuit *circuit) {
  JunctionID id = arrlen(circuit->junctions);
  Junction junction = {.net = NO_NET, .next = NO_JUNCTION, .prev = NO_JUNCTION};
  arrput(circuit->junctions, junction);
  return id;
}

WireID
circuit_add_wire(Circuit *circuit, NetID netID, WireEndID from, WireEndID to) {
  WireID id = arrlen(circuit->wires);
  Wire wire = {
    .net = netID, .from = from, .to = to, .next = NO_WIRE, .prev = NO_WIRE};
  arrput(circuit->wires, wire);

  assert(netID != NO_NET);

  Net *net = &circuit->nets[netID];
  if (net->wireFirst == NO_WIRE) {
    net->wireFirst = id;
  } else {
    circuit->wires[net->wireLast].next = id;
  }
  circuit->wires[id].prev = net->wireLast;
  net->wireLast = id;

  WireEndID ends[2] = {from, to};

  for (int i = 0; i < 2; i++) {
    uint32_t index = wire_end_index(ends[i]);
    switch (wire_end_type(ends[i])) {
    case WIRE_END_INVALID:
      assert(0);
      break;
    case WIRE_END_NONE:
      break;
    case WIRE_END_PORT: {
      Port *port = &circuit->ports[index];
      assert(port->net == NO_NET || port->net == netID);
      if (port->net == NO_NET) {
        port->net = netID;
        if (net->portFirst == NO_PORT) {
          net->portFirst = index;
        } else {
          circuit->ports[net->portLast].next = index;
        }
        port->prev = net->portLast;
        assert(port->next == NO_PORT);
        net->portLast = index;
      }
      break;
    }
    case WIRE_END_JUNC: {
      Junction *junction = &circuit->junctions[index];
      assert(junction->net == NO_NET || junction->net == netID);
      if (junction->net == NO_NET) {
        junction->net = netID;
        if (net->junctionFirst == NO_JUNCTION) {
          net->junctionFirst = index;
        } else {
          circuit->junctions[net->junctionLast].next = index;
        }
        junction->prev = net->junctionLast;
        net->junctionLast = index;
      }
      break;
    }
    }
  }

  return id;
}

LabelID circuit_add_label(Circuit *circuit, const char *text) {
  LabelID id = arrlen(circuit->labels);
  Label label = {.textOffset = arrlen(circuit->text)};

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
    label.textOffset = found - circuit->text;
  } else {
    for (const char *c = text; *c; c++) {
      arrput(circuit->text, *c);
    }
    arrput(circuit->text, '\0');
  }

  arrput(circuit->labels, label);
  return id;
}

const char *circuit_label_text(Circuit *circuit, LabelID id) {
  return circuit->text + circuit->labels[id].textOffset;
}

void circuit_write_dot(Circuit *circuit, FILE *file) {
  fprintf(file, "graph {\n");
  fprintf(file, "  rankdir=LR;\n");
  fprintf(file, "  node [shape=record];\n");

  for (ComponentID i = 0; i < arrlen(circuit->components); i++) {
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

  for (JunctionID i = 0; i < arrlen(circuit->junctions); i++) {
    fprintf(file, "  j%d [shape=\"point\"];\n", i);
  }

  fprintf(file, "\n");

  int floatPoint = 0;

  for (WireID i = 0; i < arrlen(circuit->wires); i++) {
    Wire wire = circuit->wires[i];

    fprintf(file, "  ");

    for (int j = 0; j < 2; j++) {
      WireEndID end = j == 0 ? wire.from : wire.to;
      switch (wire_end_type(end)) {
      case WIRE_END_INVALID:
        assert(0);
        break;
      case WIRE_END_NONE:
        fprintf(file, "f%d[shape=\"point\"]", floatPoint);
        floatPoint++;
        break;
      case WIRE_END_JUNC:
        fprintf(file, "j%d", wire_end_index(end));
        break;
      case WIRE_END_PORT: {
        Port *port = &circuit->ports[wire_end_index(end)];
        fprintf(file, "c%d:p%d", port->component, port->desc);
        break;
      }
      }
      if (j == 0) {
        fprintf(file, " -- ");
      }
    }
    fprintf(file, ";\n");
  }

  fprintf(file, "}\n");
}