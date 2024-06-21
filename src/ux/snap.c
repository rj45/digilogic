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
#include "ux.h"

#define SNAP_DISTANCE_THRESHOLD 500
#define SNAP_DISTANCE 8

HMM_Vec2 ux_calc_snap(CircuitUX *ux, HMM_Vec2 newCenter) {
  ID selected = ux->view.selected[0];
  HMM_Vec2 oldCenter;
  HMM_Vec2 halfSize;

  float snapDistance = SNAP_DISTANCE / draw_get_zoom(ux->view.drawCtx);

  if (circ_type_for_id(&ux->view.circuit2, selected) == TYPE_SYMBOL) {
    oldCenter = circ_get(&ux->view.circuit2, selected, Position);
    SymbolKindID kind = circ_get(&ux->view.circuit2, selected, SymbolKindID);
    halfSize = HMM_MulV2F(circ_get(&ux->view.circuit2, kind, Size), 0.5f);
  } else if (circ_type_for_id(&ux->view.circuit2, selected) == TYPE_WAYPOINT) {
    oldCenter = circ_get(&ux->view.circuit2, selected, Position);
    halfSize = HMM_V2(0, 0);
  } else {
    return newCenter;
  }

  HMM_Vec2 movedCenter = newCenter;

  float top = movedCenter.Y - halfSize.Y;
  float left = movedCenter.X - halfSize.X;
  float bottom = movedCenter.Y + halfSize.Y;
  float right = movedCenter.X + halfSize.X;

  float snapDistanceThreshold =
    SNAP_DISTANCE_THRESHOLD / draw_get_zoom(ux->view.drawCtx);

  Box snapDistanceBox = {
    .center = oldCenter,
    .halfSize = HMM_V2(snapDistanceThreshold, snapDistanceThreshold),
  };

  ux->bvhQuery = bvh_query(&ux->bvh, snapDistanceBox, ux->bvhQuery);

  for (size_t i = 0; i < arrlen(ux->bvhQuery); i++) {
    ID id = ux->bvhQuery[i].item;

    Box box = ux->bvhQuery[i].box;
    if (id == selected) {
      continue;
    }

    float itemTop = box.center.Y - box.halfSize.Y;
    float itemLeft = box.center.X - box.halfSize.X;
    float itemBottom = box.center.Y + box.halfSize.Y;
    float itemRight = box.center.X + box.halfSize.X;

    if (HMM_ABS(top - itemTop) < SNAP_DISTANCE) {
      newCenter.Y = itemTop + halfSize.Y;
    }
    if (HMM_ABS(left - itemLeft) < SNAP_DISTANCE) {
      newCenter.X = itemLeft + halfSize.X;
    }
    if (HMM_ABS(bottom - itemBottom) < SNAP_DISTANCE) {
      newCenter.Y = itemBottom - halfSize.Y;
    }
    if (HMM_ABS(right - itemRight) < SNAP_DISTANCE) {
      newCenter.X = itemRight - halfSize.X;
    }

    if (HMM_ABS(movedCenter.Y - box.center.Y) < snapDistance) {
      newCenter.Y = box.center.Y;
    }

    if (HMM_ABS(movedCenter.X - box.center.X) < snapDistance) {
      newCenter.X = box.center.X;
    }
  }

  return newCenter;
}