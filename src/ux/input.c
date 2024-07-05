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
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

#define MAX_ZOOM 20.0f
#define MOUSE_FUDGE 3.0f
#define MOUSE_WP_FUDGE 5.0f
#define MOVE_THRESHOLD 5.0f

static const char *stateNames[] = {
  [STATE_UP] = "Up",
  [STATE_DOWN] = "Down",
  [STATE_PAN] = "Pan",
  [STATE_CLICK] = "Click",
  [STATE_DESELECT] = "Desel",
  [STATE_SELECT_AREA] = "SelArea",
  [STATE_SELECT_ONE] = "SelOne",
  [STATE_MOVE_SELECTION] = "MoveSel",
  [STATE_CLICK_ENDPOINT] = "ClickEndpoint",
  [STATE_CLICK_PORT] = "ClickPort",
  [STATE_DRAG_WIRING] = "DragWiring",
  [STATE_START_CLICK_WIRING] = "StartClickWiring",
  [STATE_CLICK_WIRING] = "ClickWiring",
  [STATE_CONNECT_PORT] = "ConnectPort",
  [STATE_FLOATING_WIRE] = "FloatingWire",
  [STATE_CANCEL_WIRE] = "CancelWire",
  [STATE_ADDING_COMPONENT] = "AddingComponent",
  [STATE_ADD_COMPONENT] = "AddComponent",
  [STATE_ADDING_WAYPOINT] = "AddingWaypoint",
  [STATE_ADD_WAYPOINT] = "AddWaypoint",
};

