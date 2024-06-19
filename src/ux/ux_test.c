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

  ID andSymbolKindID = NO_ID;
  ID orSymbolKindID = NO_ID;
  CircuitIter it = circ_iter(&ux.view.circuit2, SymbolKind2);
  while (circ_iter_next(&it)) {
    SymbolKind2 *table = circ_iter_table(&it, SymbolKind2);
    for (size_t i = 0; i < table->length; i++) {
      const char *name = circ_str_get(&ux.view.circuit2, table->name[i]);
      if (strcmp(name, "AND") == 0) {
        andSymbolKindID = table->id[i];
      } else if (strcmp(name, "OR") == 0) {
        orSymbolKindID = table->id[i];
      }
    }
  }

  ux_start_adding_symbol(&ux, andSymbolKindID);

  ASSERT_TRUE(circ_has(&ux.view.circuit2, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit2, ux.addingSymbol, SymbolKindID),
    andSymbolKindID);

  ID oldID = ux.addingSymbol;

  ux_change_adding_symbol(&ux, orSymbolKindID);

  ASSERT_FALSE(circ_has(&ux.view.circuit2, oldID));
  ASSERT_TRUE(circ_has(&ux.view.circuit2, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit2, ux.addingSymbol, SymbolKindID), orSymbolKindID);

  oldID = ux.addingSymbol;

  ux_stop_adding_symbol(&ux);

  ASSERT_FALSE(circ_has(&ux.view.circuit2, oldID));
  ASSERT_EQ(ux.addingSymbol, NO_ID);

  ux_free(&ux);
}
