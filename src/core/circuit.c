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
        .name = "AND",
        .numPorts = 3,
        .ports = andPorts,
      },
    [COMP_OR] =
      {
        .name = "OR",
        .numPorts = 3,
        .ports = orPorts,
      },
    [COMP_NOT] = {
      .name = "NOT",
      .numPorts = 2,
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
}

ComponentID circuit_add_component(Circuit *circuit, ComponentDescID desc) {
  ComponentID id = arrlen(circuit->components);
  PortID portStart = arrlen(circuit->ports);

  Component comp = {.desc = desc, .portStart = portStart};
  arrput(circuit->components, comp);

  int numPorts = circuit->componentDescs[desc].numPorts;
  for (int i = 0; i < numPorts; i++) {
    Port port = {
      .component = id,
      .desc = i,
      .net = NO_NET,
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
