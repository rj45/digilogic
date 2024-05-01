#include "core.h"

#include "stb_ds.h"

SparseMap *smap_grow_(
  SparseMap *arr, uint32_t elemSize, uint32_t addLen, uint32_t minCap) {

  int minLen = smap_len(arr) + addLen;

  if (minCap < minLen) {
    minCap = minLen;
  }

  if (minCap <= smap_cap(arr)) {
    return arr;
  }

  int capacity = smap_cap(arr);
  if (capacity == 0) {
    capacity = 8;
  }
  while (minCap > capacity) {
    capacity *= 2;
  }

  void *newArr = realloc(
    (arr == NULL) ? 0 : smap_header(arr),
    sizeof(SparseMapHeader) + elemSize * capacity);
  SparseMapHeader *header = (SparseMapHeader *)newArr;
  if (arr == NULL) {
    *header = (SparseMapHeader){
      .ids = realloc(smap_ids(arr), capacity * sizeof(ID)),
      .elemSize = elemSize,
      .elemCount = 0,
      .capacity = capacity,
    };
  }

  header->capacity = capacity;

  return &header[1];
}

void smap_free_(SparseMap *arr) {
  SparseMapHeader *header = smap_header(arr);
  arrfree(header->sparse);
  arrfree(header->freeList);
  free(header->ids);
  free(header);
}

void smap_del(SparseMap *arr, ID id) {
  if (!smap_has(arr, id)) {
    return;
  }
  SparseMapHeader *header = smap_header(arr);
  int oldIndex = smap_index(arr, id);
  int lastIndex = header->elemCount - 1;
  header->sparse[id_index(id)] = 0;
  if (oldIndex != lastIndex) {
    ID lastID = header->ids[lastIndex];
    header->sparse[id_index(lastID)] =
      id_make(id_type(lastID), id_gen(lastID), id_index(oldIndex));
    header->ids[oldIndex] = header->ids[lastIndex];
    memcpy(
      ((uint8_t *)arr) + oldIndex * header->elemSize,
      ((uint8_t *)arr) + lastIndex * header->elemSize, header->elemSize);
  } else {
    header->sparse[id_index(id)] = 0;
  }
  arrput(header->freeList, id);
  header->elemCount--;
}
