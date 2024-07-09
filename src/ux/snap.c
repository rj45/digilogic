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
#include "render/draw.h"
#include "ux.h"

#define SNAP_DISTANCE_THRESHOLD 500
#define SNAP_DISTANCE 20

HMM_Vec2 ux_calc_snap(CircuitUX *ux, HMM_Vec2 newCenter) {
  ID selected = ux->view.selected[0];
  HMM_Vec2 oldCenter;

  float origin = draw_screen_to_world(ux->view.drawCtx, HMM_V2(0, 0)).X;
  float snapDistance =
    draw_screen_to_world(ux->view.drawCtx, HMM_V2(SNAP_DISTANCE, 0)).X - origin;

  if (circ_type_for_id(&ux->view.circuit, selected) == TYPE_SYMBOL) {
    oldCenter = circ_get(&ux->view.circuit, selected, Position);
  } else if (circ_type_for_id(&ux->view.circuit, selected) == TYPE_WAYPOINT) {
    oldCenter = circ_get(&ux->view.circuit, selected, Position);
  } else {
    return newCenter;
  }

  HMM_Vec2 preSnapCenter = newCenter;

  float snapDistanceThreshold =
    draw_screen_to_world(ux->view.drawCtx, HMM_V2(SNAP_DISTANCE_THRESHOLD, 0))
      .X -
    origin;

  Box snapDistanceBox = {
    .center = oldCenter,
    .halfSize = HMM_V2(snapDistanceThreshold, snapDistanceThreshold),
  };

  ux->bvhQuery = bvh_query(&ux->bvh, snapDistanceBox, ux->bvhQuery);

  float bestXDistance = snapDistance;
  float bestYDistance = snapDistance;

  for (size_t i = 0; i < arrlen(ux->bvhQuery); i++) {
    ID id = ux->bvhQuery[i].item;

    if (id == selected) {
      continue;
    }

    Box box = ux->bvhQuery[i].box;
    if (id == selected) {
      continue;
    }

    if (HMM_ABS(preSnapCenter.Y - box.center.Y) <= bestYDistance) {
      bestYDistance = HMM_ABS(preSnapCenter.Y - box.center.Y);
      newCenter.Y = box.center.Y;
    }

    if (HMM_ABS(preSnapCenter.X - box.center.X) <= bestXDistance) {
      bestXDistance = HMM_ABS(preSnapCenter.X - box.center.X);
      newCenter.X = box.center.X;
    }
  }

  return newCenter;
}