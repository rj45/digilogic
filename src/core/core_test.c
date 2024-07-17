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

#include "utest.h"

#include "core.h"
#include "utest.h"

static HMM_Vec2 testTextSize(void *user, const char *text) {
  return HMM_V2(strlen(text) * 8, 8);
}

UTEST(bv, setlen) {
  bv(uint64_t) bv = NULL;
  bv_setlen(bv, 100);
  ASSERT_EQ(arrlen(bv), 2);
  bv_free(bv);
}

UTEST(bv, setlen_even) {
  bv(uint64_t) bv = NULL;
  bv_setlen(bv, 64);
  ASSERT_EQ(arrlen(bv), 1);
  bv_free(bv);
}

UTEST(bv, set_bit) {
  bv(uint64_t) bv = NULL;
  bv_setlen(bv, 100);
  bv_clear_all(bv);
  bv_set(bv, 10);
  ASSERT_FALSE(bv_is_set(bv, 9));
  ASSERT_TRUE(bv_is_set(bv, 10));
  bv_free(bv);
}

UTEST(bv, clear_bit) {
  bv(uint64_t) bv = NULL;
  bv_setlen(bv, 100);
  bv_set_all(bv);
  bv_clear(bv, 10);
  ASSERT_TRUE(bv_is_set(bv, 9));
  ASSERT_FALSE(bv_is_set(bv, 10));
  bv_free(bv);
}

////////////////////////
// ChangeLog tests
////////////////////////

UTEST(ChangeLog, undo_3_redo_3) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  SymbolLayout layout = (SymbolLayout){
    .portSpacing = 20.0f,
    .symbolWidth = 55.0f,
    .borderWidth = 1.0f,
    .labelPadding = 2.0f,
    .user = NULL,
    .textSize = testTextSize,
  };
  circ_load_symbol_descs(&circuit, &layout, circuit_symbol_descs(), COMP_COUNT);

  circuit.top = circ_add_module(&circuit);
  circ_snapshot(&circuit);

  ID symID1 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "AND"));
  circ_commit(&circuit);
  ID symID2 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "OR"));
  circ_commit(&circuit);
  ID symID3 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "XOR"));
  circ_commit(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 3);

  circ_undo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 2);
  ASSERT_FALSE(circ_has(&circuit, symID3));

  circ_undo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 1);
  ASSERT_FALSE(circ_has(&circuit, symID2));

  circ_undo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 0);
  ASSERT_FALSE(circ_has(&circuit, symID1));

  // should do nothing
  circ_undo(&circuit);

  circ_redo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 1);
  ASSERT_TRUE(circ_has(&circuit, symID1));

  circ_redo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 2);
  ASSERT_TRUE(circ_has(&circuit, symID2));

  circ_redo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 3);
  ASSERT_TRUE(circ_has(&circuit, symID3));

  // should do nothing
  circ_redo(&circuit);

  circ_free(&circuit);
}

UTEST(ChangeLog, undo_redo_undo_redo) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  SymbolLayout layout = (SymbolLayout){
    .portSpacing = 20.0f,
    .symbolWidth = 55.0f,
    .borderWidth = 1.0f,
    .labelPadding = 2.0f,
    .user = NULL,
    .textSize = testTextSize,
  };
  circ_load_symbol_descs(&circuit, &layout, circuit_symbol_descs(), COMP_COUNT);

  circuit.top = circ_add_module(&circuit);
  circ_snapshot(&circuit);

  circ_commit(&circuit);

  ID symID1 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "AND"));
  ID symID2 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "OR"));
  ID symID3 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "XOR"));
  circ_commit(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 3);

  circ_undo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 0);

  circ_redo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 3);

  circ_undo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 0);

  circ_redo(&circuit);

  ASSERT_EQ(circ_len(&circuit, Symbol), 3);

  ASSERT_TRUE(circ_has(&circuit, symID1));
  ASSERT_TRUE(circ_has(&circuit, symID2));
  ASSERT_TRUE(circ_has(&circuit, symID3));

  circ_free(&circuit);
}

