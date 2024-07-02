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
#include "handmade_math.h"
#include "strpool.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define LOG_LEVEL LL_DEBUG
#include "log.h"

// TODO:
// - component destructors
// - finish integrating componentIDs to allow dynamic tables
// - add inverted indices and remove linked lists
// - simplify the iterator API
// - replace circuit_component_descs with a prefab system

const SymbolDesc *circuit_symbol_descs() {
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

  static PortDesc xorPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_IN, .name = "B"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc notPorts[] = {
    {.direction = PORT_IN, .name = "A"},
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc inputPorts[] = {
    {.direction = PORT_OUT, .name = "Y"},
  };

  static PortDesc outputPorts[] = {
    {.direction = PORT_IN, .name = "A"},
  };

  static const SymbolDesc descs[] = {
    [COMP_AND] =
      {
        .typeName = "AND",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_AND,
        .ports = andPorts,
      },
    [COMP_OR] =
      {
        .typeName = "OR",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_OR,
        .ports = orPorts,
      },
    [COMP_XOR] =
      {
        .typeName = "XOR",
        .numPorts = 3,
        .namePrefix = 'X',
        .shape = SHAPE_XOR,
        .ports = xorPorts,
      },
    [COMP_NOT] =
      {
        .typeName = "NOT",
        .numPorts = 2,
        .namePrefix = 'X',
        .shape = SHAPE_NOT,
        .ports = notPorts,
      },
    [COMP_INPUT] =
      {
        .typeName = "IN",
        .numPorts = 1,
        .namePrefix = 'I',
        .ports = inputPorts,
      },
    [COMP_OUTPUT] =
      {
        .typeName = "OUT",
        .numPorts = 1,
        .namePrefix = 'O',
        .ports = outputPorts,
      },
  };
  return descs;
}

const size_t componentSizes[COMPONENT_COUNT] = {COMPONENT_SIZES_LIST};

void circ_init(Circuit *circ) {
  *circ = (Circuit){0};

  strpool_init(
    &circ->strpool, &(strpool_config_t){
                      .counter_bits = 8,
                      .index_bits = 24,
                      .entry_capacity = 128,
                      .block_size = 4096,
                      .min_length = 8,
                    });

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
        circ->tableMeta[type].componentSize[count] =
          componentSizes[componentID];
        circ->tableMeta[type].componentColumn[componentID] = count;
        circ->tableMeta[type].componentCount++;
      }
    }
  }
}

void circ_free(Circuit *circ) {
  for (size_t i = 0; i < TYPE_COUNT; i++) {
    if (circ->table[i]->capacity > 0) {
      free(circ->table[i]->id);
      TableMeta *meta = &circ->tableMeta[i];
      for (size_t j = 0; j < meta->componentCount; j++) {
        free(circ_table_components_ptr(circ, i, j));
      }
    }
  }
  if (circ->capacity > 0) {
    free(circ->generations);
    free(circ->typeTags);
    free(circ->rows);
  }
  arrfree(circ->freelist);
  if (!circ->foreignStrpool) {
    strpool_term(&circ->strpool);
  }
}

