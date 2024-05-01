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

UTEST(SparseMap, grow) {
  smap(int) arr = NULL;
  smap_grow(arr, 10, 0);
  ASSERT_EQ(smap_cap(arr), 16);
  ASSERT_EQ(smap_len(arr), 0);
  smap_free(arr);
}

UTEST(SparseMap, grow_twice) {
  smap(int) arr = NULL;
  smap_grow(arr, 10, 0);
  smap_grow(arr, 20, 0);
  ASSERT_EQ(smap_cap(arr), 32);
  ASSERT_EQ(smap_len(arr), 0);
  smap_free(arr);
}

UTEST(SparseMap, free) {
  smap(int) arr = NULL;
  smap_grow(arr, 10, 0);
  ASSERT_NE(arr, NULL);
  smap_free(arr);
  ASSERT_EQ(arr, NULL);
}

UTEST(SparseMap, maybe_grow) {
  smap(int) arr = NULL;
  smap_maybe_grow(arr, 1);
  ASSERT_EQ(smap_cap(arr), 8);
  smap_free(arr);
}

UTEST(SparseMap, alloc) {
  smap(int) arr = NULL;
  ID id = smap_alloc(arr, ID_COMPONENT);

  ASSERT_NE(arr, NULL);
  ASSERT_TRUE(id_valid(id));
  ASSERT_TRUE(smap_has(arr, id));
  ASSERT_EQ(smap_len(arr), 1);
  ASSERT_EQ(smap_index(arr, id), 0);

  ASSERT_EQ(id_type(id), ID_COMPONENT);
  ASSERT_EQ(id_gen(id), 1);
  ASSERT_EQ(id_index(id), 0);

  smap_free(arr);
}

UTEST(SparseMap, put) {
  smap(int) arr = NULL;
  ID id = smap_put(arr, ID_COMPONENT, -2);

  ASSERT_TRUE(id_valid(id));
  ASSERT_TRUE(smap_has(arr, id));

  ASSERT_EQ(smap_len(arr), 1);
  ASSERT_EQ(arr[smap_index(arr, id)], -2);

  ASSERT_EQ(smap_ids(arr)[0], id);

  smap_free(arr);
}

UTEST(SparseMap, put_twice) {
  smap(int) arr = NULL;
  ID id1 = smap_put(arr, ID_COMPONENT, -2);
  ID id2 = smap_put(arr, ID_COMPONENT, -3);

  ASSERT_TRUE(id_valid(id1));
  ASSERT_TRUE(id_valid(id2));
  ASSERT_TRUE(smap_has(arr, id1));
  ASSERT_TRUE(smap_has(arr, id2));
  ASSERT_NE(id1, id2);

  ASSERT_EQ(smap_ids(arr)[0], id1);
  ASSERT_EQ(smap_ids(arr)[1], id2);

  ASSERT_EQ(smap_len(arr), 2);
  ASSERT_EQ(arr[smap_index(arr, id1)], -2);
  ASSERT_EQ(arr[smap_index(arr, id2)], -3);

  smap_free(arr);
}

UTEST(SparseMap, del) {
  smap(int) arr = NULL;
  ID id = smap_put(arr, ID_COMPONENT, -2);
  smap_del(arr, id);

  ASSERT_EQ(smap_len(arr), 0);
  ASSERT_FALSE(smap_has(arr, id));
  ASSERT_EQ(arrlen(smap_header(arr)->freeList), 1);
  ASSERT_EQ(smap_header(arr)->freeList[0], id);

  smap_free(arr);
}

UTEST(SparseMap, del_first_of_two) {
  smap(int) arr = NULL;
  ID id1 = smap_put(arr, ID_COMPONENT, -2);
  ID id2 = smap_put(arr, ID_COMPONENT, -3);
  smap_del(arr, id1);

  ASSERT_EQ(smap_len(arr), 1);
  ASSERT_FALSE(smap_has(arr, id1));
  ASSERT_TRUE(smap_has(arr, id2));
  ASSERT_EQ(smap_index(arr, id2), 0);
  ASSERT_EQ(arr[smap_index(arr, id2)], -3);
  ASSERT_EQ(arrlen(smap_header(arr)->freeList), 1);
  ASSERT_EQ(smap_header(arr)->freeList[0], id1);

  ASSERT_EQ(smap_ids(arr)[0], id2);

  smap_free(arr);
}

