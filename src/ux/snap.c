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

#include "handmade_math.h"
#include "ux.h"

#define SNAP_DISTANCE_THRESHOLD 500
#define SNAP_DISTANCE 8

HMM_Vec2 ux_calc_snap(CircuitUX *ux, HMM_Vec2 newCenter) {
  ID selected = ux->view.selected[0];
  HMM_Vec2 oldCenter;
  HMM_Vec2 halfSize;

  float snapDistance = SNAP_DISTANCE / draw_get_zoom(ux->view.drawCtx);

  if (id_type(selected) == ID_COMPONENT) {
    Component *component = circuit_component_ptr(&ux->view.circuit, selected);
    oldCenter = component->box.center;
    halfSize = component->box.halfSize;

  } else if (id_type(selected) == ID_WAYPOINT) {
    Waypoint *waypoint = circuit_waypoint_ptr(&ux->view.circuit, selected);
    oldCenter = waypoint->position;
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
  for (size_t i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
    Component *component = &ux->view.circuit.components[i];
    if (circuit_component_id(&ux->view.circuit, i) == selected) {
      continue;
    }

    if (
      HMM_LenV2(HMM_SubV2(oldCenter, component->box.center)) >
      snapDistanceThreshold) {
      continue;
    }

    float componentTop = component->box.center.Y - component->box.halfSize.Y;
    float componentLeft = component->box.center.X - component->box.halfSize.X;
    float componentBottom = component->box.center.Y + component->box.halfSize.Y;
    float componentRight = component->box.center.X + component->box.halfSize.X;

    if (HMM_ABS(top - componentTop) < SNAP_DISTANCE) {
      newCenter.Y = componentTop + halfSize.Y;
    }
    if (HMM_ABS(left - componentLeft) < SNAP_DISTANCE) {
      newCenter.X = componentLeft + halfSize.X;
    }
    if (HMM_ABS(bottom - componentBottom) < SNAP_DISTANCE) {
      newCenter.Y = componentBottom - halfSize.Y;
    }
    if (HMM_ABS(right - componentRight) < SNAP_DISTANCE) {
      newCenter.X = componentRight - halfSize.X;
    }

    if (HMM_ABS(movedCenter.Y - component->box.center.Y) < snapDistance) {
      newCenter.Y = component->box.center.Y;
    }

    if (HMM_ABS(movedCenter.X - component->box.center.X) < snapDistance) {
      newCenter.X = component->box.center.X;
    }
  }

  return newCenter;
}