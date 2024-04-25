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
    };
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
circuit_add_wire(Circuit *circuit, NetID net, WireEndID from, WireEndID to) {
  WireID id = arrlen(circuit->wires);
  Wire wire = {.net = net, .from = from, .to = to};
  arrput(circuit->wires, wire);

  if (net != NO_NET) {
    Net *n = &circuit->nets[net];
    if (n->wireFirst == NO_WIRE) {
      n->wireFirst = id;
    } else {
      circuit->wires[n->wireLast].next = id;
    }
    n->wireLast = id;
  }

  Net *n = &circuit->nets[net];
  WireEndID ends[2] = {from, to};

  for (int i = 0; i < 2; i++) {
    switch (wire_end_type(ends[i])) {
    case WIRE_END_INVALID:
      assert(0);
      break;
    case WIRE_END_NONE:
      break;
    case WIRE_END_PORT:
      assert(circuit->ports[wire_end_index(ends[i])].net == NO_NET);
      circuit->ports[wire_end_index(ends[i])].net = net;
      if (n->portFirst == NO_PORT) {
        n->portFirst = wire_end_index(ends[i]);
      } else {
        circuit->ports[n->portLast].next = wire_end_index(ends[i]);
      }
      n->portLast = wire_end_index(ends[i]);
      break;
    case WIRE_END_JUNC:
      assert(
        circuit->junctions[wire_end_index(ends[i])].net == NO_NET ||
        circuit->junctions[wire_end_index(ends[i])].net == net);
      circuit->junctions[wire_end_index(ends[i])].net = net;
      if (n->junctionFirst == NO_JUNCTION) {
        n->junctionFirst = wire_end_index(ends[i]);
      } else {
        circuit->junctions[n->junctionLast].next = wire_end_index(ends[i]);
      }
      n->junctionLast = wire_end_index(ends[i]);
      break;
    }
  }

  return id;
}

LabelID circuit_add_label(Circuit *circuit, const char *text) {
  LabelID id = arrlen(circuit->labels);
  Label label = {.textOffset = arrlen(circuit->text)};

  char *found =
    memmem(circuit->text, arrlen(circuit->text), text, strlen(text) + 1);
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
