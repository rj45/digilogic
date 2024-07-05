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

static inline UndoCommand *ux_undo_stack_top(CircuitUX *ux) {
  if (arrlen(ux->redoStack) > 0) {
    return &ux->redoStack[arrlen(ux->redoStack) - 1];
  }
  if (arrlen(ux->undoStack) == 0) {
    return NULL;
  }
  return &ux->undoStack[arrlen(ux->undoStack) - 1];
}

static void ux_perform_command(CircuitUX *ux, UndoCommand command) {
  ux->changed = true;

  switch (command.verb) {
  case UNDO_NONE:
    break;
  case UNDO_MOVE_SELECTION: {
    log_debug(
      "Performing move selection: %f %f -> %f %f", command.oldCenter.X,
      command.oldCenter.Y, command.newCenter.X, command.newCenter.Y);
    HMM_Vec2 initialDelta = HMM_SubV2(command.newCenter, command.oldCenter);
    HMM_Vec2 newCenter = command.newCenter;
    if (arrlen(ux->view.selected) == 1 && command.snap) {
      newCenter = ux_calc_snap(ux, command.newCenter);
    }

    for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
      ID id = ux->view.selected[i];
      if (circ_type_for_id(&ux->view.circuit, id) == TYPE_SYMBOL) {
        circ_set_symbol_position(&ux->view.circuit, id, newCenter);
      } else if (circ_type_for_id(&ux->view.circuit, id) == TYPE_WAYPOINT) {
        circ_set_waypoint_position(&ux->view.circuit, id, newCenter);
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
    log_debug("Performing select item: %x", command.selectedID);
    arrput(ux->view.selected, command.selectedID);
    break;
  case UNDO_SELECT_AREA:
    log_debug(
      "Performing select area: %f %f %f %f", command.area.center.X,
      command.area.center.Y, command.area.halfSize.X, command.area.halfSize.Y);
    ux->view.selectionBox = command.area;
    arrsetlen(ux->view.selected, 0);

    ux->bvhQuery = bvh_query(&ux->bvh, command.area, ux->bvhQuery);
    for (size_t i = 0; i < arrlen(ux->bvhQuery); i++) {
      ID id = ux->bvhQuery[i].item;
      EntityType type = circ_type_for_id(&ux->view.circuit, id);
      if (type == TYPE_SYMBOL || type == TYPE_WAYPOINT) {
        arrput(ux->view.selected, id);
      }
    }

    break;
  case UNDO_DESELECT_ITEM:
    log_debug("Performing deselect item: %x", command.selectedID);
    for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
      if (ux->view.selected[i] == command.selectedID) {
        arrdel(ux->view.selected, i);
        break;
      }
    }
    break;
  case UNDO_DESELECT_AREA:
    log_debug(
      "Performing deselect area: %f %f %f %f", command.area.center.X,
      command.area.center.Y, command.area.halfSize.X, command.area.halfSize.Y);
    arrsetlen(ux->view.selected, 0);
    ux->view.selectionBox = (Box){0};
    break;
  case UNDO_ADD_SYMBOL:
    log_debug(
      "Performing add component: %x %d %f %f", command.childID,
      command.parentID, command.center.X, command.center.Y);
    if (!circ_has(&ux->view.circuit, command.childID)) {
      ID id = circ_add_symbol(
        &ux->view.circuit, ux->view.circuit.top, command.parentID);
      circ_set_symbol_position(&ux->view.circuit, id, command.center);
      UndoCommand *top = ux_undo_stack_top(ux);
      top->childID = id;

      // check ports to see if there are endpoints there and reconnect those
      // endpoints to the ports
      // TODO: convert this to new ECS
      // Component *component = circuit_component_ptr(&ux->view.circuit, id);
      // PortID portID = component->portFirst;
      // while (circuit_has(&ux->view.circuit, portID)) {
      //   Port *port = circuit_port_ptr(&ux->view.circuit, portID);
      //   for (size_t i = 0; i < circuit_endpoint_len(&ux->view.circuit); i++)
      //   {
      //     Endpoint *endpoint = &ux->view.circuit.endpoints[i];
      //     HMM_Vec2 portPos = HMM_AddV2(component->box.center,
      //     port->position); float dist = HMM_LenSqr(HMM_SubV2(portPos,
      //     endpoint->position)); if (dist < 0.1) {
      //       endpoint->port = portID;
      //       port->endpoint = circuit_endpoint_id(&ux->view.circuit, i);
      //       break;
      //     }
      //   }
      //   portID = port->next;
      // }
    }

    break;

  case UNDO_DEL_SYMBOL:
    log_debug("Performing del component: %x", command.childID);
    if (circ_has(&ux->view.circuit, command.childID)) {
      // TODO: if adding component, replace it with the component removed
      // and delete the adding component instead
      circ_remove_symbol(&ux->view.circuit, command.childID);
    }
    break;
  case UNDO_ADD_WAYPOINT:
    log_debug(
      "Performing add waypoint: %x %x %f %f", command.childID, command.parentID,
      command.center.X, command.center.Y);
    if (!circ_has(&ux->view.circuit, command.childID)) {
      ID id = circ_add_waypoint(&ux->view.circuit, command.parentID);
      circ_set_waypoint_position(&ux->view.circuit, id, command.center);
      UndoCommand *top = ux_undo_stack_top(ux);
      top->childID = id;
    }
    break;
  case UNDO_DEL_WAYPOINT:
    log_debug("Performing del waypoint: %x", command.childID);
    if (circ_has(&ux->view.circuit, command.childID)) {
      circ_remove_waypoint(&ux->view.circuit, command.childID);
    }
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
        lastCommand->selectedID = command.selectedID;
        return;
      case UNDO_SELECT_AREA:
        lastCommand->area = command.area;
        return;
      case UNDO_DESELECT_ITEM:
        if (lastCommand->selectedID == command.selectedID) {
          return;
        }
        break;
      case UNDO_DESELECT_AREA:
        break;
      case UNDO_ADD_SYMBOL:
        break;
      case UNDO_DEL_SYMBOL:
        break;
      case UNDO_ADD_WAYPOINT:
        break;
      case UNDO_DEL_WAYPOINT:
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
    flip = undo_cmd_move_selection(cmd.newCenter, cmd.oldCenter, cmd.snap);
    break;
  case UNDO_SELECT_ITEM:
    flip = undo_cmd_deselect_item(cmd.selectedID);
    break;
  case UNDO_SELECT_AREA:
    flip = undo_cmd_deselect_area(cmd.area);
    break;
  case UNDO_DESELECT_ITEM:
    flip = undo_cmd_select_item(cmd.selectedID);
    break;
  case UNDO_DESELECT_AREA:
    flip = undo_cmd_select_area(cmd.area);
    break;
  case UNDO_ADD_SYMBOL:
    flip = undo_cmd_del_symbol(cmd.center, cmd.childID, cmd.parentID);
    break;
  case UNDO_DEL_SYMBOL:
    flip = undo_cmd_add_symbol(cmd.center, cmd.childID, cmd.parentID);
    break;
  case UNDO_ADD_WAYPOINT:
    flip = undo_cmd_del_waypoint(cmd.center, cmd.childID, cmd.parentID);
    break;
  case UNDO_DEL_WAYPOINT:
    flip = undo_cmd_add_waypoint(cmd.center, cmd.childID, cmd.parentID);
    break;
  }
  return flip;
}

UndoCommand ux_undo(CircuitUX *ux) {
  // if (arrlen(ux->undoStack) == 0) {
  //   return (UndoCommand){0};
  // }

  // UndoCommand cmd = arrpop(ux->undoStack);

  // // push the opposite of the command to the redo stack
  // UndoCommand redoCmd = ux_flip_command(cmd);
  // arrput(ux->redoStack, redoCmd);

  // ux_perform_command(ux, redoCmd);
  circ_undo(&ux->view.circuit);
  ux_route(ux);
  ux_build_bvh(ux);
  return (UndoCommand){0};
}

UndoCommand ux_redo(CircuitUX *ux) {
  // if (arrlen(ux->redoStack) == 0) {
  //   return (UndoCommand){0};
  // }

  // UndoCommand cmd = arrpop(ux->redoStack);

  // // push the opposite of the command to the undo stack
  // UndoCommand undoCmd = ux_flip_command(cmd);
  // arrput(ux->undoStack, undoCmd);

  // ux_perform_command(ux, undoCmd);
  circ_redo(&ux->view.circuit);
  ux_route(ux);
  ux_build_bvh(ux);
  return (UndoCommand){0};
}
