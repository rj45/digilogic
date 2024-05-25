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
  [STATE_CLICK_PORT] = "ClickPort",
  [STATE_DRAG_WIRING] = "DragWiring",
  [STATE_START_CLICK_WIRING] = "StartClickWiring",
  [STATE_CLICK_WIRING] = "ClickWiring",
  [STATE_CONNECT_PORT] = "ConnectPort",
  [STATE_FLOATING_WIRE] = "FloatingWire",
  [STATE_ADDING_COMPONENT] = "AddingComponent",
  [STATE_ADD_COMPONENT] = "AddComponent",
};

static void ux_mouse_down_state_machine(CircuitUX *ux, HMM_Vec2 worldMousePos) {
  bool rightDown = ux->input.modifiers & MODIFIER_RMB;
  bool leftDown = ux->input.modifiers & MODIFIER_LMB;
  bool overPort = circuit_has(&ux->view.circuit, ux->view.hoveredPort);
  bool overItem = circuit_has(&ux->view.circuit, ux->view.hovered);

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
    for (size_t i = 0; i < arrlen(ux->view.selected); i++) {
      ID id = ux->view.selected[i];
      if (id_type(id) == ID_COMPONENT) {
        Component *component = circuit_component_ptr(&ux->view.circuit, id);
        if (box_intersect_point(component->box, worldMousePos)) {
          inSelection = true;
          break;
        }
      } else if (id_type(id) == ID_WAYPOINT) {
        Waypoint *waypoint = circuit_waypoint_ptr(&ux->view.circuit, id);
        if (
          HMM_LenSqrV2(HMM_SubV2(waypoint->position, worldMousePos)) <
          MOUSE_WP_FUDGE) {
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
      if (!leftDown) {
        state = STATE_UP;
      } else if (move) {
        state = STATE_MOVE_SELECTION;
      }
      break;
    case STATE_MOVE_SELECTION:
      if (!leftDown) {
        state = STATE_UP;
      }
      break;
    case STATE_CLICK_PORT:
      if (!leftDown) {
        state = STATE_START_CLICK_WIRING;
      } else if (move) {
        state = STATE_DRAG_WIRING;
      }
      break;
    case STATE_DRAG_WIRING:
      if (overPort && !leftDown) {
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
        break;

      case STATE_CLICK_WIRING:
      case STATE_DRAG_WIRING:
        // rebuild the BVH after wiring things
        ux_build_bvh(ux);
        break;

      case STATE_ADD_COMPONENT: {
        // "drop" the component here and start adding a new one
        Component *component =
          circuit_component_ptr(&ux->view.circuit, ux->addingComponent);
        ComponentDescID descID = component->desc;
        ux_do(
          ux, (UndoCommand){
                .verb = UNDO_ADD_COMPONENT,
                .itemID = ux->addingComponent,
                .descID = descID,
                .newCenter = component->box.center,
              });
        ux_start_adding_component(ux, descID);

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
          ux_do(
            ux, (UndoCommand){
                  .verb = UNDO_DESELECT_AREA,
                  .area = ux->view.selectionBox,
                });
        } else {
          if (
            state == STATE_DESELECT ||
            (ux->input.modifiers & MODIFIER_SHIFT) == 0) {
            while (arrlen(ux->view.selected) > 0) {
              ux_do(
                ux,
                (UndoCommand){
                  .verb = UNDO_DESELECT_ITEM,
                  .itemID = ux->view.selected[arrlen(ux->view.selected) - 1],
                });
            }
          }
        }

        if (state == STATE_SELECT_ONE) {
          ux_do(
            ux, (UndoCommand){
                  .verb = UNDO_SELECT_ITEM,
                  .itemID = ux->view.hovered,
                });
          ux->selectionCenter = ux_calc_selection_center(ux);
        }
        break;

      case STATE_CLICK_PORT:
        ux->clickedPort = ux->view.hoveredPort;
        break;

      case STATE_START_CLICK_WIRING:
      case STATE_DRAG_WIRING:
        if (circuit_has(&ux->view.circuit, ux->clickedPort)) {
          ux_start_wire(ux, ux->clickedPort);
          ux->clickedPort = NO_PORT;
        }
        break;

      case STATE_CONNECT_PORT:
        if (circuit_has(&ux->view.circuit, ux->view.hoveredPort)) {
          ux_connect_wire(ux, ux->view.hoveredPort);
        }
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
        ux, (UndoCommand){
              .verb = UNDO_MOVE_SELECTION,
              .oldCenter = oldCenter,
              .newCenter = newCenter,
              .snap = (ux->input.modifiers & MODIFIER_CTRL) == 0,
            });
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

    ux_do(
      ux, (UndoCommand){
            .verb = UNDO_SELECT_AREA,
            .area = area,
          });
    break;
  }

  case STATE_PAN: {
    HMM_Vec2 delta = HMM_SubV2(worldMousePos, ux->downStart);
    draw_add_pan(ux->view.drawCtx, delta);
    break;
  }

  case STATE_ADDING_COMPONENT:
    circuit_move_component_to(
      &ux->view.circuit, ux->addingComponent, worldMousePos);
    break;

  case STATE_DRAG_WIRING:
  case STATE_CLICK_WIRING:
    circuit_move_endpoint_to(&ux->view.circuit, ux->endpointEnd, worldMousePos);
    ux_route(ux);
    break;

  default:
    break;
  }

  ux->mouseDownState = state;
}

static void ux_handle_mouse(CircuitUX *ux) {
  ux->view.hovered = NO_ID;
  ux->view.hoveredPort = NO_PORT;

  HMM_Vec2 worldMousePos =
    draw_screen_to_world(ux->view.drawCtx, ux->input.mousePos);

  Box mouseBox = {
    .center = worldMousePos,
    .halfSize = HMM_V2(MOUSE_FUDGE, MOUSE_FUDGE),
  };

  arrsetlen(ux->view.hovered2, 0);
  ux->view.hovered2 = bvh_query(&ux->bvh, mouseBox, ux->view.hovered2);

  for (size_t i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
    Component *component = &ux->view.circuit.components[i];
    if (box_intersect_box(component->box, mouseBox)) {
      ux->view.hovered = circuit_component_id(&ux->view.circuit, i);
    }
    PortID portID = ux->view.circuit.components[i].portFirst;
    while (circuit_has(&ux->view.circuit, portID)) {
      Port *port = circuit_port_ptr(&ux->view.circuit, portID);
      Box portBox = {
        .center = HMM_AddV2(port->position, component->box.center),
        .halfSize = HMM_V2(
          ux->view.theme.portWidth / 2.0f, ux->view.theme.portWidth / 2.0f),
      };
      if (box_intersect_box(portBox, mouseBox)) {
        ux->view.hoveredPort = portID;
      }

      portID = circuit_port_ptr(&ux->view.circuit, portID)->next;
    }
  }

  for (size_t i = 0; i < circuit_waypoint_len(&ux->view.circuit); i++) {
    Waypoint *waypoint = &ux->view.circuit.waypoints[i];
    Box waypointBox = {
      .center = waypoint->position,
      .halfSize = HMM_V2(MOUSE_WP_FUDGE, MOUSE_WP_FUDGE),
    };
    if (box_intersect_box(waypointBox, mouseBox)) {
      ux->view.hovered = circuit_waypoint_id(&ux->view.circuit, i);
    }
  }

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

  // cmd + y or ctrl + y: redo (common on windows / linux)
  if (bv_is_set(ux->input.keysPressed, KEYCODE_Y)) {
    if (
      ux->input.modifiers & MODIFIER_CTRL ||
      ux->input.modifiers & MODIFIER_SUPER) {
      ux_redo(ux);
    }
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_SPACE)) {
    ux->rtDebugLines = !ux->rtDebugLines;
    ux->view.debugMode = ux->rtDebugLines;
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_B)) {
    ux->routingConfig.minimizeGraph = !ux->routingConfig.minimizeGraph;
    printf(
      "Minimize routing graph: %s\n",
      ux->routingConfig.minimizeGraph ? "on" : "off");
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_C)) {
    ux->routingConfig.performCentering = !ux->routingConfig.performCentering;
    printf(
      "Perform Centering: %s\n",
      ux->routingConfig.performCentering ? "on" : "off");
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_V)) {
    ux->bvhDebugLines = !ux->bvhDebugLines;
    printf("BVH debug lines: %s\n", ux->bvhDebugLines ? "on" : "off");
    if (ux->bvhDebugLines) {
      ux_build_bvh(ux);
    }
  }

  if (ux->bvhDebugLines) {
    if (bv_is_set(ux->input.keysPressed, KEYCODE_COMMA)) {
      ux->bvhDebugLevel--;
      if (ux->bvhDebugLevel < 0) {
        ux->bvhDebugLevel = 0;
      }
      printf("BVH debug level: %d\n", ux->bvhDebugLevel);
    } else if (bv_is_set(ux->input.keysPressed, KEYCODE_PERIOD)) {
      ux->bvhDebugLevel++;
      printf("BVH debug level: %d\n", ux->bvhDebugLevel);
    }
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_F3)) {
    ux->showFPS = true;
  }

  if (ux->input.scroll.Y > 0.001 || ux->input.scroll.Y < -0.001) {
    ux_zoom(ux);
  }

  ux_handle_mouse(ux);
}

