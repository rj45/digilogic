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
#include "core/structs.h"
#include "handmade_math.h"

#include <assert.h>
#include <stdbool.h>

#define LOG_LEVEL LL_DEBUG
#include "log.h"

// todo:
// - component destructors
// - finish integrating componentIDs to allow dynamic tables

const size_t componentSizes[COMPONENT_COUNT] = {COMPONENT_SIZES_LIST};

void circ_init(Circuit2 *circ) {
  *circ = (Circuit2){0};

  strpool_init(
    &circ->strpool, &(strpool_config_t){
                      .counter_bits = 8,
                      .index_bits = 24,
                      .entry_capacity = 128,
                      .block_size = 4096,
                      .min_length = 8,
                    });

  circ->numTables = TYPE_COUNT;
  circ->table = malloc(TYPE_COUNT * sizeof(Table *));
  circ->tableMeta = malloc(TYPE_COUNT * sizeof(TableMeta));

  // get pointers to each table
  circ->table[TYPE_PORT] = (Table *)&circ->port;
  circ->table[TYPE_SYMBOL_KIND] = (Table *)&circ->symbolKind;
  circ->table[TYPE_SYMBOL] = (Table *)&circ->symbol;
  circ->table[TYPE_WAYPOINT] = (Table *)&circ->waypoint;
  circ->table[TYPE_ENDPOINT] = (Table *)&circ->endpoint;
  circ->table[TYPE_SUBNET_BIT] = (Table *)&circ->subnetBit;
  circ->table[TYPE_SUBNET_BITS] = (Table *)&circ->subnetBits;
  circ->table[TYPE_SUBNET] = (Table *)&circ->subnet;
  circ->table[TYPE_NET] = (Table *)&circ->net;
  circ->table[TYPE_NETLIST] = (Table *)&circ->netlist;
  circ->table[TYPE_MODULE] = (Table *)&circ->module;

  memcpy(
    circ->tableMeta, (TableMeta[]){STANDARD_TABLE_LIST},
    TYPE_COUNT * sizeof(TableMeta));
  for (size_t type = 0; type < TYPE_COUNT; type++) {
    for (size_t componentID = 0; componentID < COMPONENT_COUNT; componentID++) {
      if (circ->tableMeta[type].components & (1 << componentID)) {
        int count = circ->tableMeta[type].componentCount;
        circ->tableMeta[type].componentSizes[count] =
          componentSizes[componentID];
        circ->tableMeta[type].componentIndices[componentID] = count;
        circ->tableMeta[type].componentCount++;
      }
    }
  }
}

void circ_free(Circuit2 *circ) {
  for (size_t i = 0; i < circ->numTables; i++) {
    if (circ->table[i]->capacity > 0) {
      free(circ->table[i]->id);
      TableMeta *meta = &circ->tableMeta[i];
      for (size_t j = 0; j < meta->componentCount; j++) {
        free(circ_table_components_ptr(circ, i, j));
      }
    }
  }
  free(circ->table);
  free(circ->tableMeta);
  if (circ->capacity > 0) {
    free(circ->generations);
    free(circ->typeTags);
    free(circ->rows);
  }
  arrfree(circ->freelist);
  strpool_term(&circ->strpool);
}

