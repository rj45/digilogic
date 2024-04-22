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

  static PortDesc notPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_OUT, .name = "Y"},
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
    [COMP_NOT] = {
      .typeName = "NOT",
      .numPorts = 2,
      .namePrefix = 'X',
      .ports = notPorts,
    }};
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

NetID circuit_add_net(Circuit *circuit, PortID portFrom, PortID portTo) {
  NetID id = arrlen(circuit->nets);
  Net net = {
    .portFrom = portFrom,
    .portTo = portTo,
    .next = NO_NET,
    .prev = NO_NET,
  };
  arrput(circuit->nets, net);

  if (portFrom != NO_PORT) {
    assert(portFrom < arrlen(circuit->ports));
    circuit->ports[portFrom].net = id;
  }

  if (portTo != NO_PORT) {
    assert(portTo < arrlen(circuit->ports));
    circuit->ports[portTo].net = id;
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