UTEST(ChangeLog, discard) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  SymbolLayout layout = (SymbolLayout){
    .portSpacing = 20.0f,
    .symbolWidth = 55.0f,
    .borderWidth = 1.0f,
    .labelPadding = 2.0f,
    .user = NULL,
    .textSize = testTextSize,
  };
  circ_load_symbol_descs(&circuit, &layout, circuit_symbol_descs(), COMP_COUNT);

  circuit.top = circ_add_module(&circuit);
  circ_snapshot(&circuit);

  circ_commit(&circuit);

  ID symID1 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "AND"));
  circ_commit(&circuit);
  ID symID2 = circ_add_symbol(
    &circuit, circuit.top, circ_get_symbol_kind_by_name(&circuit, "OR"));

  ASSERT_TRUE(circ_has(&circuit, symID1));
  ASSERT_TRUE(circ_has(&circuit, symID2));

  circ_discard_since_last_commit(&circuit);

  ASSERT_FALSE(circ_has(&circuit, symID2));

  circ_free(&circuit);
}

////////////////////////
// Circuit tests
////////////////////////

UTEST(Circuit, circ_table_components_ptr_ptr) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  void **ptr = circ_table_components_ptr_ptr(&circuit, TYPE_ENDPOINT, 4);
  ASSERT_EQ(ptr, (void **)&circuit.endpoint.port);
  circ_free(&circuit);
}

UTEST(Circuit, circ_table_components_ptr) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  void *ptr = circ_table_components_ptr(&circuit, TYPE_ENDPOINT, 4);
  ASSERT_EQ(ptr, (void *)circuit.endpoint.port);
  circ_free(&circuit);
}

UTEST(Circuit, circ_table_component_ptr) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  circ_add(&circuit, Endpoint);
  circ_add(&circuit, Endpoint);
  void *ptr = circ_table_component_ptr(&circuit, TYPE_ENDPOINT, 4, 1);
  ASSERT_EQ(ptr, (void *)&circuit.endpoint.port[1]);
  circ_free(&circuit);
}

UTEST(Circuit, circ_add_entity_id) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id = circ_add(&circuit, Endpoint);
  ASSERT_EQ(circuit.endpoint.id[0], id);
  ASSERT_EQ(circuit.generations[id_index(id)], id_gen(id));
  ASSERT_EQ(circuit.endpoint.length, 1);
  ASSERT_EQ(circuit.rows[id_index(id)], 0);
  circ_free(&circuit);
}

UTEST(Circuit, circ_set) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id = circ_add(&circuit, Endpoint);
  circuit.endpoint.port[circuit.rows[id_index(id)]] = (PortRef){.symbol = 1};
  // circ_set(&circuit, id, endpoint, port, (&(PortRef){.symbol = 1}));
  ASSERT_EQ(circuit.endpoint.port[0].symbol, 1);
  circ_free(&circuit);
}

UTEST(Circuit, circ_remove_enitity_end) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id1 = circ_add(&circuit, Endpoint);
  ID id2 = circ_add(&circuit, Endpoint);
  ID id3 = circ_add(&circuit, Endpoint);

  ASSERT_EQ(circuit.endpoint.length, 3);

  circ_remove(&circuit, id3);

  ASSERT_EQ(circuit.endpoint.length, 2);
  ASSERT_TRUE(circ_has(&circuit, id1));
  ASSERT_TRUE(circ_has(&circuit, id2));
  ASSERT_FALSE(circ_has(&circuit, id3));

  circ_remove(&circuit, id2);

  ASSERT_EQ(circuit.endpoint.length, 1);
  ASSERT_TRUE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_FALSE(circ_has(&circuit, id3));

  circ_remove(&circuit, id1);

  ASSERT_EQ(circuit.endpoint.length, 0);
  ASSERT_EQ(circuit.generations[id_index(id1)], 0);
  ASSERT_EQ(circuit.generations[id_index(id2)], 0);
  ASSERT_EQ(circuit.generations[id_index(id3)], 0);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_FALSE(circ_has(&circuit, id3));
  circ_free(&circuit);
}

UTEST(Circuit, circ_remove_enitity_middle) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id1 = circ_add(&circuit, Endpoint);
  ID id2 = circ_add(&circuit, Endpoint);
  ID id3 = circ_add(&circuit, Endpoint);

  ASSERT_EQ(circuit.endpoint.length, 3);

  circ_remove(&circuit, id2);

  ASSERT_EQ(circuit.endpoint.length, 2);
  ASSERT_TRUE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_TRUE(circ_has(&circuit, id3));

  circ_remove(&circuit, id1);

  ASSERT_EQ(circuit.endpoint.length, 1);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_TRUE(circ_has(&circuit, id3));

  circ_remove(&circuit, id3);

  ASSERT_EQ(circuit.endpoint.length, 0);
  ASSERT_EQ(circuit.generations[id_index(id1)], 0);
  ASSERT_EQ(circuit.generations[id_index(id2)], 0);
  ASSERT_EQ(circuit.generations[id_index(id3)], 0);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_FALSE(circ_has(&circuit, id3));
  circ_free(&circuit);
}

