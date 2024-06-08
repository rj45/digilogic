#include "newstructs.h"

#include <assert.h>
#include <stdbool.h>

static inline bool circ_has(Circuit *circuit, ID id) {
  return id_index(id) < circuit->numEntities &&
         circuit->generations[id_index(id)] == id_gen(id);
}

static inline size_t circ_row(Circuit *circuit, ID id) {
  assert(circ_has(circuit, id));
  return circuit->rows[id_index(id)];
}

static inline ID circ_id(Circuit *circuit, EntityType type, size_t row) {
  assert(type < TYPE_COUNT && type >= 0);
  assert(row < circuit->header[type]->length);
  return circuit->header[type]->id[row];
}
