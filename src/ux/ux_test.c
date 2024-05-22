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
#include "utest.h"

#include "ux.h"

UTEST(CircuitUX, init) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ASSERT_EQ(circuit_component_len(&ux.view.circuit), 0);

  ux_free(&ux);
}

UTEST(CircuitUX, adding_components) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ux_start_adding_component(&ux, COMP_AND);

  ASSERT_EQ(circuit_component_len(&ux.view.circuit), 1);
  ASSERT_TRUE(circuit_has(&ux.view.circuit, ux.addingComponent));
  ASSERT_EQ(
    circuit_component_ptr(&ux.view.circuit, ux.addingComponent)->desc,
    COMP_AND);

  ux_change_adding_component(&ux, COMP_OR);

  ASSERT_EQ(circuit_component_len(&ux.view.circuit), 1);
  ASSERT_TRUE(circuit_has(&ux.view.circuit, ux.addingComponent));
  ASSERT_EQ(
    circuit_component_ptr(&ux.view.circuit, ux.addingComponent)->desc, COMP_OR);

  ux_stop_adding_component(&ux);

  ASSERT_EQ(circuit_component_len(&ux.view.circuit), 0);
  ASSERT_EQ(ux.addingComponent, NO_COMPONENT);

  ux_free(&ux);
}