void circ_load_symbol_descs(
  Circuit2 *circ, const ComponentDesc *descs, size_t count) {
  for (size_t i = 0; i < count; i++) {
    const ComponentDesc *symDesc = &descs[i];
    ID symID = circ_add(circ, SymbolKind2);
    int compRow = circ_row_for_id(circ, symID);
    circ->symbolKind.name[compRow] = circ_str_c(circ, symDesc->typeName);
    circ->symbolKind.prefix[compRow] =
      circ_str(circ, (char[]){symDesc->namePrefix}, 1);
    circ->symbolKind.shape[compRow] = symDesc->shape;

    for (size_t j = 0; j < symDesc->numPorts; j++) {
      PortDesc portDesc = symDesc->ports[j];
      ID portID = circ_add(circ, Port2);
      int portRow = circ_row_for_id(circ, portID);
      circ->port.symbolKind[portRow] = symID;
      circ->port.name[portRow] = circ_str_c(circ, portDesc.name);
      if (portDesc.direction == PORT_IN || portDesc.direction == PORT_INOUT) {
        circ_add_tags(circ, portID, TAG_IN);
      }
      if (portDesc.direction == PORT_OUT || portDesc.direction == PORT_INOUT) {
        circ_add_tags(circ, portID, TAG_OUT);
      }
      circ->port.pin[portRow] = portDesc.number;

      circ_linked_list_append(circ, symID, portID);
    }
  }
}

static void circ_grow_entities(Circuit2 *circ, size_t newLength) {
  ptrdiff_t newCapacity = circ->capacity;
  if (newCapacity == 0) {
    newCapacity = 1;
  }
  while (newCapacity < newLength) {
    newCapacity *= 2;
  }

  if (newCapacity == circ->capacity) {
    return;
  }

  circ->generations = realloc(circ->generations, newCapacity * sizeof(uint8_t));
  circ->typeTags = realloc(circ->typeTags, newCapacity * sizeof(uint16_t));
  circ->rows = realloc(circ->rows, newCapacity * sizeof(uint32_t));

  // zero out the new entries and add them to the freelist
  for (ptrdiff_t i = newCapacity - 1; i >= (ptrdiff_t)circ->capacity; i--) {
    circ->generations[i] = 0;
    circ->typeTags[i] = 0;
    circ->rows[i] = 0;
    arrput(circ->freelist, id_make(0, 1, i));
  }

  circ->capacity = newCapacity;
}

static void circ_grow_table(Circuit2 *circ, EntityType type, size_t newLength) {
  Table *header = circ->table[type];
  size_t newCapacity = header->capacity;
  if (newCapacity == 0) {
    newCapacity = 1;
  }
  while (newCapacity < newLength) {
    newCapacity *= 2;
  }

  if (newCapacity == header->capacity) {
    return;
  }

  header->id = realloc(header->id, newCapacity * sizeof(ID));

  TableMeta *meta = &circ->tableMeta[type];
  for (size_t i = 0; i < meta->componentCount; i++) {
    void **ptr = circ_table_components_ptr_ptr(circ, type, i);
    *ptr = realloc(*(void **)ptr, newCapacity * meta->componentSizes[i]);
  }

  header->capacity = newCapacity;
}

static void circ_add_impl(Circuit2 *circ, EntityType type, ID id) {
  Table *header = circ->table[type];

  int index = id_index(id);
  int row = header->length;
  circ->generations[index] = id_gen(id);
  circ->typeTags[index] = type;
  circ->rows[index] = row;
  circ->numEntities++;

  // grow the table if necessary
  circ_grow_table(circ, type, row + 1);
  header->length++;

  TableMeta *meta = &circ->tableMeta[type];
  for (size_t i = 1; i < meta->componentCount; i++) {
    memset(
      circ_table_component_ptr(circ, type, i, row), 0, meta->componentSizes[i]);
  }

  header->id[row] = id;
}

void circ_add_type_id(Circuit2 *circ, EntityType type, ID id) {
  assert(id_gen(id) > 0);
  circ_grow_entities(circ, id_index(id) + 1);
  assert(circ->generations[id_index(id)] == 0); // id must be unique

  for (size_t i = 0; i < arrlen(circ->freelist); i++) {
    if (id_index(circ->freelist[i]) == id_index(id)) {
      arrdel(circ->freelist, i);
      break;
    }
  }

  circ_add_impl(circ, type, id);
}

ID circ_add_type(Circuit2 *circ, EntityType type) {
  circ_grow_entities(circ, circ->numEntities + 1);
  ID id = arrpop(circ->freelist);
  circ_add_impl(circ, type, id);
  return id;
}

