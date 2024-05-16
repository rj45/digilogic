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

static void ux_perform_command(CircuitUX *ux, UndoCommand command) {
  switch (command.verb) {
  case UNDO_NONE:
    break;
  case UNDO_MOVE_SELECTION: {
    HMM_Vec2 initialDelta = HMM_SubV2(command.newCenter, command.oldCenter);
    HMM_Vec2 newCenter = command.newCenter;
    HMM_Vec2 delta = initialDelta;
    if (arrlen(ux->view.selected) == 1 && command.snap) {
      newCenter = ux_calc_snap(ux, command.newCenter);
    }

    HMM_Vec2 currentCenter = ux_calc_selection_center(ux);
    delta = HMM_SubV2(newCenter, currentCenter);

    for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
      ID id = ux->view.selected[i];
      if (id_type(id) == ID_COMPONENT) {
        ux_move_component(ux, id, delta);
      } else if (id_type(id) == ID_WAYPOINT) {
        ux_move_waypoint(ux, id, delta);
      }
    }
    ux_route(ux);
    ux->view.selectionBox.center =
      HMM_AddV2(ux->view.selectionBox.center, initialDelta);
    ux->downStart = HMM_AddV2(ux->downStart, initialDelta);
    ux->selectionCenter = command.newCenter;
    break;
  }
  case UNDO_SELECT_ITEM:
    arrput(ux->view.selected, command.itemID);
    break;
  case UNDO_SELECT_AREA:
    ux->view.selectionBox = command.area;
    arrsetlen(ux->view.selected, 0);
    for (size_t i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
      Component *component = &ux->view.circuit.components[i];
      if (box_intersect_box(component->box, command.area)) {
        arrput(ux->view.selected, circuit_component_id(&ux->view.circuit, i));
      }
    }
    for (size_t i = 0; i < circuit_waypoint_len(&ux->view.circuit); i++) {
      Waypoint *waypoint = &ux->view.circuit.waypoints[i];
      Box box = (Box){.center = waypoint->position, .halfSize = HMM_V2(5, 5)};
      if (box_intersect_box(box, command.area)) {
        arrput(ux->view.selected, circuit_waypoint_id(&ux->view.circuit, i));
      }
    }
    break;
  case UNDO_DESELECT_ITEM:
    for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
      if (ux->view.selected[i] == command.itemID) {
        arrdel(ux->view.selected, i);
        break;
      }
    }
    break;
  case UNDO_DESELECT_AREA:
    arrsetlen(ux->view.selected, 0);
    ux->view.selectionBox = (Box){0};
    break;
  }
}

static void ux_push_undo(CircuitUX *ux, UndoCommand command) {
  int last = arrlen(ux->undoStack) - 1;
  if (last >= 0) {
    // merge commands if they are the same
    UndoCommand *lastCommand = &ux->undoStack[last];
    if (lastCommand->verb == command.verb) {
      switch (command.verb) {
      case UNDO_NONE:
        return;
      case UNDO_MOVE_SELECTION:
        lastCommand->newCenter = command.newCenter;
        return;
      case UNDO_SELECT_ITEM:
        lastCommand->itemID = command.itemID;
        return;
      case UNDO_SELECT_AREA:
        lastCommand->area = command.area;
        return;
      case UNDO_DESELECT_ITEM:
        if (lastCommand->itemID == command.itemID) {
          return;
        }
        break;
      case UNDO_DESELECT_AREA:
        break;
      }
    }
  }
  arrput(ux->undoStack, command);
}

void ux_do(CircuitUX *ux, UndoCommand command) {
  arrsetlen(ux->redoStack, 0);
  ux_push_undo(ux, command);
  ux_perform_command(ux, command);
}

static UndoCommand ux_flip_command(UndoCommand cmd) {
  UndoCommand flip = {0};
  switch (cmd.verb) {
  case UNDO_NONE:
    break;
  case UNDO_MOVE_SELECTION:
    flip.verb = UNDO_MOVE_SELECTION;
    HMM_Vec2 tmp = cmd.oldCenter;
    flip.oldCenter = cmd.newCenter;
    flip.newCenter = tmp;
    break;
  case UNDO_SELECT_ITEM:
    flip.verb = UNDO_DESELECT_ITEM;
    flip.itemID = cmd.itemID;
    break;
  case UNDO_SELECT_AREA:
    flip.verb = UNDO_DESELECT_AREA;
    flip.area = cmd.area;
    break;
  case UNDO_DESELECT_ITEM:
    flip.verb = UNDO_SELECT_ITEM;
    flip.itemID = cmd.itemID;
    break;
  case UNDO_DESELECT_AREA:
    flip.verb = UNDO_SELECT_AREA;
    flip.area = cmd.area;
    break;
  }
  return flip;
}

UndoCommand ux_undo(CircuitUX *ux) {
  if (arrlen(ux->undoStack) == 0) {
    return (UndoCommand){0};
  }

  UndoCommand cmd = arrpop(ux->undoStack);

  // push the opposite of the command to the redo stack
  UndoCommand redoCmd = ux_flip_command(cmd);
  arrput(ux->redoStack, redoCmd);

  ux_perform_command(ux, redoCmd);
  return cmd;
}

UndoCommand ux_redo(CircuitUX *ux) {
  if (arrlen(ux->redoStack) == 0) {
    return (UndoCommand){0};
  }

  UndoCommand cmd = arrpop(ux->redoStack);

  // push the opposite of the command to the undo stack
  UndoCommand undoCmd = ux_flip_command(cmd);
  arrput(ux->undoStack, undoCmd);

  ux_perform_command(ux, undoCmd);
  return cmd;
}