static void ux_mouse_down_state_machine(CircuitUX *ux, HMM_Vec2 worldMousePos) {
  bool rightDown = ux->input.modifiers & MODIFIER_RMB;
  bool leftDown = ux->input.modifiers & MODIFIER_LMB;
  bool shiftDown = ux->input.modifiers & MODIFIER_SHIFT;
  bool cancelDown = bv_is_set(ux->input.keysDown, KEYCODE_ESCAPE) ||
                    bv_is_set(ux->input.keysDown, KEYCODE_BACKSPACE) ||
                    bv_is_set(ux->input.keysDown, KEYCODE_DELETE);

  bool overPort = false;
  bool overItem = false;
  bool overEndpoint = false;
  bool overNet = false;

  for (size_t i = 0; i < arrlen(ux->view.hovered); i++) {
    ID id = ux->view.hovered[i].subitem;
    if (!circ_has(&ux->view.circuit, id)) {
      id = ux->view.hovered[i].item;
    }

    EntityType type = circ_type_for_id(&ux->view.circuit, id);
    if (type == TYPE_PORT) {
      overPort = true;
    } else if (type == TYPE_SYMBOL || type == TYPE_WAYPOINT) {
      overItem = true;
    } else if (type == TYPE_ENDPOINT) {
      overEndpoint = true;
    } else if (type == TYPE_NET) {
      overNet = true;
    }
  }

  MouseDownState oldState = ux->mouseDownState;
  MouseDownState state = oldState;
  for (;;) {
    bool move =
      leftDown && HMM_LenV2(HMM_SubV2(worldMousePos, ux->downStart)) >
                    (MOVE_THRESHOLD / draw_get_zoom(ux->view.drawCtx));
    bool selected = arrlen(ux->view.selected) > 0 ||
                    HMM_LenSqrV2(ux->view.selectionBox.halfSize) > 0.0f;

    bool inSelection =
      box_intersect_point(ux->view.selectionBox, worldMousePos);
    for (size_t i = 0; i < arrlen(ux->view.hovered); i++) {
      for (size_t j = 0; j < arrlen(ux->view.selected); j++) {
        if (ux->view.hovered[i].item == ux->view.selected[j]) {
          inSelection = true;
          break;
        }
      }
    }

    // process the state transitions -- do not put actions here, see the next
    // switch statement
    switch (state) {
    case STATE_UP:
      if (leftDown) {
        if (inSelection) {
          state = STATE_MOVE_SELECTION;
        } else if (overEndpoint) {
          if (shiftDown && overPort) {
            state = STATE_CLICK_PORT;
          } else {
            state = STATE_CLICK_ENDPOINT;
          }
        } else if (overPort) {
          state = STATE_CLICK_PORT;
        } else if (overItem) {
          state = STATE_SELECT_ONE;
        } else {
          state = STATE_DOWN;
        }
      } else if (rightDown) {
        state = STATE_PAN;
      }
      break;
    case STATE_PAN:
      if (!rightDown) {
        state = STATE_UP;
      }
      break;
    case STATE_DOWN:
      if (!leftDown) {
        if (selected) {
          state = STATE_DESELECT;
        } else {
          state = STATE_CLICK;
        }
      } else if (move && !selected) {
        state = STATE_SELECT_AREA;
      }
      break;
    case STATE_CLICK:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_DESELECT:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_SELECT_AREA:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_SELECT_ONE:
      state = STATE_MOVE_SELECTION;
      break;
    case STATE_MOVE_SELECTION:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_CLICK_ENDPOINT: // fallthrough
    case STATE_CLICK_PORT:
      if (!leftDown) {
        state = STATE_START_CLICK_WIRING;
      } else if (move) {
        state = STATE_DRAG_WIRING;
      }
      break;
    case STATE_DRAG_WIRING:
      if (cancelDown) {
        state = STATE_CANCEL_WIRE;
      } else if (overPort && !leftDown) {
        state = STATE_CONNECT_PORT;
      } else if (!overPort && !leftDown) {
        state = STATE_FLOATING_WIRE;
      }
      break;
    case STATE_START_CLICK_WIRING:
      if (!leftDown) {
        state = STATE_CLICK_WIRING;
      }
      break;
    case STATE_CLICK_WIRING:
      if (leftDown) {
        if (overPort) {
          state = STATE_CONNECT_PORT;
        } else if (!overPort) {
          state = STATE_FLOATING_WIRE;
        }
      } else if (cancelDown) {
        state = STATE_CANCEL_WIRE;
      }
      break;
    case STATE_CONNECT_PORT:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_FLOATING_WIRE:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_CANCEL_WIRE:
      if (!leftDown && !cancelDown) {
        state = STATE_UP;
      }
      break;
    case STATE_ADDING_COMPONENT:
      if (leftDown) {
        state = STATE_ADD_COMPONENT;
      }
      break;
    case STATE_ADD_COMPONENT:
      if (!leftDown) {
        state = STATE_ADDING_COMPONENT;
      }
      break;
    case STATE_ADDING_WAYPOINT:
      if (leftDown && overNet && !overEndpoint && !overItem) {
        state = STATE_ADD_WAYPOINT;
      }
    case STATE_ADD_WAYPOINT:
      if (!leftDown) {
        state = STATE_ADDING_WAYPOINT;
      }
      break;
    }

    if (state != oldState) {
      log_debug(
        "State transition: %s -> %s", stateNames[oldState], stateNames[state]);

      // process exit state actions here
      switch (oldState) {
      case STATE_UP:
        ux->downStart = worldMousePos;
        break;

      case STATE_MOVE_SELECTION:
        // rebuild the BVH after moving things
        ux_build_bvh(ux);
        circ_commit(&ux->view.circuit);
        break;

      case STATE_CLICK_WIRING:
      case STATE_DRAG_WIRING:
        // rebuild the BVH after wiring things
        ux_build_bvh(ux);
        break;

      case STATE_ADD_COMPONENT: {
        // "drop" the symbol here and start adding a new one
        SymbolKindID kindID =
          circ_get(&ux->view.circuit, ux->addingSymbol, SymbolKindID);
        Position pos = circ_get(&ux->view.circuit, ux->addingSymbol, Position);
        ux_do(ux, undo_cmd_add_symbol(pos, ux->addingSymbol, kindID));
        circ_commit(&ux->view.circuit);
        ux_start_adding_symbol(ux, kindID);

        // rebuild the BVH after adding things
        ux_build_bvh(ux);
      } break;

      default:
        break;
      }

      // process enter state actions here
      switch (state) {
      case STATE_SELECT_ONE: // fallthrough
      case STATE_DESELECT:
        if (HMM_LenSqrV2(ux->view.selectionBox.halfSize) > 0.001f) {
          ux_do(ux, undo_cmd_deselect_area(ux->view.selectionBox));
        } else {
          if (
            state == STATE_DESELECT ||
            (ux->input.modifiers & MODIFIER_SHIFT) == 0) {
            while (arrlen(ux->view.selected) > 0) {
              ux_do(
                ux, undo_cmd_deselect_item(
                      ux->view.selected[arrlen(ux->view.selected) - 1]));
            }
          }
        }

        if (state == STATE_SELECT_ONE) {
          static const EntityType typePriority[] = {
            TYPE_SYMBOL,
            TYPE_WAYPOINT,
          };
          ID found = NO_ID;
          for (size_t i = 0; found == NO_ID && i < 2; i++) {
            EntityType type = typePriority[i];
            for (size_t j = 0; j < arrlen(ux->view.hovered); j++) {
              ID id = ux->view.hovered[j].item;
              if (circ_type_for_id(&ux->view.circuit, id) == type) {
                found = id;
                break;
              }
            }
          }
          if (found != NO_ID) {
            ux_do(ux, undo_cmd_select_item(found));
            ux->selectionCenter = ux_calc_selection_center(ux);
          }
        }
        break;

      case STATE_CLICK_ENDPOINT:
        for (size_t i = 0; i < arrlen(ux->view.hovered); i++) {
          ID id = ux->view.hovered[i].item;
          if (circ_type_for_id(&ux->view.circuit, id) == TYPE_ENDPOINT) {
            ux_continue_wire(ux, id);
            break;
          }
        }
        break;

      case STATE_CLICK_PORT:
        for (size_t i = 0; i < arrlen(ux->view.hovered); i++) {
          ID id = ux->view.hovered[i].subitem;
          if (circ_type_for_id(&ux->view.circuit, id) == TYPE_PORT) {
            ux->clickedPort =
              (PortRef){.symbol = ux->view.hovered[i].item, .port = id};
            break;
          }
        }
        break;

      case STATE_START_CLICK_WIRING:
      case STATE_DRAG_WIRING:
        if (circ_has(&ux->view.circuit, ux->clickedPort.port)) {
          ux_start_wire(ux, ux->clickedPort);
          ux->clickedPort = (PortRef){0};
        }
        break;

      case STATE_CONNECT_PORT:
        for (size_t i = 0; i < arrlen(ux->view.hovered); i++) {
          ID id = ux->view.hovered[i].subitem;
          if (circ_type_for_id(&ux->view.circuit, id) == TYPE_PORT) {
            ux_connect_wire(
              ux, (PortRef){.symbol = ux->view.hovered[i].item, .port = id});
            ux_route(ux);
            circ_commit(&ux->view.circuit);
            break;
          }
        }
        break;

      case STATE_CANCEL_WIRE:
        ux_cancel_wire(ux);

        ux_route(ux);
        circ_commit(&ux->view.circuit);

        // rebuild the BVH after removing things
        ux_build_bvh(ux);
        break;

      case STATE_ADD_WAYPOINT:
        log_debug("add waypoint");
        ux_add_waypoint(ux, worldMousePos);
        ux_route(ux);
        ux_build_bvh(ux);
        circ_commit(&ux->view.circuit);
        break;

      default:
        break;
      }

      oldState = state;
      continue;
    }

    break;
  }

  // handle continuous update state actions here
  switch (state) {
  case STATE_MOVE_SELECTION: {
    HMM_Vec2 delta = HMM_SubV2(worldMousePos, ux->downStart);
    HMM_Vec2 oldCenter = ux->selectionCenter;
    HMM_Vec2 newCenter = HMM_AddV2(oldCenter, delta);
    if (HMM_LenSqrV2(delta) > 0.01f) {
      ux_do(
        ux,
        undo_cmd_move_selection(
          oldCenter, newCenter, (ux->input.modifiers & MODIFIER_CTRL) == 0));
    }

    break;
  }

  case STATE_SELECT_AREA: {
    Box area = box_from_tlbr(ux->downStart, worldMousePos);
    if (arrlen(ux->view.selected) > 0) {
      ux->selectionCenter = ux_calc_selection_center(ux);
    } else {
      ux->selectionCenter = area.center;
    }

    ux_do(ux, undo_cmd_select_area(area));
    break;
  }

  case STATE_PAN: {
    HMM_Vec2 delta = HMM_SubV2(worldMousePos, ux->downStart);
    draw_add_pan(ux->view.drawCtx, delta);
    break;
  }

  case STATE_ADDING_COMPONENT:
    circ_set_symbol_position(
      &ux->view.circuit, ux->addingSymbol, worldMousePos);
    break;

  case STATE_DRAG_WIRING:
  case STATE_CLICK_WIRING:
    circ_set_endpoint_position(
      &ux->view.circuit, ux->endpointEnd, worldMousePos);
    ux_route(ux);
    break;

  default:
    break;
  }

  ux->mouseDownState = state;
}