void circ_remove(Circuit2 *circ, ID id) {
  assert(circ_has(circ, id));

  // remove the entity from table
  EntityType type = tagtype_type(circ->typeTags[id_index(id)]);
  Table *table = circ->table[type];
  int row = circ->rows[id_index(id)];
  int lastRow = table->length - 1;
  if (row != lastRow) {
    ID lastID = table->id[lastRow];
    for (size_t i = 0; i < circ->tableMeta[type].componentCount; i++) {
      void *src = circ_table_component_ptr(circ, type, i, lastRow);
      void *dst = circ_table_component_ptr(circ, type, i, row);
      memcpy(dst, src, circ->tableMeta[type].componentSizes[i]);
    }
    table->id[row] = lastID;
    circ->rows[id_index(lastID)] = row;
  }
  table->length--;

  circ->rows[id_index(id)] = 0;
  circ->generations[id_index(id)] = 0;
  circ->typeTags[id_index(id)] = 0;
  int gen = (id_gen(id) + 1) & ID_GEN_MASK;
  if (gen == 0) {
    gen = 1;
  }
  arrput(circ->freelist, id_make(0, gen, id_index(id)));
  circ->numEntities--;
}

void circ_add_tags(Circuit2 *circ, ID id, Tag tags) {
  assert(circ_has(circ, id));
  circ->typeTags[id_index(id)] |= tags;
}

StringHandle circ_str(Circuit2 *circ, const char *str, size_t len) {
  StringHandle handle = (StringHandle)strpool_inject(&circ->strpool, str, len);
  strpool_incref(&circ->strpool, handle);
  return handle;
}

StringHandle circ_str_c(Circuit2 *circ, const char *str) {
  return circ_str(circ, str, strlen(str));
}

void circ_str_free(Circuit2 *circ, StringHandle handle) {
  int count = strpool_decref(&circ->strpool, handle);
  if (count == 0) {
    strpool_discard(&circ->strpool, handle);
  }
}

const char *circ_str_get(Circuit2 *circ, StringHandle handle) {
  return strpool_cstr(&circ->strpool, handle);
}

void circ_linked_list_append(Circuit2 *circ, ID parent, ID child) {
  assert(circ_has(circ, parent));
  assert(circ_has(circ, child));

  LinkedList list = circ_get(circ, parent, LinkedList);
  ListNode node = circ_get(circ, child, ListNode);
  node.prev = list.tail;
  list.tail = child;
  if (!circ_has(circ, node.prev)) {
    list.head = child;
  } else {
    ListNode prev = circ_get(circ, node.prev, ListNode);
    prev.next = child;
    circ_set_ptr(circ, node.prev, ListNode, &prev);
  }
  circ_set_ptr(circ, child, ListNode, &node);
  circ_set_ptr(circ, parent, LinkedList, &list);
}

void circ_linked_list_remove(Circuit2 *circ, ID parent, ID child) {
  assert(circ_has(circ, parent));
  assert(circ_has(circ, child));

  LinkedList list = circ_get(circ, parent, LinkedList);
  bool parentChanged = false;
  ListNode node = circ_get(circ, child, ListNode);
  if (!circ_has(circ, node.prev)) {
    list.head = node.next;
    parentChanged = true;
  } else {
    ListNode prev = circ_get(circ, node.prev, ListNode);
    prev.next = node.next;
    circ_set_ptr(circ, node.prev, ListNode, &prev);
  }
  if (!circ_has(circ, node.next)) {
    ID newTail = node.prev;
    if (!circ_has(circ, newTail)) {
      newTail = list.head;
    }
    list.tail = newTail;
    parentChanged = true;
  } else {
    ListNode next = circ_get(circ, node.next, ListNode);
    next.prev = node.prev;
    circ_set_ptr(circ, node.next, ListNode, &next);
  }
  if (parentChanged) {
    circ_set_ptr(circ, parent, LinkedList, &list);
  }
}