void circ_layout_symbol_kind(
  Circuit *circ, SymbolLayout *layout, ID symbolKindID) {
  float labelPadding = layout->labelPadding;

  float width = layout->symbolWidth;

  const char *name = circ_str_get(circ, circ_get(circ, symbolKindID, Name));

  HMM_Vec2 labelSize = layout->textSize(layout->user, name);
  if (labelSize.X + labelPadding * 2 > width) {
    width = labelSize.X + labelPadding * 2;
  }

  int numInputPorts = 0;
  int numOutputPorts = 0;
  LinkedListIter it = circ_lliter(circ, symbolKindID);
  while (circ_lliter_next(&it)) {
    ID portID = it.current;

    if (circ_has_tags(circ, portID, TAG_IN)) {
      numInputPorts++;
    } else {
      numOutputPorts++;
    }
    const char *portName = circ_str_get(circ, circ_get(circ, portID, Name));
    labelSize = layout->textSize(layout->user, portName);
    float desiredHalfWidth = labelSize.X * 0.5f + labelPadding * 3;
    if (desiredHalfWidth > width / 2) {
      width = desiredHalfWidth * 2;
    }
  }

  float height = fmaxf(numInputPorts, numOutputPorts) * layout->portSpacing +
                 layout->portSpacing;

  float leftInc = (height) / (numInputPorts + 1);
  float rightInc = (height) / (numOutputPorts + 1);
  float leftY = leftInc - height / 2;
  float rightY = rightInc - height / 2;
  float borderWidth = layout->borderWidth;

  it = circ_lliter(circ, symbolKindID);
  while (circ_lliter_next(&it)) {
    ID portID = it.current;
    if (circ_has_tags(circ, portID, TAG_IN)) {
      Position position = HMM_V2(-width / 2 + borderWidth / 2, leftY);
      circ_set_ptr(circ, portID, Position, &position);
      leftY += leftInc;
    } else {
      Position position = HMM_V2(width / 2 - borderWidth / 2, rightY);
      circ_set_ptr(circ, portID, Position, &position);
      rightY += rightInc;
    }
  }

  SymbolShape shape = circ_get(circ, symbolKindID, SymbolShape);
  if (shape != SYMSHAPE_DEFAULT) {
    // compensate for font based shapes being smaller
    height -= height * 2.0f / 5.0f;
  }

  Size size = (Size){.Width = width, .Height = height};
  circ_set_ptr(circ, symbolKindID, Size, &size);
}

void circ_load_symbol_descs(
  Circuit *circ, SymbolLayout *layout, const SymbolDesc *descs, size_t count) {
  for (size_t i = 0; i < count; i++) {
    const SymbolDesc *symDesc = &descs[i];
    ID symID = circ_add(circ, SymbolKind);
    circ_set(circ, symID, Name, {circ_str_c(circ, symDesc->typeName)});
    circ_set(
      circ, symID, Prefix, {circ_str(circ, (char[]){symDesc->namePrefix}, 1)});
    circ_set(circ, symID, SymbolShape, {symDesc->shape});

    for (size_t j = 0; j < symDesc->numPorts; j++) {
      PortDesc portDesc = symDesc->ports[j];
      ID portID = circ_add(circ, Port);
      circ_set(circ, portID, Parent, {symID});
      circ_set(circ, portID, Name, {circ_str_c(circ, portDesc.name)});
      circ_set(circ, portID, Number, {portDesc.number});
      if (portDesc.direction == PORT_IN || portDesc.direction == PORT_INOUT) {
        circ_add_tags(circ, portID, TAG_IN);
      }
      if (portDesc.direction == PORT_OUT || portDesc.direction == PORT_INOUT) {
        circ_add_tags(circ, portID, TAG_OUT);
      }

      circ_linked_list_append(circ, symID, portID);
    }

    circ_layout_symbol_kind(circ, layout, symID);
  }
}

static void circ_grow_entities(Circuit *circ, size_t newLength) {
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

static void circ_grow_table(Circuit *circ, EntityType type, size_t newLength) {
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
    *ptr = realloc(*(void **)ptr, newCapacity * meta->componentSize[i]);
  }

  header->capacity = newCapacity;
}

void circ_clone(Circuit *dst, Circuit *src) {
  // TODO: this should look at the logs and play back new log entries in dst

  circ_grow_entities(dst, src->capacity);
  memcpy(dst->generations, src->generations, src->capacity * sizeof(uint8_t));
  memcpy(dst->typeTags, src->typeTags, src->capacity * sizeof(uint16_t));
  memcpy(dst->rows, src->rows, src->capacity * sizeof(uint32_t));
  dst->numEntities = src->numEntities;
  arrsetlen(dst->freelist, arrlen(src->freelist));
  memcpy(dst->freelist, src->freelist, arrlen(src->freelist) * sizeof(ID));

  for (size_t i = 0; i < TYPE_COUNT; i++) {
    Table *srcTable = src->table[i];
    Table *dstTable = dst->table[i];
    circ_grow_table(dst, i, srcTable->length);
    if (srcTable->length > 0) {
      memcpy(dstTable->id, srcTable->id, srcTable->length * sizeof(ID));
      for (size_t j = 0; j < dst->tableMeta[i].componentCount; j++) {
        memcpy(
          circ_table_components_ptr(dst, i, j),
          circ_table_components_ptr(src, i, j),
          srcTable->length * dst->tableMeta[i].componentSize[j]);
      }
    }
    dstTable->length = srcTable->length;
  }

  if (!dst->foreignStrpool) {
    // free the old strpool before we overwrite it
    strpool_term(&dst->strpool);
  }

  // TODO: must be something better than this.... probably will be solved by
  // log playback
  memcpy(&dst->strpool, &src->strpool, sizeof(strpool_t));
  dst->foreignStrpool = true;
}