static void ux_handle_mouse(CircuitUX *ux) {
  HMM_Vec2 worldMousePos =
    draw_screen_to_world(ux->view.drawCtx, ux->input.mousePos);

  Box mouseBox = {
    .center = worldMousePos,
    .halfSize = HMM_V2(MOUSE_FUDGE, MOUSE_FUDGE),
  };

  arrsetlen(ux->view.hovered, 0);
  ux->view.hovered = bvh_query(&ux->bvh, mouseBox, ux->view.hovered);

  ux_mouse_down_state_machine(ux, worldMousePos);
}

static void ux_zoom(CircuitUX *ux) {
  // calculate the new zoom
  ux->zoomExp += ux->input.scroll.Y * 0.5f;
  if (ux->zoomExp < -MAX_ZOOM) {
    ux->zoomExp = -MAX_ZOOM;
  } else if (ux->zoomExp > MAX_ZOOM) {
    ux->zoomExp = MAX_ZOOM;
  }
  float newZoom = powf(1.1f, ux->zoomExp);

  // figure out where the mouse was in "world coords" with the old zoom
  HMM_Vec2 originalMousePos =
    draw_screen_to_world(ux->view.drawCtx, ux->input.mousePos);

  draw_set_zoom(ux->view.drawCtx, newZoom);

  // figure out where the mouse is in "world coords" with the new zoom
  HMM_Vec2 newMousePos =
    draw_screen_to_world(ux->view.drawCtx, ux->input.mousePos);

  // figure out the correction to the pan so that the zoom is centred on the
  // mouse position
  HMM_Vec2 correction = HMM_SubV2(newMousePos, originalMousePos);
  draw_add_pan(ux->view.drawCtx, correction);
}