typedef struct LinkedListIter {
  Circuit2 *circ;
  ID current;
  ID next;
} LinkedListIter;

LinkedListIter circ_lliter(Circuit2 *circ, ID parent) {
  LinkedList list = circ_get(circ, parent, LinkedList);
  return (LinkedListIter){circ, .next = list.head};
}

bool circ_lliter_next(LinkedListIter *iter) {
  if (!circ_has(iter->circ, iter->next)) {
    return false;
  }
  ListNode node = circ_get(iter->circ, iter->next, ListNode);
  iter->current = iter->next;
  iter->next = node.next;
  return true;
}

ID circ_lliter_get(LinkedListIter *iter) { return iter->current; }

// ---

ID circ_add_port(Circuit2 *circ, ID symbolKind) {
  ID portID = circ_add(circ, Port2);
  circ_set(circ, portID, Parent, {symbolKind});
  circ_linked_list_append(circ, symbolKind, portID);
  return portID;
}

void circ_remove_port(Circuit2 *circ, ID id) {
  Parent symbolKind = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, symbolKind, id);
  circ_str_free(circ, circ_get(circ, id, Name));
  circ_remove(circ, id);
}

// ---

ID circ_add_symbol_kind(Circuit2 *circ) {
  ID symbolKindID = circ_add(circ, SymbolKind2);
  return symbolKindID;
}

void circ_remove_symbol_kind(Circuit2 *circ, ID id) {
  const LinkedList *ll = circ_get_ptr(circ, id, LinkedList);
  while (circ_has(circ, ll->head)) {
    circ_remove_port(circ, ll->head);
  }

  // todo: make this faster?
  CircuitIter it = circ_iter(circ, Symbol2);
  while (circ_iter_next(&it)) {
    Symbol2 *symbols = circ_iter_table(&it, Symbol2);
    for (size_t i = 0; i < symbols->length; i++) {
      if (symbols->symbolKind[i] == id) {
        circ_remove_symbol(circ, symbols->id[i]);
        i--;
      }
    }
  }

  circ_str_free(circ, circ_get(circ, id, Name));
  circ_str_free(circ, circ_get(circ, id, Prefix));
  circ_remove(circ, id);
}

// ---

ID circ_add_symbol(Circuit2 *circ, ID module, ID symbolKind) {
  ID symbolID = circ_add(circ, Symbol2);
  circ_set(circ, symbolID, Parent, {module});
  circ_set(circ, symbolID, SymbolKindID, {symbolKind});
  circ_linked_list_append(circ, module, symbolID);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ComponentDescID desc = 0;
    const char *name = circ_str_get(circ, circ_get(circ, symbolKind, Name));
    for (size_t i = 0; i < COMP_COUNT; i++) {
      if (strcmp(name, circ->oldCircuit->componentDescs[i].typeName) == 0) {
        desc = i;
        break;
      }
    }
    ComponentID compID =
      circuit_add_component(circ->oldCircuit, desc, HMM_V2(0, 0));
    hmput(circ->oldToNew, compID, symbolID);
    hmput(circ->newToOld, symbolID, compID);
  }
  return symbolID;
}

void circ_remove_symbol(Circuit2 *circ, ID id) {
  Parent module = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, module, id);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ComponentID compID = hmget(circ->newToOld, id);
    circuit_del(circ->oldCircuit, compID);

    hmdel(circ->oldToNew, compID);
    hmdel(circ->newToOld, id);
  }
  circ_remove(circ, id);
}

void circ_set_symbol_position(Circuit2 *circ, ID id, HMM_Vec2 position) {
  circ_set_ptr(circ, id, Position, &position);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ComponentID compID = hmget(circ->newToOld, id);
    circuit_move_component_to(circ->oldCircuit, compID, position);
  }
}