static void circ_add_impl(Circuit *circ, EntityType type, ID id) {
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
      circ_table_component_ptr(circ, type, i, row), 0, meta->componentSize[i]);
  }

  header->id[row] = id;
}

void circ_add_type_id(Circuit *circ, EntityType type, ID id) {
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

ID circ_add_type(Circuit *circ, EntityType type) {
  circ_grow_entities(circ, circ->numEntities + 1);
  ID id = arrpop(circ->freelist);
  circ_add_impl(circ, type, id);
  return id;
}

void circ_remove(Circuit *circ, ID id) {
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
      memcpy(dst, src, circ->tableMeta[type].componentSize[i]);
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

void circ_add_tags(Circuit *circ, ID id, Tag tags) {
  assert(circ_has(circ, id));
  circ->typeTags[id_index(id)] |= tags;
}

bool circ_has_tags(Circuit *circ, ID id, Tag tags) {
  assert(circ_has(circ, id));
  return (circ->typeTags[id_index(id)] & tags) == tags;
}

StringHandle circ_str(Circuit *circ, const char *str, size_t len) {
  StringHandle handle = (StringHandle)strpool_inject(&circ->strpool, str, len);
  strpool_incref(&circ->strpool, handle);
  return handle;
}

StringHandle circ_str_c(Circuit *circ, const char *str) {
  return circ_str(circ, str, strlen(str));
}

StringHandle circ_str_tmp(Circuit *circ, const char *str, size_t len) {
  StringHandle handle = (StringHandle)strpool_inject(&circ->strpool, str, len);
  return handle;
}

StringHandle circ_str_tmp_c(Circuit *circ, const char *str) {
  return circ_str_tmp(circ, str, strlen(str));
}

void circ_str_free(Circuit *circ, StringHandle handle) {
  int count = strpool_decref(&circ->strpool, handle);
  if (count == 0) {
    strpool_discard(&circ->strpool, handle);
  }
}

const char *circ_str_get(Circuit *circ, StringHandle handle) {
  if (handle == 0) {
    return "";
  }
  return strpool_cstr(&circ->strpool, handle);
}

void circ_linked_list_append(Circuit *circ, ID parent, ID child) {
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

void circ_linked_list_remove(Circuit *circ, ID parent, ID child) {
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

// ---

void circ_clear(Circuit *circ) {
  CircuitIter it = circ_iter(circ, Module);
  while (circ_iter_next(&it)) {
    Module *table = circ_iter_table(&it, Module);
    for (ptrdiff_t i = table->length - 1; i >= 0; i--) {
      circ_remove_module(circ, table->id[i]);
    }
  }
  circ->top = circ_add_module(circ);
}

// ---

ID circ_add_port(Circuit *circ, ID symbolKind) {
  ID portID = circ_add(circ, Port);
  circ_set(circ, portID, Parent, {symbolKind});
  circ_linked_list_append(circ, symbolKind, portID);
  return portID;
}

void circ_remove_port(Circuit *circ, ID id) {
  Parent symbolKind = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, symbolKind, id);
  circ_str_free(circ, circ_get(circ, id, Name));
  circ_remove(circ, id);
}

HMM_Vec2 circ_port_position(Circuit *circ, PortRef portRef) {
  Position portPosition = circ_get(circ, portRef.port, Position);
  Position symbolPosition = circ_get(circ, portRef.symbol, Position);
  return HMM_AddV2(symbolPosition, portPosition);
}

// ---

ID circ_add_symbol_kind(Circuit *circ) {
  ID symbolKindID = circ_add(circ, SymbolKind);
  return symbolKindID;
}

void circ_remove_symbol_kind(Circuit *circ, ID id) {
  const LinkedList *ll = circ_get_ptr(circ, id, LinkedList);
  while (circ_has(circ, ll->head)) {
    circ_remove_port(circ, ll->head);
  }

  // TODO: make this faster?
  CircuitIter it = circ_iter(circ, Symbol);
  while (circ_iter_next(&it)) {
    Symbol *symbols = circ_iter_table(&it, Symbol);
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

ID circ_get_symbol_kind_by_name(Circuit *circuit, const char *name) {
  ID symbolKindID = NO_ID;
  CircuitIter it = circ_iter(circuit, SymbolKind);
  while (circ_iter_next(&it)) {
    SymbolKind *table = circ_iter_table(&it, SymbolKind);
    for (size_t i = 0; i < table->length; i++) {
      const char *symbolKindName = circ_str_get(circuit, table->name[i]);
      if (strcmp(symbolKindName, name) == 0) {
        symbolKindID = table->id[i];
        break;
      }
    }
  }
  return symbolKindID;
}

// ---

ID circ_add_symbol(Circuit *circ, ID module, ID symbolKind) {
  assert(circ_has(circ, symbolKind));
  assert(circ_type_for_id(circ, symbolKind) == TYPE_SYMBOL_KIND);

  ID symbolID = circ_add(circ, Symbol);
  circ_set(circ, symbolID, Parent, {module});
  circ_set(circ, symbolID, SymbolKindID, {symbolKind});
  circ_linked_list_append(circ, module, symbolID);

  Prefix prefix = circ_get(circ, symbolKind, Prefix);
  Number maxNumber = {0};
  LinkedListIter it = circ_lliter(circ, module);
  while (circ_lliter_next(&it)) {
    ID otherSymbolID = it.current;
    ID otherKindID = circ_get(circ, otherSymbolID, SymbolKindID);
    Prefix otherPrefix = circ_get(circ, otherKindID, Prefix);
    if (otherPrefix == prefix) {
      Number number = circ_get(circ, otherSymbolID, Number);
      if (number > maxNumber) {
        maxNumber = number;
      }
    }
  }
  circ_set(circ, symbolID, Number, {maxNumber + 1});

  return symbolID;
}

void circ_remove_symbol(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_SYMBOL);

  Parent module = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, module, id);

  // TODO: when inverted indices are implemented, this can be done much faster
  CircuitIter it = circ_iter(circ, Endpoint);
  while (circ_iter_next(&it)) {
    Endpoint *table = circ_iter_table(&it, Endpoint);
    for (size_t i = 0; i < table->length; i++) {
      ID endpointID = table->id[i];
      PortRef ref = circ_get(circ, endpointID, PortRef);
      if (ref.symbol == id) {
        circ_disconnect_endpoint_from_port(circ, endpointID);
      }
    }
  }

  circ_remove(circ, id);
}

void circ_set_symbol_position(Circuit *circ, ID id, HMM_Vec2 position) {
  circ_set_ptr(circ, id, Position, &position);

  // TODO: when inverted indices are implemented, this can be done much faster
  CircuitIter it = circ_iter(circ, Endpoint);
  while (circ_iter_next(&it)) {
    Endpoint *table = circ_iter_table(&it, Endpoint);
    for (size_t i = 0; i < table->length; i++) {
      ID endpointID = table->id[i];
      PortRef ref = circ_get(circ, endpointID, PortRef);
      if (ref.symbol == id) {
        Position relPosition = circ_get(circ, ref.port, Position);
        Position portPosition = HMM_AddV2(position, relPosition);
        circ_set_endpoint_position(circ, endpointID, portPosition);
      }
    }
  }
}

Box circ_get_symbol_box(Circuit *circ, ID id) {
  HMM_Vec2 position = circ_get(circ, id, Position);
  SymbolKindID kindID = circ_get(circ, id, SymbolKindID);
  Size size = circ_get(circ, kindID, Size);
  return (Box){
    .center = position,
    .halfSize = HMM_MulV2F(size, 0.5f),
  };
}

typedef struct SymPos {
  ID symbolID;
  HMM_Vec2 position;
} SymPos;

static int compareSymPos(const void *a, const void *b) {
  SymPos *symPosA = (SymPos *)a;
  SymPos *symPosB = (SymPos *)b;

  if ((int)(symPosA->position.X / 20.0f) < (int)(symPosB->position.X / 20.0f)) {
    return -1;
  }
  if ((int)(symPosA->position.X / 20.0f) > (int)(symPosB->position.X / 20.0f)) {
    return 1;
  }

  if (symPosA->position.Y < symPosB->position.Y) {
    return -1;
  }
  if (symPosA->position.Y > symPosB->position.Y) {
    return 1;
  }

  return 0;
}

void circ_renumber_symbols(Circuit *circ, ID moduleID) {
  arr(Prefix) renumberedPrefixes = NULL;
  arr(SymPos) order = NULL;

  LinkedListIter it = circ_lliter(circ, moduleID);
  while (circ_lliter_next(&it)) {
    ID symbolID = it.current;
    ID symbolKindID = circ_get(circ, symbolID, SymbolKindID);
    Prefix prefix = circ_get(circ, symbolKindID, Prefix);
    bool renumbered = false;
    for (size_t i = 0; i < arrlen(renumberedPrefixes); i++) {
      if (renumberedPrefixes[i] == prefix) {
        renumbered = true;
        break;
      }
    }
    if (renumbered) {
      continue;
    }
    arrput(renumberedPrefixes, prefix);

    LinkedListIter it2 = circ_lliter(circ, moduleID);
    while (circ_lliter_next(&it2)) {
      ID symbolID2 = it2.current;
      ID symbolKindID2 = circ_get(circ, symbolID2, SymbolKindID);
      Prefix prefix2 = circ_get(circ, symbolKindID2, Prefix);
      if (prefix2 == prefix) {
        SymPos sympos = {symbolID2, circ_get(circ, symbolID2, Position)};
        arrput(order, sympos);
      }
    }

    qsort(order, arrlen(order), sizeof(SymPos), compareSymPos);

    Number number = 1;
    for (size_t i = 0; i < arrlen(order); i++) {
      ID symbolID = order[i].symbolID;
      circ_set(circ, symbolID, Number, {number});
      number++;
    }

    arrsetlen(order, 0);
  }
}

// ---

ID circ_add_waypoint(Circuit *circ, ID endpoint) {
  assert(circ_has(circ, endpoint));
  assert(circ_type_for_id(circ, endpoint) == TYPE_ENDPOINT);

  ID waypointID = circ_add(circ, Waypoint);
  circ_set(circ, waypointID, Parent, {endpoint});
  circ_linked_list_append(circ, endpoint, waypointID);
  return waypointID;
}

void circ_remove_waypoint(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_WAYPOINT);

  Parent endpoint = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, endpoint, id);
  circ_remove(circ, id);
}

void circ_set_waypoint_position(Circuit *circ, ID id, HMM_Vec2 position) {
  circ_set_ptr(circ, id, Position, &position);
}

// ---

ID circ_add_endpoint(Circuit *circ, ID subnet) {
  assert(circ_has(circ, subnet));
  assert(circ_type_for_id(circ, subnet) == TYPE_SUBNET);

  ID endpointID = circ_add(circ, Endpoint);
  circ_set(circ, endpointID, Parent, {subnet});
  circ_linked_list_append(circ, subnet, endpointID);
  return endpointID;
}

void circ_remove_endpoint(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_ENDPOINT);

  Parent subnet = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnet, id);

  LinkedListIter it = circ_lliter(circ, id);
  while (circ_lliter_next(&it)) {
    circ_remove_waypoint(circ, it.current);
  }

  circ_remove(circ, id);
}