UTEST(Circuit, circ_remove_enitity_beginning) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id1 = circ_add(&circuit, Endpoint);
  ID id2 = circ_add(&circuit, Endpoint);
  ID id3 = circ_add(&circuit, Endpoint);

  ASSERT_EQ(circuit.endpoint.length, 3);

  circ_remove(&circuit, id1);

  ASSERT_EQ(circuit.endpoint.length, 2);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_TRUE(circ_has(&circuit, id2));
  ASSERT_TRUE(circ_has(&circuit, id3));

  circ_remove(&circuit, id2);

  ASSERT_EQ(circuit.endpoint.length, 1);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_TRUE(circ_has(&circuit, id3));

  circ_remove(&circuit, id3);

  ASSERT_EQ(circuit.endpoint.length, 0);
  ASSERT_EQ(circuit.generations[id_index(id1)], 0);
  ASSERT_EQ(circuit.generations[id_index(id2)], 0);
  ASSERT_EQ(circuit.generations[id_index(id3)], 0);
  ASSERT_FALSE(circ_has(&circuit, id1));
  ASSERT_FALSE(circ_has(&circuit, id2));
  ASSERT_FALSE(circ_has(&circuit, id3));
  circ_free(&circuit);
}

UTEST(Circuit, circ_load_symbol_descs) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  SymbolLayout layout = (SymbolLayout){
    .portSpacing = 20.0f,
    .symbolWidth = 55.0f,
    .borderWidth = 1.0f,
    .labelPadding = 2.0f,
    .user = NULL,
    .textSize = testTextSize,
  };
  circ_load_symbol_descs(&circuit, &layout, circuit_symbol_descs(), COMP_COUNT);

  // TODO: make more thorough

  ASSERT_EQ(circuit.symbolKind.length, COMP_COUNT);
  ASSERT_STREQ(
    circ_str_get(&circuit, circuit.symbolKind.name[COMP_AND]), "AND");
  ASSERT_STREQ(
    circ_str_get(&circuit, circuit.symbolKind.prefix[COMP_AND]), "X");
  ASSERT_EQ(circuit.symbolKind.shape[COMP_AND], SYMSHAPE_AND);

  int count = 0;
  PortID portID = circuit.symbolKind.ports[COMP_AND].head;
  while (circ_has(&circuit, portID)) {
    count++;
    portID = circuit.port.list[circ_row_for_id(&circuit, portID)].next;
  }
  ASSERT_EQ(count, 3);

  circ_free(&circuit);
}

UTEST(Circuit, circ_iter) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID id1 = circ_add(&circuit, Endpoint);
  ID id2 = circ_add(&circuit, Endpoint);
  circ_set(&circuit, id1, PortRef, {.symbol = 1});
  circ_set(&circuit, id2, PortRef, {.symbol = 2});
  ID id;
  size_t i = 0;
  for (CircuitIter it = circ_iter(&circuit, Endpoint); circ_iter_next(&it);) {
    Endpoint *endpoints = circ_iter_table(&it, Endpoint);

    for (size_t j = 0; j < endpoints->length; j++) {
      id = endpoints->id[j];
      if (i == 0) {
        ASSERT_EQ(id, id1);
        ASSERT_EQ(endpoints->port[j].symbol, 1);
      } else if (i == 1) {
        ASSERT_EQ(id, id2);
        ASSERT_EQ(endpoints->port[j].symbol, 2);
      }
      i++;
    }
  }
  circ_free(&circuit);
}

