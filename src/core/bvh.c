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

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void bvh_init(BVH *bvh) { *bvh = (BVH){0}; }

void bvh_free(BVH *bvh) {
  arrfree(bvh->nodeHeap);
  arrfree(bvh->leaves);
  arrfree(bvh->stack);
  arrfree(bvh->scratch);
}

void bvh_clear(BVH *bvh) {
  arrsetlen(bvh->nodeHeap, 0);
  arrsetlen(bvh->leaves, 0);
  arrsetlen(bvh->stack, 0);
}

// static inline uint32_t bvh_parent(uint32_t i) { return (i - 1) / 2; }
static inline uint32_t bvh_left(uint32_t i) { return 2 * i + 1; }
static inline uint32_t bvh_right(uint32_t i) { return 2 * i + 2; }

void bvh_query_leaf_nodes(BVH *bvh, HMM_Vec2 point) {
  arrsetlen(bvh->scratch, 0);
  arrsetlen(bvh->stack, 0);
  if (arrlen(bvh->nodeHeap) == 0) {
    return;
  }
  arrput(bvh->stack, 0);
  while (arrlen(bvh->stack) > 0) {
    uint32_t index = arrpop(bvh->stack);
    BVHNode *node = &bvh->nodeHeap[index];
    if (box_intersect_point(node->box, point)) {
      if (node->numLeaves == 0) {
        arrput(bvh->stack, bvh_left(index));
        arrput(bvh->stack, bvh_right(index));
      }
      arrput(bvh->scratch, index);
    }
  }
}

arr(BVHLeaf) bvh_query(BVH *bvh, Box box, arr(BVHLeaf) result) {
  if (bvh->needsRebuild) {
    bvh_rebuild(bvh);
  }
  if (arrlen(bvh->nodeHeap) == 0) {
    return result;
  }
  arrsetlen(bvh->stack, 0);
  arrput(bvh->stack, 0);
  while (arrlen(bvh->stack) > 0) {
    uint32_t index = arrpop(bvh->stack);
    BVHNode *node = &bvh->nodeHeap[index];
    if (box_intersect_box(box, node->box)) {
      if (node->numLeaves == 0) {
        arrput(bvh->stack, bvh_left(index));
        arrput(bvh->stack, bvh_right(index));
      } else {
        for (uint32_t i = 0; i < node->numLeaves; i++) {
          if (box_intersect_box(box, bvh->leaves[node->firstLeaf + i].box)) {
            // leaf may be in the BVH multiple times, so we need to check
            // if it's already in the result array
            BVHLeaf leaf = bvh->leaves[node->firstLeaf + i];
            bool found = false;
            for (size_t j = 0; j < arrlen(result); j++) {
              if (
                result[j].item == leaf.item &&
                result[j].subitem == leaf.subitem &&
                box_equal(result[j].box, leaf.box)) {
                found = true;
                break;
              }
            }
            if (!found) {
              arrput(result, leaf);
            }
          }
        }
      }
    }
  }
  return result;
}

void bvh_add(BVH *bvh, ID item, ID subitem, Box box) {
  BVHLeaf leaf = {.box = box, .item = item, .subitem = subitem};
  bvh_query_leaf_nodes(bvh, box.center);
  for (size_t i = 0; i < arrlen(bvh->scratch); i++) {
    uint32_t index = bvh->scratch[i];
    BVHNode *node = &bvh->nodeHeap[index];
    if (node->numLeaves > 0) {
      // This node has leaves, so add the leaf to this node,
      // then increase the `firstLeaf` of all the nodes that
      // come after it.
      arrins(bvh->leaves, node->firstLeaf + node->numLeaves, leaf);
      node->numLeaves++;
      for (size_t j = index + 1; j < arrlen(bvh->nodeHeap); j++) {
        bvh->nodeHeap[j].firstLeaf++;
      }
      node->box = box_union(node->box, box);
      return;
    }
  }
  // fallback, just add to the end and rebuild
  arrput(bvh->leaves, leaf);
  bvh->needsRebuild = true;
}

void bvh_remove(BVH *bvh, ID item, ID subitem, Box box) {
  bvh_query_leaf_nodes(bvh, box.center);
  for (size_t i = 0; i < arrlen(bvh->scratch); i++) {
    uint32_t index = bvh->scratch[i];
    BVHNode *node = &bvh->nodeHeap[index];
    if (node->numLeaves > 0) {
      for (size_t j = 0; j < node->numLeaves; j++) {
        BVHLeaf leaf = bvh->leaves[node->firstLeaf + j];
        if (
          leaf.item == item && leaf.subitem == subitem &&
          box_equal(leaf.box, box)) {
          arrdel(bvh->leaves, node->firstLeaf + j);
          node->numLeaves--;
          for (size_t k = index + 1; k < arrlen(bvh->nodeHeap); k++) {
            bvh->nodeHeap[k].firstLeaf--;
          }
          return;
        }
      }
    }
  }
}

void bvh_update(BVH *bvh, ID item, ID subitem, Box oldBox, Box newBox) {
  // TODO: a more efficient algorithm
  bvh_remove(bvh, item, subitem, oldBox);
  bvh_add(bvh, item, subitem, newBox);
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
    node->box = box;
    return;
  }

  uint32_t half = (node->numLeaves) / 2;
  if (bvh_right(index) >= arrlen(bvh->nodeHeap)) {
    arrsetlen(bvh->nodeHeap, bvh_right(index) + 1);
    node = &bvh->nodeHeap[index];
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
  node = &bvh->nodeHeap[index];

  node->box = box_union(
    bvh->nodeHeap[bvh_left(index)].box, bvh->nodeHeap[bvh_right(index)].box);

  node->firstLeaf = 0;
  node->numLeaves = 0;
}

#include "sokol_time.h"
void bvh_rebuild(BVH *bvh) {
  uint64_t now = stm_now();
  arrsetlen(bvh->nodeHeap, 1);

  bvh->nodeHeap[0].firstLeaf = 0;
  bvh->nodeHeap[0].numLeaves = arrlen(bvh->leaves);

  if (arrlen(bvh->leaves) > 0) {
    bvh_recursive_subdivide(bvh, 0, 0);
  }
  uint64_t elapsed = stm_since(now);
  log_debug("BVH rebuild took %f ms", stm_ms(elapsed));

  bvh->needsRebuild = false;
}