void circ_set_endpoint_position(Circuit *circ, ID id, HMM_Vec2 position) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_ENDPOINT);

  circ_set_ptr(circ, id, Position, &position);
}

void circ_connect_endpoint_to_port(
  Circuit *circ, ID endpointID, ID symbolID, ID portID) {
  assert(circ_has(circ, endpointID));
  assert(circ_has(circ, symbolID));
  assert(circ_has(circ, portID));

  PortRef ref = (PortRef){.symbol = symbolID, .port = portID};
  circ_set_ptr(circ, endpointID, PortRef, &ref);
  Position position = circ_port_position(circ, ref);
  circ_set_ptr(circ, endpointID, Position, &position);
}

void circ_disconnect_endpoint_from_port(Circuit *circ, ID endpointID) {
  assert(circ_has(circ, endpointID));
  circ_set(circ, endpointID, PortRef, {0});
}

// ---

ID circ_add_subnet_bit(Circuit *circ, ID subnetBits) {
  assert(circ_has(circ, subnetBits));
  assert(circ_type_for_id(circ, subnetBits) == TYPE_SUBNET_BITS);

  ID subnetBitID = circ_add(circ, SubnetBit);
  circ_set(circ, subnetBitID, Parent, {subnetBits});
  circ_linked_list_append(circ, subnetBits, subnetBitID);
  return subnetBitID;
}