#define WASD_PIXELS_PER_SECOND 1000.0f

void ux_update(CircuitUX *ux) {
  float dt = (float)ux->input.frameDuration;
  HMM_Vec2 panDelta = HMM_V2(0, 0);
  if (bv_is_set(ux->input.keysDown, KEYCODE_W)) {
    panDelta.Y += WASD_PIXELS_PER_SECOND * dt;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_A)) {
    panDelta.X += WASD_PIXELS_PER_SECOND * dt;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_S)) {
    panDelta.Y -= WASD_PIXELS_PER_SECOND * dt;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_D)) {
    panDelta.X -= WASD_PIXELS_PER_SECOND * dt;
  }
  if (panDelta.X != 0 || panDelta.Y != 0) {
    HMM_Vec2 adjustedDelta =
      draw_scale_screen_to_world(ux->view.drawCtx, panDelta);
    draw_add_pan(ux->view.drawCtx, adjustedDelta);
  }

  // cmd + z or ctrl + z: undo
  // cmd + shift + z or ctrl + shift + z: redo (common on macos)
  if (bv_is_set(ux->input.keysPressed, KEYCODE_Z)) {
    if (
      ux->input.modifiers & MODIFIER_CTRL ||
      ux->input.modifiers & MODIFIER_SUPER) {
      if (ux->input.modifiers & MODIFIER_SHIFT) {
        ux_redo(ux);
      } else {
        ux_undo(ux);
      }
    }
  }

  // ctrl + y: redo (common on windows / linux)
  if (bv_is_set(ux->input.keysPressed, KEYCODE_Y)) {
    if (ux->input.modifiers & MODIFIER_CTRL) {
      ux_redo(ux);
    }
  }

  if (
    bv_is_set(ux->input.keysPressed, KEYCODE_DELETE) ||
    bv_is_set(ux->input.keysPressed, KEYCODE_BACKSPACE)) {
    ux_delete_selected(ux);
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_SPACE)) {
    ux->rtDebugLines = !ux->rtDebugLines;
    ux->view.debugMode = ux->rtDebugLines;
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_B)) {
    ux->routingConfig.minimizeGraph = !ux->routingConfig.minimizeGraph;
    log_info(
      "Minimize routing graph: %s",
      ux->routingConfig.minimizeGraph ? "on" : "off");
    ux_route(ux);
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_C)) {
    ux->routingConfig.performCentering = !ux->routingConfig.performCentering;
    log_info(
      "Perform Centering: %s",
      ux->routingConfig.performCentering ? "on" : "off");
    ux_route(ux);
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_V)) {
    ux->bvhDebugLines = !ux->bvhDebugLines;
    log_info("BVH debug lines: %s", ux->bvhDebugLines ? "on" : "off");
    if (ux->bvhDebugLines) {
      ux_build_bvh(ux);
    }
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_X)) {
    autoroute_dump_routing_data(
      ux->router, ux->routingConfig, "routing_data.dat");
    log_info("Dumped routing data to routing_data.dat");
  }

  if (ux->bvhDebugLines) {
    if (bv_is_set(ux->input.keysPressed, KEYCODE_COMMA)) {
      ux->bvhDebugLevel--;
      if (ux->bvhDebugLevel < 0) {
        ux->bvhDebugLevel = 0;
      }
      log_info("BVH debug level: %d", ux->bvhDebugLevel);
    } else if (bv_is_set(ux->input.keysPressed, KEYCODE_PERIOD)) {
      ux->bvhDebugLevel++;
      log_info("BVH debug level: %d", ux->bvhDebugLevel);
    }
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_F3)) {
    ux->showFPS = !ux->showFPS;
  }

  if (ux->input.scroll.Y > 0.001 || ux->input.scroll.Y < -0.001) {
    ux_zoom(ux);
  }

  ux_handle_mouse(ux);
}