void ux_start_adding_component(CircuitUX *ux, ComponentDescID descID) {
  ux->mouseDownState = STATE_ADDING_COMPONENT;
  ux->addingComponent =
    circuit_add_component(&ux->view.circuit, descID, HMM_V2(0, 0));
}

void ux_stop_adding_component(CircuitUX *ux) {
  ux->mouseDownState = STATE_UP;
  circuit_del(&ux->view.circuit, ux->addingComponent);
  ux->addingComponent = NO_COMPONENT;
}

void ux_change_adding_component(CircuitUX *ux, ComponentDescID descID) {
  ux_stop_adding_component(ux);
  ux_start_adding_component(ux, descID);
}

void ux_start_wire(CircuitUX *ux, PortID portID) {
  Port *port = circuit_port_ptr(&ux->view.circuit, portID);
  NetID netID;
  if (circuit_has(&ux->view.circuit, port->endpoint)) {
    Endpoint *endpoint =
      circuit_endpoint_ptr(&ux->view.circuit, port->endpoint);
    netID = endpoint->net;
    ux->endpointStart = port->endpoint;
    ux->newNet = false;
  } else {
    netID = circuit_add_net(&ux->view.circuit);
    ux->endpointStart =
      circuit_add_endpoint(&ux->view.circuit, netID, portID, port->position);
    ux->newNet = true;
  }
  ux->endpointEnd =
    circuit_add_endpoint(&ux->view.circuit, netID, NO_PORT, HMM_V2(0, 0));
}

void ux_cancel_wire(CircuitUX *ux) {
  circuit_del(&ux->view.circuit, ux->endpointEnd);
  if (ux->newNet) {
    Endpoint *endpoint =
      circuit_endpoint_ptr(&ux->view.circuit, ux->endpointStart);
    NetID netID = endpoint->net;
    circuit_del(&ux->view.circuit, ux->endpointStart);
    circuit_del(&ux->view.circuit, netID);
  }
  ux->newNet = false;
  ux->endpointStart = NO_ENDPOINT;
  ux->endpointEnd = NO_ENDPOINT;
  ux_route(ux);
}

void ux_connect_wire(CircuitUX *ux, PortID portID) {
  Port *port = circuit_port_ptr(&ux->view.circuit, portID);
  if (circuit_has(&ux->view.circuit, port->endpoint)) {
    log_error("TODO: add merging of nets. For now, cancelling wire");
    ux_cancel_wire(ux);
    return;
  }

  circuit_endpoint_connect(&ux->view.circuit, ux->endpointEnd, portID);
  ux_route(ux);

  ux->newNet = false;
  ux->endpointStart = NO_ENDPOINT;
  ux->endpointEnd = NO_ENDPOINT;
}
