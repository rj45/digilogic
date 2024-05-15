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

#include "autoroute/autoroute.h"
#include "core/core.h"
#include "handmade_math.h"
#include "render/draw.h"
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#define MAX_ZOOM 20.0f
#define MOUSE_FUDGE 1.5f
#define MOUSE_WP_FUDGE 5.0f
#define MOVE_THRESHOLD 5.0f

/* Enter this into mermaid.live:
    stateDiagram
        [*] --> Up : !down
        Up --> Down : down & !overComp & !overPort & !inSel
        Down --> Click : !down & !sel
        Down --> Desel : !down & sel
        Desel --> [*]
        Down --> SelArea : move & !sel
        SelArea --> [*]
        Up --> MoveSel : down & inSel
        MoveSel --> [*]
        SelOne --> MoveSel : move
        SelOne --> [*]
        Click --> [*]
        ConnectPort --> [*]
        Up --> SelOne : down & overComp & !overPort & !inSel
        Up --> ClickPort : down & overPort & !inSel
        ClickPort --> DragWiring : move
        ClickPort --> ClickWiring : !down
        DragWiring --> ConnectPort : overPort & !down
        DragWiring --> FloatingWire : !overPort & !down
        ClickWiring --> ConnectPort : overPort & down
        ClickWiring --> FloatingWire : !overPort & down
        FloatingWire --> [*]
*/
static void ux_mouse_down_state_machine(CircuitUX *ux, HMM_Vec2 worldMousePos) {
  bool rightDown = ux->input.modifiers & MODIFIER_RMB;
  bool leftDown = ux->input.modifiers & MODIFIER_LMB;
  bool overPort = ux->view.hoveredPort != NO_PORT;
  bool overItem = ux->view.hovered != NO_ID;

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
        ComponentView *componentView = view_component_ptr(&ux->view, id);
        if (box_intersect_point(componentView->box, worldMousePos)) {
          inSelection = true;
          break;
        }
      } else if (id_type(id) == ID_WAYPOINT) {
        WaypointView *waypointView = view_waypoint_ptr(&ux->view, id);
        if (
          HMM_LenSqrV2(HMM_SubV2(waypointView->pos, worldMousePos)) <
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
      if (leftDown) {
        state = STATE_CLICK_WIRING;
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
        break;
      }
    case STATE_FLOATING_WIRE:
      if (!leftDown) {
        state = STATE_UP;
        break;
      }
    }

    if (state != oldState) {
      // process exit state actions here
      switch (oldState) {
      case STATE_UP:
        ux->downStart = worldMousePos;
        break;

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

  for (size_t i = 0; i < circuit_component_len(&ux->view.circuit); i++) {
    ComponentView *componentView = &ux->view.components[i];
    if (box_intersect_box(componentView->box, mouseBox)) {
      ux->view.hovered = circuit_component_id(&ux->view.circuit, i);
    }
    PortID portID = ux->view.circuit.components[i].portFirst;
    while (portID != NO_PORT) {
      PortView *portView = view_port_ptr(&ux->view, portID);
      Box portBox = {
        .center = HMM_AddV2(portView->center, componentView->box.center),
        .halfSize = HMM_V2(
          ux->view.theme.portWidth / 2.0f, ux->view.theme.portWidth / 2.0f),
      };
      if (box_intersect_box(portBox, mouseBox)) {
        ux->view.hoveredPort = portID;
      }

      portID = circuit_port_ptr(&ux->view.circuit, portID)->compNext;
    }
  }

  for (size_t i = 0; i < circuit_waypoint_len(&ux->view.circuit); i++) {
    WaypointView *waypointView = &ux->view.waypoints[i];
    Box waypointBox = {
      .center = waypointView->pos,
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
    ux->debugLines = !ux->debugLines;
    ux->view.debugMode = ux->debugLines;
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_B)) {
    ux->betterRoutes = !ux->betterRoutes;
    printf("Better (minimal) routes: %s\n", ux->betterRoutes ? "on" : "off");
  }

  if (bv_is_set(ux->input.keysPressed, KEYCODE_F3)) {
    ux->showFPS = true;
  }

  if (ux->input.scroll.Y > 0.001 || ux->input.scroll.Y < -0.001) {
    ux_zoom(ux);
  }

  ux_handle_mouse(ux);
}

void ux_draw(CircuitUX *ux) {
  view_draw(&ux->view);

  if (ux->debugLines) {
    autoroute_draw_debug_lines(ux->router, ux->view.drawCtx);
  }
}
