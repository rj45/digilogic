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

#include "stb_ds.h"
#include <stdint.h>

void bvh_init(BVH *bvh) { *bvh = (BVH){0}; }

void bvh_free(BVH *bvh) {
  arrfree(bvh->nodeHeap);
  arrfree(bvh->leaves);
  arrfree(bvh->stack);
}

// static inline uint32_t bvh_parent(uint32_t i) { return (i - 1) / 2; }
static inline uint32_t bvh_left(uint32_t i) { return 2 * i + 1; }
static inline uint32_t bvh_right(uint32_t i) { return 2 * i + 2; }

void bvh_add(BVH *bvh, Box box, ID item) {
  // todo: a more efficient algorithm
  BVHLeaf leaf = {.box = box, .item = item};
  arrput(bvh->leaves, leaf);
  bvh_rebuild(bvh);
}

void bvh_remove(BVH *bvh, Box box, ID item) {}

void bvh_update(BVH *bvh, Box oldBox, Box newBox, ID item) {
  // todo: a more efficient algorithm
  bvh_remove(bvh, oldBox, item);
  bvh_add(bvh, newBox, item);
}

static int bvh_compare_x(const void *a, const void *b) {
  BVHLeaf *al = (BVHLeaf *)a;
  BVHLeaf *bl = (BVHLeaf *)b;
  return al->box.center.X - bl->box.center.X;
}
static int bvh_compare_y(const void *a, const void *b) {
  BVHLeaf *al = (BVHLeaf *)a;
  BVHLeaf *bl = (BVHLeaf *)b;
  return al->box.center.Y - bl->box.center.Y;
}
int (*bvh_axis_compares[2])(const void *, const void *) = {
  &bvh_compare_x, &bvh_compare_y};

#define LEAVES_PER_NODE 4

static void bvh_recursive_subdivide(BVH *bvh, uint32_t index, int axis) {
  BVHNode *node = &bvh->nodeHeap[index];
  if (node->numLeaves <= LEAVES_PER_NODE) {
    // calculate the bounding box
    Box box = bvh->leaves[node->firstLeaf].box;
    for (uint32_t i = 1; i < node->numLeaves; i++) {
      box = box_union(box, bvh->leaves[node->firstLeaf + i].box);
    }
    return;
  }

  uint32_t half = (node->numLeaves) / 2;
  if (bvh_right(index) >= arrlen(bvh->nodeHeap)) {
    arrsetlen(bvh->nodeHeap, arrlen(bvh->nodeHeap) * 2);
  }

  // for debugging
  node->median = bvh->leaves[node->firstLeaf + half].box.center.Elements[axis];
  node->axis = axis;

  bvh->nodeHeap[bvh_left(index)] =
    (BVHNode){.firstLeaf = node->firstLeaf, .numLeaves = half};
  bvh->nodeHeap[bvh_right(index)] = (BVHNode){
    .firstLeaf = node->firstLeaf + half, .numLeaves = node->numLeaves - half};

  qsort(
    &bvh->leaves[node->firstLeaf], node->numLeaves, sizeof(BVHLeaf),
    bvh_axis_compares[axis]);

  bvh_recursive_subdivide(bvh, bvh_left(index), axis ? 0 : 1);
  bvh_recursive_subdivide(bvh, bvh_right(index), axis ? 0 : 1);

  bvh->nodeHeap[index].box = box_union(
    bvh->nodeHeap[bvh_left(index)].box, bvh->nodeHeap[bvh_right(index)].box);

  bvh->nodeHeap[index].firstLeaf = 0;
  bvh->nodeHeap[index].numLeaves = 0;
}

void bvh_rebuild(BVH *bvh) {
  arrsetlen(bvh->nodeHeap, 1);

  bvh->nodeHeap[0].firstLeaf = 0;
  bvh->nodeHeap[0].numLeaves = arrlen(bvh->leaves);

  bvh_recursive_subdivide(bvh, 0, 0);
}

arr(ID) bvh_query(BVH *bvh, Box box, arr(ID) result);