// ---

ID circ_add_waypoint(Circuit2 *circ, ID endpoint) {
  ID waypointID = circ_add(circ, Waypoint2);
  circ_set(circ, waypointID, Parent, {endpoint});
  circ_linked_list_append(circ, endpoint, waypointID);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ID subnetID = circ_get(circ, endpoint, Parent);
    ID netID = circ_get(circ, subnetID, Parent);
    NetID oldNetID = hmget(circ->newToOld, netID);

    WaypointID oldWaypoint =
      circuit_add_waypoint(circ->oldCircuit, oldNetID, HMM_V2(0, 0));

    hmput(circ->oldToNew, oldWaypoint, waypointID);
    hmput(circ->newToOld, waypointID, oldWaypoint);
  }
  return waypointID;
}

void circ_remove_waypoint(Circuit2 *circ, ID id) {
  Parent endpoint = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, endpoint, id);
  circ_remove(circ, id);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    WaypointID waypointID = hmget(circ->newToOld, id);
    circuit_del(circ->oldCircuit, waypointID);

    hmdel(circ->oldToNew, waypointID);
    hmdel(circ->newToOld, id);
  }
}

void circ_set_waypoint_position(Circuit2 *circ, ID id, HMM_Vec2 position) {
  circ_set_ptr(circ, id, Position, &position);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    WaypointID waypointID = hmget(circ->newToOld, id);
    HMM_Vec2 oldPosition =
      circ->oldCircuit->waypoints[circuit_index(circ->oldCircuit, waypointID)]
        .position;
    circuit_move_waypoint(
      circ->oldCircuit, waypointID, HMM_Sub(oldPosition, position));
  }
}

// ---

ID circ_add_endpoint(Circuit2 *circ, ID subnet) {
  ID endpointID = circ_add(circ, Endpoint2);
  circ_set(circ, endpointID, Parent, {subnet});
  circ_linked_list_append(circ, subnet, endpointID);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ID netID = circ_get(circ, subnet, Parent);
    NetID oldNetID = hmget(circ->newToOld, netID);

    EndpointID oldEndpoint =
      circuit_add_endpoint(circ->oldCircuit, oldNetID, NO_ID, HMM_V2(0, 0));

    hmput(circ->oldToNew, oldEndpoint, endpointID);
    hmput(circ->newToOld, endpointID, oldEndpoint);
  }
  return endpointID;
}

void circ_remove_endpoint(Circuit2 *circ, ID id) {
  Parent subnet = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnet, id);
  circ_remove(circ, id);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    EndpointID endpointID = hmget(circ->newToOld, id);
    circuit_del(circ->oldCircuit, endpointID);

    hmdel(circ->oldToNew, endpointID);
    hmdel(circ->newToOld, id);
  }
}

void circ_set_endpoint_position(Circuit2 *circ, ID id, HMM_Vec2 position) {
  circ_set_ptr(circ, id, Position, &position);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    EndpointID endpointID = hmget(circ->newToOld, id);
    circuit_move_endpoint_to(circ->oldCircuit, endpointID, position);
  }
}

void circ_connect_endpoint_to_port(
  Circuit2 *circ, ID endpointID, ID symbolID, ID portID) {
  assert(circ_has(circ, endpointID));
  assert(circ_has(circ, symbolID));
  assert(circ_has(circ, portID));

  PortRef ref = (PortRef){.symbol = symbolID, .port = portID};
  circ_set_ptr(circ, endpointID, PortRef, &ref);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    ComponentID oldCompID = hmget(circ->newToOld, symbolID);
    const char *portName = circ_str_get(circ, circ_get(circ, portID, Name));
    Component *oldComp = circuit_component_ptr(circ->oldCircuit, oldCompID);
    PortID portID = oldComp->portFirst;
    while (circuit_has(circ->oldCircuit, portID)) {
      Port *port = circuit_port_ptr(circ->oldCircuit, portID);
      if (
        strcmp(
          circ->oldCircuit->componentDescs[oldComp->desc]
            .ports[port->desc]
            .name,
          portName) == 0) {
        break;
      }
      portID = port->next;
    }
    assert(circuit_has(circ->oldCircuit, portID));
    EndpointID oldEndpointID = hmget(circ->newToOld, endpointID);
    circuit_endpoint_connect(circ->oldCircuit, oldEndpointID, portID);
  }
}