UTEST(Circuit, add_port) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ID portID = circ_add_port(&circuit, symbolKindID);
  ASSERT_EQ(circuit.port.length, 1);
  ASSERT_EQ(circuit.port.id[0], portID);
  ASSERT_EQ(circuit.port.symbolKind[0], symbolKindID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_port) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ID portID = circ_add_port(&circuit, symbolKindID);
  circ_remove_port(&circuit, portID);
  ASSERT_EQ(circuit.port.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_symbol_kind) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ASSERT_EQ(circuit.symbolKind.length, 1);
  ASSERT_EQ(circuit.symbolKind.id[0], symbolKindID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_symbol_kind) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  circ_add_port(&circuit, symbolKindID);
  circ_add_port(&circuit, symbolKindID);
  circ_add_port(&circuit, symbolKindID);
  circ_remove_symbol_kind(&circuit, symbolKindID);
  ASSERT_EQ(circuit.symbolKind.length, 0);
  ASSERT_EQ(circuit.port.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_symbol) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ID symbolID = circ_add_symbol(&circuit, moduleID, symbolKindID);
  ASSERT_EQ(circuit.symbol.length, 1);
  ASSERT_EQ(circuit.symbol.id[0], symbolID);
  ASSERT_EQ(circuit.symbol.module[0], moduleID);
  ASSERT_EQ(circuit.symbol.symbolKind[0], symbolKindID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_symbol) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ID symbolID = circ_add_symbol(&circuit, moduleID, symbolKindID);
  circ_remove_symbol(&circuit, symbolID);
  ASSERT_EQ(circuit.symbol.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, removing_symbol_kind_removes_all_linked_symbols) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID symbolKindID = circ_add_symbol_kind(&circuit);
  ID symbolKindID2 = circ_add_symbol_kind(&circuit);
  ID symbolID1 = circ_add_symbol(&circuit, moduleID, symbolKindID);
  ID symbolID2 = circ_add_symbol(&circuit, moduleID, symbolKindID2);
  ID symbolID3 = circ_add_symbol(&circuit, moduleID, symbolKindID);
  circ_remove_symbol_kind(&circuit, symbolKindID);
  ASSERT_EQ(circuit.symbol.length, 1);
  ASSERT_FALSE(circ_has(&circuit, symbolID1));
  ASSERT_TRUE(circ_has(&circuit, symbolID2));
  ASSERT_FALSE(circ_has(&circuit, symbolID3));
  circ_free(&circuit);
}

UTEST(Circuit, add_waypoint) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  ID endpointID = circ_add_endpoint(&circuit, subnetID);
  ID waypointID = circ_add_waypoint(&circuit, endpointID);
  ASSERT_EQ(circuit.waypoint.length, 1);
  ASSERT_EQ(circuit.waypoint.id[0], waypointID);
  ASSERT_EQ(circuit.waypoint.endpoint[0], endpointID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_waypoint) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  ID endpointID = circ_add_endpoint(&circuit, subnetID);
  ID waypointID = circ_add_waypoint(&circuit, endpointID);
  circ_remove_waypoint(&circuit, waypointID);
  ASSERT_EQ(circuit.waypoint.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_endpoint) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  ID endpointID = circ_add_endpoint(&circuit, subnetID);
  ASSERT_EQ(circuit.endpoint.length, 1);
  ASSERT_EQ(circuit.endpoint.id[0], endpointID);
  ASSERT_EQ(circuit.endpoint.subnet[0], subnetID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_endpoint) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  ID endpointID = circ_add_endpoint(&circuit, subnetID);
  circ_remove_endpoint(&circuit, endpointID);
  ASSERT_EQ(circuit.endpoint.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_subnet) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  ASSERT_EQ(circuit.subnet.length, 1);
  ASSERT_EQ(circuit.subnet.id[0], subnetID);
  ASSERT_EQ(circuit.subnet.net[0], netID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_subnet) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ID subnetID = circ_add_subnet(&circuit, netID);
  circ_remove_subnet(&circuit, subnetID);
  ASSERT_EQ(circuit.subnet.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_net) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  ASSERT_EQ(circuit.net.length, 1);
  ASSERT_EQ(circuit.net.id[0], netID);
  ID netlistID = circuit.module.nets[0];
  ASSERT_EQ(circuit.net.netlist[0], netlistID);
  circ_free(&circuit);
}

UTEST(Circuit, remove_net) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ID netID = circ_add_net(&circuit, moduleID);
  circ_remove_net(&circuit, netID);
  ASSERT_EQ(circuit.net.length, 0);
  circ_free(&circuit);
}

UTEST(Circuit, add_module) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  ASSERT_EQ(circuit.module.length, 1);
  ASSERT_EQ(circuit.netlist.length, 1);
  ASSERT_EQ(circuit.symbolKind.length, 1);

  ASSERT_EQ(circuit.module.id[0], moduleID);
  ASSERT_EQ(circuit.netlist.module[0], moduleID);
  ASSERT_EQ(circuit.symbolKind.module[0], moduleID);
  ASSERT_EQ(circuit.module.symbolKind[0], circuit.symbolKind.id[0]);
  circ_free(&circuit);
}

UTEST(Circuit, remove_module) {
  Circuit circuit;
  ErrStack errs = {0};
  circ_init(&circuit, &errs);
  ID moduleID = circ_add_module(&circuit);
  circ_remove_module(&circuit, moduleID);
  ASSERT_EQ(circuit.module.length, 0);
  ASSERT_EQ(circuit.netlist.length, 0);
  circ_free(&circuit);
}
