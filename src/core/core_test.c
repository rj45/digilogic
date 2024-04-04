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
#include "utest.h"

#include "core.h"
#include "utest.h"

UTEST(Circuit, add_component) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  ComponentID id = circuit_add_component(&circuit, COMP_AND);
  ASSERT_EQ(id, 0);
  ASSERT_EQ(arrlen(circuit.components), 1);
  ASSERT_EQ(circuit.components[id].desc, COMP_AND);
  ASSERT_EQ(circuit.components[id].portStart, 0);
  ASSERT_EQ(arrlen(circuit.ports), 3);
  circuit_free(&circuit);
}

UTEST(Circuit, add_net_no_ports) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  NetID id = circuit_add_net(&circuit, NO_PORT, NO_PORT);
  ASSERT_EQ(id, 0);
  ASSERT_EQ(arrlen(circuit.nets), 1);
  ASSERT_EQ(circuit.nets[id].portFrom, NO_PORT);
  ASSERT_EQ(circuit.nets[id].portTo, NO_PORT);
  circuit_free(&circuit);
}

UTEST(Circuit, add_net_with_ports) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  ComponentID compID = circuit_add_component(&circuit, COMP_AND);
  PortID portID = circuit.components[compID].portStart;
  NetID id = circuit_add_net(&circuit, portID, portID + 1);
  ASSERT_EQ(id, 0);
  ASSERT_EQ(arrlen(circuit.nets), 1);
  ASSERT_EQ(circuit.nets[id].portFrom, portID);
  ASSERT_EQ(circuit.ports[portID].net, id);
  ASSERT_EQ(circuit.nets[id].portTo, portID + 1);
  ASSERT_EQ(circuit.ports[portID + 1].net, id);
  ASSERT_EQ(circuit.nets[id].next, NO_NET);
  ASSERT_EQ(circuit.nets[id].prev, NO_NET);
  circuit_free(&circuit);
}