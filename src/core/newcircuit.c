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
#include "core/newstructs.h"

#include <assert.h>
#include <stdbool.h>

// todo:
// - iterators and a way to let code using iterators be
//   agnostic to whether COW is used or paged memory
// - mark a row as deleted but don't actually remove it
//   until later. Let iterators skip such rows, and circ_has return false,
//   but yet allow event queue iterators to see the memory for destructors to
//   run

const size_t componentSizes[COMPONENT_COUNT] = {COMPONENT_SIZES_LIST};

void circ_init(Circuit2 *circ) {
  *circ = (Circuit2){0};

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

static void circ_add_entity_impl(Circuit2 *circ, EntityType type, ID id) {
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

void circ_add_entity_id(Circuit2 *circ, EntityType type, ID id) {
  assert(id_gen(id) > 0);
  circ_grow_entities(circ, id_index(id) + 1);
  assert(circ->generations[id_index(id)] == 0); // id must be unique

  for (size_t i = 0; i < arrlen(circ->freelist); i++) {
    if (id_index(circ->freelist[i]) == id_index(id)) {
      arrdel(circ->freelist, i);
      break;
    }
  }

  circ_add_entity_impl(circ, type, id);
}

ID circ_add_entity(Circuit2 *circ, EntityType type) {
  circ_grow_entities(circ, circ->numEntities + 1);
  ID id = arrpop(circ->freelist);
  circ_add_entity_impl(circ, type, id);
  return id;
}

void circ_remove_entity(Circuit2 *circ, ID id) {
  assert(circ_has(circ, id));
  circ->generations[id_index(id)] = 0;
  circ->typeTags[id_index(id)] = 0;
  circ->rows[id_index(id)] = 0;
  int gen = (id_gen(id) + 1) & ID_GEN_MASK;
  if (gen == 0) {
    gen = 1;
  }
  arrput(circ->freelist, id_make(0, gen, id_index(id)));
  circ->numEntities--;
}
