#include "core.h"

void smap_init(SparseMap *smap, IDType type) {
  *smap = (SparseMap){
    .type = type,
  };
}

void smap_free(SparseMap *smap) {
  if (smap->capacity > 0) {
    for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
      free(*smap->syncedArrays[i].ptr);
      *smap->syncedArrays[i].ptr = NULL;
    }
    free(smap->ids);
  }
  arrfree(smap->syncedArrays);
  arrfree(smap->sparse);
  arrfree(smap->freeList);
}

void smap_add_synced_array(SparseMap *smap, void **ptr, uint32_t elemSize) {
  SyncedArray syncedArray = {
    .ptr = ptr,
    .elemSize = elemSize,
  };
  arrput(smap->syncedArrays, syncedArray);
}

ID smap_alloc(SparseMap *smap) {
  if (smap->capacity <= smap->length + 1) {
    int newCapacity = smap->capacity * 2;
    if (newCapacity == 0) {
      newCapacity = 8;
    }

    ID *newIds = realloc(smap->ids, newCapacity * sizeof(ID));
    if (newIds == NULL) {
      // todo: emit OOM error
      return NO_ID;
    }
    smap->ids = newIds;

    for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
      void *newPtr = realloc(
        *smap->syncedArrays[i].ptr,
        newCapacity * smap->syncedArrays[i].elemSize);
      if (newPtr == NULL) {
        // todo: emit OOM error
        return NO_ID;
      }
      *smap->syncedArrays[i].ptr = newPtr;
    }

    smap->capacity = newCapacity;
  }

  int sparseIndex;
  int gen = 0;
  if (arrlen(smap->freeList) > 0) {
    int id = arrpop(smap->freeList);
    gen = id_gen(id);
    sparseIndex = id_index(id);
  } else {
    sparseIndex = arrlen(smap->sparse);
    arrput(smap->sparse, 0);
  }

  gen = (gen + 1) & ID_GEN_MASK;
  if (gen == 0) {
    // todo: report the overflow for debugging
    gen = 1;
  }

  int denseIndex = smap->length;
  smap->length++;

  ID sparseID = id_make(smap->type, gen, denseIndex);
  ID denseID = id_make(smap->type, gen, sparseIndex);

  smap->sparse[sparseIndex] = sparseID;
  smap->ids[denseIndex] = denseID;

  return denseID;
}

void smap_del(SparseMap *smap, ID id) {
  if (!smap_has(smap, id)) {
    return;
  }

  int sparseIndex = id_index(id);
  int denseIndex = id_index(smap->sparse[sparseIndex]);
  int lastDenseIndex = smap->length - 1;

  smap->sparse[sparseIndex] = NO_ID;
  arrput(smap->freeList, id);

  if (denseIndex != lastDenseIndex) {
    ID lastID = smap->ids[lastDenseIndex];
    smap->sparse[id_index(lastID)] =
      id_make(smap->type, id_gen(lastID), denseIndex);
    smap->ids[denseIndex] = lastID;

    for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
      int elemSize = smap->syncedArrays[i].elemSize;
      memcpy(
        (char *)*smap->syncedArrays[i].ptr + denseIndex * elemSize,
        (char *)*smap->syncedArrays[i].ptr + lastDenseIndex * elemSize,
        elemSize);
    }
  }

  smap->length--;
}
