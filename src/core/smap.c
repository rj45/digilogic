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

#include "core.h"

void smap_init(SparseMap *smap, IDType type) {
  *smap = (SparseMap){
    .type = type,
  };
}

void smap_free(SparseMap *smap) {
  if (smap->capacity > 0) {
    for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
      SyncedArray *syncedArray = &smap->syncedArrays[i];
      for (int j = 0; j < smap->length; j++) {
        for (int k = 0; k < arrlen(syncedArray->delete); k++) {
          syncedArray->delete[k].fn(
            syncedArray->delete[k].user, smap->ids[j],
            ((char *)*syncedArray->ptr) + (j * syncedArray->elemSize));
        }
      }
      free(*syncedArray->ptr);
      *syncedArray->ptr = NULL;
    }
    free(smap->ids);
  }
  arrfree(smap->syncedArrays);
  arrfree(smap->sparse);
  arrfree(smap->freeList);
}

void smap_clear(SparseMap *smap) {
  int last = smap->length - 1;
  while (last >= 0) {
    smap_del(smap, smap->ids[last]);
    last--;
  }
  arrsetlen(smap->freeList, 0);
  assert(smap_len(smap) == 0);
}

void smap_add_synced_array(SparseMap *smap, void **ptr, uint32_t elemSize) {
  SyncedArray syncedArray = {
    .ptr = ptr,
    .elemSize = elemSize,
  };
  arrput(smap->syncedArrays, syncedArray);
}

void smap_on_create(SparseMap *smap, void *array, SmapCallback callback) {
  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    if (*smap->syncedArrays[i].ptr == array) {
      arrput(smap->syncedArrays[i].create, callback);
      return;
    }
  }
  assert(0); // array was not found
}

void smap_on_update(SparseMap *smap, void *array, SmapCallback callback) {
  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    if (*smap->syncedArrays[i].ptr == array) {
      arrput(smap->syncedArrays[i].update, callback);
      return;
    }
  }
  assert(0); // array was not found
}

void smap_on_delete(SparseMap *smap, void *array, SmapCallback callback) {
  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    if (*smap->syncedArrays[i].ptr == array) {
      arrput(smap->syncedArrays[i].delete, callback);
      return;
    }
  }
  assert(0); // array was not found
}

static bool smap_grow(SparseMap *smap, int wantedCapacity) {
  if (wantedCapacity <= smap->capacity) {
    return true;
  }

  int newCapacity = smap->capacity * 2;
  if (newCapacity == 0) {
    newCapacity = 8;
  }

  while (newCapacity < wantedCapacity) {
    newCapacity *= 2;
  }

  ID *newIds = realloc(smap->ids, newCapacity * sizeof(ID));
  if (newIds == NULL) {
    // todo: emit OOM error
    return false;
  }
  smap->ids = newIds;

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    void *newPtr = realloc(
      *smap->syncedArrays[i].ptr, newCapacity * smap->syncedArrays[i].elemSize);
    if (newPtr == NULL) {
      // todo: emit OOM error
      return false;
    }
    *smap->syncedArrays[i].ptr = newPtr;
  }

  smap->capacity = newCapacity;

  return true;
}

ID smap_add(SparseMap *smap, void *value) {
  if (!smap_grow(smap, smap->length + 1)) {
    return NO_ID;
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

  memcpy(
    (char *)*smap->syncedArrays[0].ptr +
      denseIndex * smap->syncedArrays[0].elemSize,
    value, smap->syncedArrays[0].elemSize);

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    SyncedArray *syncedArray = &smap->syncedArrays[i];
    for (int j = 0; j < arrlen(syncedArray->create); j++) {
      syncedArray->create[j].fn(
        syncedArray->create[j].user, denseID,
        ((char *)*syncedArray->ptr) + denseIndex * syncedArray->elemSize);
    }
  }

  return denseID;
}

void smap_del(SparseMap *smap, ID id) {
  if (!smap_has(smap, id)) {
    return;
  }

  int sparseIndex = id_index(id);
  int denseIndex = id_index(smap->sparse[sparseIndex]);

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    SyncedArray *syncedArray = &smap->syncedArrays[i];
    for (int j = 0; j < arrlen(syncedArray->delete); j++) {
      syncedArray->delete[j].fn(
        syncedArray->delete[j].user, id,
        ((char *)*syncedArray->ptr) + denseIndex * syncedArray->elemSize);
    }
  }

  int lastDenseIndex = smap->length - 1;

  smap->sparse[sparseIndex] = NO_ID;
  arrput(smap->freeList, id);

  if (denseIndex != lastDenseIndex) {
    ID lastID = smap->ids[lastDenseIndex];
    smap->sparse[id_index(lastID)] =
      id_make(smap->type, id_gen(lastID), denseIndex);
    smap->ids[denseIndex] = lastID;

    for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
      SyncedArray *syncedArray = &smap->syncedArrays[i];
      int elemSize = syncedArray->elemSize;
      memcpy(
        (char *)*syncedArray->ptr + denseIndex * elemSize,
        (char *)*syncedArray->ptr + lastDenseIndex * elemSize, elemSize);
    }
  }

  smap->length--;
}

void smap_update_id(SparseMap *smap, ID id) {
  if (!smap_has(smap, id)) {
    return;
  }

  int sparseIndex = id_index(id);
  int denseIndex = id_index(smap->sparse[sparseIndex]);

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    SyncedArray *syncedArray = &smap->syncedArrays[i];
    for (int j = 0; j < arrlen(syncedArray->update); j++) {
      syncedArray->update[j].fn(
        syncedArray->update[j].user, id,
        ((char *)*syncedArray->ptr) + denseIndex * syncedArray->elemSize);
    }
  }
}

void smap_update_index(SparseMap *smap, uint32_t index) {
  if (index >= smap->length) {
    return;
  }

  for (int i = 0; i < arrlen(smap->syncedArrays); i++) {
    SyncedArray *syncedArray = &smap->syncedArrays[i];
    for (int j = 0; j < arrlen(syncedArray->update); j++) {
      syncedArray->update[j].fn(
        syncedArray->update[j].user, smap->ids[index],
        ((char *)*syncedArray->ptr) + index * syncedArray->elemSize);
    }
  }
}

void smap_clone_from(SparseMap *dst, SparseMap *src) {
  smap_clear(dst);
  smap_grow(dst, src->length);

  dst->type = src->type;
  dst->length = src->length;

  for (int i = 0; i < arrlen(dst->syncedArrays); i++) {
    void *srcPtr = *src->syncedArrays[i].ptr;
    void *dstPtr = *dst->syncedArrays[i].ptr;
    memcpy(dstPtr, srcPtr, src->length * src->syncedArrays[i].elemSize);
  }

  memcpy(dst->ids, src->ids, src->length * sizeof(ID));

  arrsetlen(dst->sparse, arrlen(src->sparse));
  memcpy(dst->sparse, src->sparse, arrlen(src->sparse) * sizeof(ID));

  arrsetlen(dst->freeList, arrlen(src->freeList));
  memcpy(dst->freeList, src->freeList, arrlen(src->freeList) * sizeof(ID));
}
