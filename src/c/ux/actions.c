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
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#define LOG_LEVEL LL_INFO
#include "log.h"

void ux_move_selection(
  CircuitUX *ux, HMM_Vec2 oldCenter, HMM_Vec2 newCenter, bool snap) {
  log_debug(
    "Performing move selection: %f %f -> %f %f", oldCenter.X, oldCenter.Y,
    newCenter.X, newCenter.Y);
  HMM_Vec2 initialDelta = HMM_SubV2(newCenter, oldCenter);
  HMM_Vec2 updatedCenter = newCenter;
  if (arrlen(ux->view.selected) == 1 && snap) {
    updatedCenter = ux_calc_snap(ux, newCenter);
  }

  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    ID id = ux->view.selected[i];
    if (circ_type_for_id(&ux->view.circuit, id) == TYPE_SYMBOL) {
      circ_set_symbol_position(&ux->view.circuit, id, updatedCenter);
    } else if (circ_type_for_id(&ux->view.circuit, id) == TYPE_WAYPOINT) {
      circ_set_waypoint_position(&ux->view.circuit, id, updatedCenter);
    }
  }
  ux_route(ux);
  ux->view.selectionBox.center =
    HMM_AddV2(ux->view.selectionBox.center, initialDelta);
  ux->downStart = HMM_AddV2(ux->downStart, initialDelta);
  ux->selectionCenter = newCenter;
}

void ux_select_item(CircuitUX *ux, ID id) {
  log_debug("Performing select item: %x", id);
  arrput(ux->view.selected, id);
  ux->selectionCenter = ux_calc_selection_center(ux);
}

void ux_select_area(CircuitUX *ux, Box area) {
  log_debug(
    "Performing select area: %f %f %f %f", area.center.X, area.center.Y,
    area.halfSize.X, area.halfSize.Y);
  ux->view.selectionBox = area;
  arrsetlen(ux->view.selected, 0);

  ux->bvhQuery = bvh_query(&ux->bvh, area, ux->bvhQuery);
  for (size_t i = 0; i < arrlen(ux->bvhQuery); i++) {
    ID id = ux->bvhQuery[i].item;
    EntityType type = circ_type_for_id(&ux->view.circuit, id);
    if (type == TYPE_SYMBOL || type == TYPE_WAYPOINT) {
      arrput(ux->view.selected, id);
    }
  }
}

void ux_deselect_item(CircuitUX *ux, ID id) {
  log_debug("Performing deselect item: %x", id);
  for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
    if (ux->view.selected[i] == id) {
      arrdel(ux->view.selected, i);
      break;
    }
  }
}

void ux_deselect_area(CircuitUX *ux, Box area) {
  log_debug(
    "Performing deselect area: %f %f %f %f", area.center.X, area.center.Y,
    area.halfSize.X, area.halfSize.Y);
  arrsetlen(ux->view.selected, 0);
  ux->view.selectionBox = (Box){0};
}

void ux_add_symbol(CircuitUX *ux, ID parentID, HMM_Vec2 center) {
  log_debug("Performing add symbol: %x %f %f", parentID, center.X, center.Y);

  ID id = circ_add_symbol(&ux->view.circuit, ux->view.circuit.top, parentID);
  circ_set_symbol_position(&ux->view.circuit, id, center);
}

void ux_del_symbol(CircuitUX *ux, ID id) {
  log_debug("Performing del symbol: %x", id);
  circ_remove_symbol(&ux->view.circuit, id);
}

void ux_add_waypoint(CircuitUX *ux, ID parentID, HMM_Vec2 center) {
  log_debug("Performing add waypoint: %x %f %f", parentID, center.X, center.Y);
  ID id = circ_add_waypoint(&ux->view.circuit, parentID);
  circ_set_waypoint_position(&ux->view.circuit, id, center);
}

void ux_del_waypoint(CircuitUX *ux, ID id) {
  log_debug("Performing del waypoint: %x", id);
  circ_remove_waypoint(&ux->view.circuit, id);
}

void ux_undo(CircuitUX *ux) {
  if (ux->tool == TOOL_SYMBOL) {
    // will crash if we allow this
    return;
  }

  circ_undo(&ux->view.circuit);
  ux_route(ux);
  ux_build_bvh(ux);
  return;
}

void ux_redo(CircuitUX *ux) {
  if (ux->tool == TOOL_SYMBOL) {
    // will crash if we allow this
    return;
  }

  // ux_perform_command(ux, undoCmd);
  circ_redo(&ux->view.circuit);
  ux_route(ux);
  ux_build_bvh(ux);
  return;
}