void circ_remove_subnet_bit(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_SUBNET_BIT);

  Parent subnetBits = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnetBits, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_subnet_bits(Circuit *circ, ID subnet) {
  assert(circ_has(circ, subnet));
  assert(circ_type_for_id(circ, subnet) == TYPE_SUBNET);

  ID subnetBitsID = circ_add(circ, SubnetBits);
  circ_set(circ, subnetBitsID, Parent, {subnet});
  circ_linked_list_append(circ, subnet, subnetBitsID);
  return subnetBitsID;
}

void circ_remove_subnet_bits(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_SUBNET_BITS);

  Parent subnet = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, subnet, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_subnet(Circuit *circ, ID net) {
  assert(circ_has(circ, net));
  assert(circ_type_for_id(circ, net) == TYPE_NET);

  ID subnetID = circ_add(circ, Subnet);
  circ_set(circ, subnetID, Parent, {net});
  circ_linked_list_append(circ, net, subnetID);
  return subnetID;
}

void circ_remove_subnet(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_SUBNET);

  Parent net = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, net, id);
  circ_remove(circ, id);
}

// ---

ID circ_add_net(Circuit *circ, ID module) {
  assert(circ_has(circ, module));
  assert(circ_type_for_id(circ, module) == TYPE_MODULE);

  ID netID = circ_add(circ, Net);
  ID netlistID = circ_get(circ, module, NetlistID);
  circ_set(circ, netID, Parent, {netlistID});
  circ_linked_list_append(circ, netlistID, netID);
  return netID;
}

void circ_remove_net(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_NET);

  Parent module = circ_get(circ, id, Parent);
  circ_linked_list_remove(circ, module, id);
  circ_remove(circ, id);
}

void circuit_set_net_wire_vertices(
  Circuit *circ, ID netID, WireVertices wireVerts) {
  circ_set_ptr(circ, netID, WireVertices, &wireVerts);
}

// ---

ID circ_add_module(Circuit *circ) {
  ID moduleID = circ_add(circ, Module);
  ID netlistID = circ_add(circ, Netlist);
  circ_set(circ, moduleID, NetlistID, {netlistID});
  circ_set(circ, netlistID, Parent, {moduleID});
  ID symbolKindID = circ_add_symbol_kind(circ);
  circ_set(circ, moduleID, SymbolKindID, {symbolKindID});
  circ_set(circ, symbolKindID, ModuleID, {moduleID});
  return moduleID;
}

void circ_remove_module(Circuit *circ, ID id) {
  assert(circ_has(circ, id));
  assert(circ_type_for_id(circ, id) == TYPE_MODULE);

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