UTEST(SparseMap, del_second_of_two) {
  smap(int) arr = NULL;
  ID id1 = smap_put(arr, ID_COMPONENT, -2);
  ID id2 = smap_put(arr, ID_COMPONENT, -3);
  smap_del(arr, id2);

  ASSERT_EQ(smap_len(arr), 1);
  ASSERT_TRUE(smap_has(arr, id1));
  ASSERT_FALSE(smap_has(arr, id2));
  ASSERT_EQ(smap_index(arr, id1), 0);
  ASSERT_EQ(arr[smap_index(arr, id1)], -2);
  ASSERT_EQ(arrlen(smap_header(arr)->freeList), 1);
  ASSERT_EQ(smap_header(arr)->freeList[0], id2);

  ASSERT_EQ(smap_ids(arr)[0], id1);

  smap_free(arr);
}

UTEST(SparseMap, put_delled) {
  smap(int) arr = NULL;
  ID id1 = smap_put(arr, ID_COMPONENT, -2);
  ID id2 = smap_put(arr, ID_COMPONENT, -3);
  smap_del(arr, id1);
  ID id3 = smap_put(arr, ID_COMPONENT, -4);

  ASSERT_EQ(smap_len(arr), 2);
  ASSERT_FALSE(smap_has(arr, id1));
  ASSERT_TRUE(smap_has(arr, id2));
  ASSERT_TRUE(smap_has(arr, id3));
  ASSERT_EQ(smap_index(arr, id2), 0);
  ASSERT_EQ(smap_index(arr, id3), 1);

  ASSERT_EQ(arr[smap_index(arr, id2)], -3);
  ASSERT_EQ(arr[smap_index(arr, id3)], -4);

  ASSERT_EQ(arrlen(smap_header(arr)->freeList), 0);

  ASSERT_EQ(id_index(id1), id_index(id3));
  ASSERT_EQ(id_gen(id1), 1);
  ASSERT_EQ(id_gen(id3), 2);

  ASSERT_EQ(smap_ids(arr)[0], id2);
  ASSERT_EQ(smap_ids(arr)[1], id3);

  smap_free(arr);
}

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
  NetID id = circuit_add_net(&circuit);
  ASSERT_EQ(id, 0);
  ASSERT_EQ(arrlen(circuit.nets), 1);
  circuit_free(&circuit);
}

UTEST(Circuit, add_net_with_ports) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  ComponentID compID = circuit_add_component(&circuit, COMP_AND);
  PortID portID = circuit.components[compID].portStart;
  NetID net = circuit_add_net(&circuit);
  WireID id = circuit_add_wire(
    &circuit, net, wire_end_make(WIRE_END_PORT, portID),
    wire_end_make(WIRE_END_PORT, portID + 1));
  ASSERT_EQ(net, 0);
  ASSERT_EQ(arrlen(circuit.nets), 1);
  ASSERT_EQ(circuit.wires[id].from, wire_end_make(WIRE_END_PORT, portID));
  ASSERT_EQ(circuit.ports[portID].net, net);
  ASSERT_EQ(circuit.wires[id].to, wire_end_make(WIRE_END_PORT, portID + 1));
  ASSERT_EQ(circuit.ports[portID + 1].net, net);
  ASSERT_EQ(circuit.wires[id].next, NO_WIRE);
  ASSERT_EQ(circuit.wires[id].prev, NO_WIRE);
  ASSERT_EQ(circuit.nets[net].wireFirst, id);
  ASSERT_EQ(circuit.nets[net].wireLast, id);
  circuit_free(&circuit);
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