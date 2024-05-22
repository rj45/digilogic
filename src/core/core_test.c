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

UTEST(SparseMap, init) {
  SparseMap smap;
  smap_init(&smap, ID_COMPONENT);
  ASSERT_EQ(smap.type, ID_COMPONENT);
  smap_free(&smap);
}

UTEST(SparseMap, add_synced_array) {
  SparseMap smap;
  smap_init(&smap, ID_COMPONENT);
  int data = 0;
  void *ptr = &data;
  smap_add_synced_array(&smap, &ptr, sizeof(int));
  ASSERT_EQ(arrlen(smap.syncedArrays), 1);
  ASSERT_EQ(smap.syncedArrays[0].ptr, &ptr);
  ASSERT_EQ(smap.syncedArrays[0].elemSize, sizeof(int));
  smap_free(&smap);
}

UTEST(SparseMap, alloc) {
  SparseMap smap;
  int *data = NULL;
  smap_init(&smap, ID_COMPONENT);
  smap_add_synced_array(&smap, (void **)&data, sizeof(int));
  ID id = smap_add(&smap, &(int){1});
  ASSERT_EQ(id_index(id), 0);
  ASSERT_EQ(id_type(id), ID_COMPONENT);
  ASSERT_EQ(id_gen(id), 1);
  ASSERT_NE(smap.ids, NULL);
  ASSERT_NE(data, NULL);
  ASSERT_EQ(smap.length, 1);
  ASSERT_EQ(smap.capacity, 8);
  ASSERT_EQ(smap_index(&smap, id), 0);
  smap_free(&smap);
}

UTEST(SparseMap, alloc_multiple) {
  SparseMap smap;
  int *data = NULL;
  smap_init(&smap, ID_COMPONENT);
  smap_add_synced_array(&smap, (void **)&data, sizeof(int));
  ID id1 = smap_add(&smap, &(int){1});
  ID id2 = smap_add(&smap, &(int){1});
  ASSERT_EQ(id_index(id1), 0);
  ASSERT_EQ(id_type(id1), ID_COMPONENT);
  ASSERT_EQ(id_gen(id1), 1);
  ASSERT_EQ(id_index(id2), 1);
  ASSERT_EQ(id_type(id2), ID_COMPONENT);
  ASSERT_EQ(id_gen(id2), 1);
  ASSERT_NE(smap.ids, NULL);
  ASSERT_NE(data, NULL);
  ASSERT_EQ(smap.length, 2);
  ASSERT_EQ(smap.capacity, 8);
  ASSERT_EQ(smap_index(&smap, id1), 0);
  ASSERT_EQ(smap_index(&smap, id2), 1);
  ASSERT_EQ(smap.ids[0], id1);
  ASSERT_EQ(smap.ids[1], id2);
  smap_free(&smap);
}

UTEST(SparseMap, alloc_full) {
  SparseMap smap;
  int *data = NULL;
  smap_init(&smap, ID_COMPONENT);
  smap_add_synced_array(&smap, (void **)&data, sizeof(int));
  for (int i = 0; i < 7; i++) {
    smap_add(&smap, &(int){1});
  }
  int *oldData = data;
  ID *oldIds = smap.ids;
  smap_add(&smap, &(int){1});
  ASSERT_NE(smap.ids, oldIds);
  ASSERT_NE(data, oldData);
  ASSERT_EQ(smap.length, 8);
  ASSERT_EQ(smap.capacity, 16);
  smap_free(&smap);
}

UTEST(SparseMap, del) {
  SparseMap smap;
  int *data = NULL;
  smap_init(&smap, ID_COMPONENT);
  smap_add_synced_array(&smap, (void **)&data, sizeof(int));
  ID id1 = smap_add(&smap, &(int){1});
  ID id2 = smap_add(&smap, &(int){1});
  data[0] = 1;
  data[1] = 2;

  ASSERT_NE(id1, id2);

  smap_del(&smap, id1);

  ASSERT_EQ(data[0], 2);
  ASSERT_EQ(smap.ids[0], id2);
  ASSERT_EQ(smap_index(&smap, id2), 0);
  ASSERT_EQ(smap.length, 1);

  smap_free(&smap);
}

UTEST(SparseMap, del_last) {
  SparseMap smap;
  int *data = NULL;
  smap_init(&smap, ID_COMPONENT);
  smap_add_synced_array(&smap, (void **)&data, sizeof(int));
  ID id1 = smap_add(&smap, &(int){1});
  ID id2 = smap_add(&smap, &(int){1});
  data[0] = 1;
  data[1] = 2;

  smap_del(&smap, id2);

  ASSERT_EQ(smap.length, 1);
  ASSERT_EQ(smap.ids[0], id1);
  ASSERT_EQ(data[0], 1);

  smap_free(&smap);
}

UTEST(Circuit, add_component) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  ComponentID id = circuit_add_component(&circuit, COMP_AND, HMM_V2(0, 0));
  ASSERT_EQ(circuit_index(&circuit, id), 0);
  ASSERT_EQ(circuit_component_len(&circuit), 1);
  ASSERT_EQ(circuit_component_ptr(&circuit, id)->desc, COMP_AND);
  ASSERT_NE(circuit_component_ptr(&circuit, id)->portFirst, NO_PORT);
  ASSERT_EQ(circuit_port_len(&circuit), 3);
  circuit_free(&circuit);
}

UTEST(Circuit, add_net_no_ports) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  NetID id = circuit_add_net(&circuit);
  ASSERT_EQ(circuit_index(&circuit, id), 0);
  ASSERT_EQ(circuit_net_len(&circuit), 1);
  circuit_free(&circuit);
}

UTEST(Circuit, add_net_with_ports) {
  Circuit circuit;
  circuit_init(&circuit, circuit_component_descs());
  ComponentID compID = circuit_add_component(&circuit, COMP_AND, HMM_V2(0, 0));
  Component *comp = circuit_component_ptr(&circuit, compID);
  PortID portID1 = comp->portFirst;
  PortID portID2 = circuit_port_ptr(&circuit, portID1)->next;
  NetID net = circuit_add_net(&circuit);
  EndpointID epID1 = circuit_add_endpoint(&circuit, net, portID1, HMM_V2(0, 0));
  EndpointID epID2 = circuit_add_endpoint(&circuit, net, portID2, HMM_V2(0, 0));

  ASSERT_EQ(circuit_index(&circuit, net), 0);
  ASSERT_EQ(circuit_net_len(&circuit), 1);
  ASSERT_EQ(circuit_endpoint_len(&circuit), 2);
  ASSERT_EQ(circuit_endpoint_ptr(&circuit, epID1)->port, portID1);
  ASSERT_EQ(circuit_endpoint_ptr(&circuit, epID2)->port, portID2);
  ASSERT_EQ(circuit_endpoint_ptr(&circuit, epID1)->net, net);
  ASSERT_EQ(circuit_endpoint_ptr(&circuit, epID2)->net, net);
  // todo add more checks for the linked lists
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