// ---

ID circ_add_subnet_bit(Circuit2 *circ, ID subnetBits) {
  ID subnetBitID = circ_add(circ, SubnetBit2);
  circ_set(circ, subnetBitID, Parent, {subnetBits});
  circ_linked_list_append(circ, subnetBits, subnetBitID);
  return subnetBitID;
}

void circ_remove_subnet_bit(Circuit2 *circ, ID id) {
  Parent subnetBits = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnetBits, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_subnet_bits(Circuit2 *circ, ID subnet) {
  ID subnetBitsID = circ_add(circ, SubnetBits2);
  circ_set(circ, subnetBitsID, Parent, {subnet});
  circ_linked_list_append(circ, subnet, subnetBitsID);
  return subnetBitsID;
}

void circ_remove_subnet_bits(Circuit2 *circ, ID id) {
  Parent subnet = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnet, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_subnet(Circuit2 *circ, ID net) {
  ID subnetID = circ_add(circ, Subnet2);
  circ_set(circ, subnetID, Parent, {net});
  circ_linked_list_append(circ, net, subnetID);
  return subnetID;
}

void circ_remove_subnet(Circuit2 *circ, ID id) {
  Parent net = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, net, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_net(Circuit2 *circ, ID module) {
  ID netID = circ_add(circ, Net2);
  ID netlistID = circ_get(circ, module, NetlistID);
  circ_set(circ, netID, Parent, {netlistID});
  circ_linked_list_append(circ, netlistID, netID);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    NetID oldNetID = circuit_add_net(circ->oldCircuit);

    hmput(circ->oldToNew, oldNetID, netID);
    hmput(circ->newToOld, netID, oldNetID);
  }
  return netID;
}

void circ_remove_net(Circuit2 *circ, ID id) {
  Parent module = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, module, id);
  circ_remove(circ, id);
  if (circ->oldCircuit) {
    // todo: remove this when transition is over
    EndpointID netID = hmget(circ->newToOld, id);
    circuit_del(circ->oldCircuit, netID);

    hmdel(circ->oldToNew, netID);
    hmdel(circ->newToOld, id);
  }
}

// ---

ID circ_add_module(Circuit2 *circ) {
  ID moduleID = circ_add(circ, Module2);
  ID netlistID = circ_add(circ, Netlist2);
  circ_set(circ, moduleID, NetlistID, {netlistID});
  circ_set(circ, netlistID, Parent, {moduleID});
  ID symbolKindID = circ_add_symbol_kind(circ);
  circ_set(circ, moduleID, SymbolKindID, {symbolKindID});
  circ_set(circ, symbolKindID, ModuleID, {moduleID});
  return moduleID;
}

void circ_remove_module(Circuit2 *circ, ID id) {
  LinkedListIter it = circ_lliter(circ, id);
  while (circ_lliter_next(&it)) {
    ID symbolID = circ_lliter_get(&it);
    circ_remove_symbol(circ, symbolID);
  }
  ID netlistID = circ_get(circ, id, NetlistID);
  it = circ_lliter(circ, netlistID);
  while (circ_lliter_next(&it)) {
    ID netID = circ_lliter_get(&it);
    circ_remove_net(circ, netID);
  }
  circ_remove(circ, netlistID);
  circ_remove_symbol_kind(circ, circ_get(circ, id, SymbolKindID));
  circ_str_free(circ, circ_get(circ, id, Name));
  circ_remove(circ, id);
}