void ux_start_adding_waypoint(CircuitUX *ux) {
  ux->mouseDownState = STATE_ADDING_WAYPOINT;
}

void ux_stop_adding_waypoint(CircuitUX *ux) {
  if (
    ux->mouseDownState == STATE_ADDING_WAYPOINT ||
    ux->mouseDownState == STATE_ADD_WAYPOINT) {
    ux->mouseDownState = STATE_UP;
  }
}

void ux_start_adding_symbol(CircuitUX *ux, ID symbolKindID) {
  ux->mouseDownState = STATE_ADDING_COMPONENT;
  ux->addingSymbol =
    circ_add_symbol(&ux->view.circuit, ux->view.circuit.top, symbolKindID);
}

void ux_stop_adding_symbol(CircuitUX *ux) {
  ux->mouseDownState = STATE_UP;
  circ_remove_symbol(&ux->view.circuit, ux->addingSymbol);
  ux->addingSymbol = NO_ID;
}

void ux_change_adding_symbol(CircuitUX *ux, ID symbolKindID) {
  ux_stop_adding_symbol(ux);
  ux_start_adding_symbol(ux, symbolKindID);
}

void ux_start_wire(CircuitUX *ux, PortRef portRef) {
  ID moduleID = circ_get(&ux->view.circuit, portRef.symbol, Parent);
  ID netlistID = circ_get(&ux->view.circuit, moduleID, NetlistID);

  LinkedListIter netit = circ_lliter(&ux->view.circuit, netlistID);
  while (circ_lliter_next(&netit)) {
    ID netID = netit.current;
    LinkedListIter subnetit = circ_lliter(&ux->view.circuit, netID);
    while (circ_lliter_next(&subnetit)) {
      ID subnetID = subnetit.current;
      LinkedListIter endpointit = circ_lliter(&ux->view.circuit, subnetID);
      while (circ_lliter_next(&endpointit)) {
        ID endpointID = endpointit.current;
        PortRef endpointPortRef =
          circ_get(&ux->view.circuit, endpointID, PortRef);
        if (
          endpointPortRef.port == portRef.port &&
          endpointPortRef.symbol == portRef.symbol) {
          ux->endpointStart = endpointID;
          ux->newNet = false;
          ux->endpointEnd = circ_add_endpoint(&ux->view.circuit, subnetID);
          return;
        }
      }
    }
  }

  ux->newNet = true;
  ID netID = circ_add_net(&ux->view.circuit, ux->view.circuit.top);
  ID subnetID = circ_add_subnet(&ux->view.circuit, netID);
  ux->endpointStart = circ_add_endpoint(&ux->view.circuit, subnetID);
  circ_connect_endpoint_to_port(
    &ux->view.circuit, ux->endpointStart, portRef.symbol, portRef.port);
  ux->endpointEnd = circ_add_endpoint(&ux->view.circuit, subnetID);
}

void ux_continue_wire(CircuitUX *ux, ID endpointID) {
  ux->newNet = false;
  ux->endpointStart = NO_ID;
  ux->endpointEnd = endpointID;

  PortRef portRef = circ_get(&ux->view.circuit, endpointID, PortRef);
  if (circ_has(&ux->view.circuit, portRef.port)) {
    circ_disconnect_endpoint_from_port(&ux->view.circuit, endpointID);
  }
  ID subnetID = circ_get(&ux->view.circuit, endpointID, Parent);
  ID netID = circ_get(&ux->view.circuit, subnetID, Parent);
  int count = 0;
  ID otherEndpointID = NO_ID;
  LinkedListIter subnetit = circ_lliter(&ux->view.circuit, netID);
  while (circ_lliter_next(&subnetit)) {
    LinkedListIter endpointit =
      circ_lliter(&ux->view.circuit, subnetit.current);
    while (circ_lliter_next(&endpointit)) {
      if (endpointit.current != endpointID) {
        otherEndpointID = endpointit.current;
      }
      count++;
    }
  }

  if (count <= 2) {
    ux->newNet = true;
    ux->endpointStart = otherEndpointID;
  }
}

void ux_cancel_wire(CircuitUX *ux) {

  if (ux->newNet) {
    ID subnetID = circ_get(&ux->view.circuit, ux->endpointEnd, Parent);
    ID netID = circ_get(&ux->view.circuit, subnetID, Parent);

    // endpoints and subnet are recursively removed
    circ_remove_net(&ux->view.circuit, netID);
  } else {
    circ_remove_endpoint(&ux->view.circuit, ux->endpointEnd);
  }

  ux->newNet = false;
  ux->endpointStart = NO_ID;
  ux->endpointEnd = NO_ID;
}

void ux_connect_wire(CircuitUX *ux, PortRef portRef) {
  circ_connect_endpoint_to_port(
    &ux->view.circuit, ux->endpointEnd, portRef.symbol, portRef.port);

  ux->newNet = false;
  ux->endpointStart = NO_ID;
  ux->endpointEnd = NO_ID;
}
