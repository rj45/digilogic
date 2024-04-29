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

#include "avoid/avoid.h"
#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#define MAX_ZOOM 20.0f
#define MOUSE_FUDGE 1.5f

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
  bool overComponent = ux->view.hoveredComponent != NO_COMPONENT;

  MouseDownState oldState = ux->mouseDownState;
  MouseDownState state = oldState;
  for (;;) {
    bool move = leftDown && HMM_LenV2(HMM_Sub(worldMousePos, ux->downStart)) >
                              (10.0f * ux->view.zoom);
    bool selected = arrlen(ux->view.selectedComponents) > 0 ||
                    HMM_LenSqrV2(ux->view.selectionBox.halfSize) > 0.0f;

    bool inSelection =
      box_intersect_point(ux->view.selectionBox, worldMousePos);
    for (size_t i = 0; i < arrlen(ux->view.selectedComponents); i++) {
      ComponentID id = ux->view.selectedComponents[i];
      ComponentView *componentView = &ux->view.components[id];
      if (box_intersect_point(componentView->box, worldMousePos)) {
        inSelection = true;
        break;
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
        } else if (overComponent) {
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
            while (arrlen(ux->view.selectedComponents) > 0) {
              ux_do(
                ux,
                (UndoCommand){
                  .verb = UNDO_DESELECT_COMPONENT,
                  .componentID = ux->view.selectedComponents
                                   [arrlen(ux->view.selectedComponents) - 1],
                });
            }
          }
        }

        if (state == STATE_SELECT_ONE) {
          ux_do(
            ux, (UndoCommand){
                  .verb = UNDO_SELECT_COMPONENT,
                  .componentID = ux->view.hoveredComponent,
                });
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
    HMM_Vec2 delta = HMM_Sub(worldMousePos, ux->downStart);
    ux_do(
      ux, (UndoCommand){
            .verb = UNDO_MOVE_SELECTION,
            .delta = delta,
          });

    break;
  }
  case STATE_SELECT_AREA:
    ux_do(
      ux, (UndoCommand){
            .verb = UNDO_SELECT_AREA,
            .area = box_from_tlbr(ux->downStart, worldMousePos),
          });
    break;
  case STATE_PAN: {
    HMM_Vec2 delta = HMM_Sub(worldMousePos, ux->downStart);
    ux->downStart = worldMousePos;
    ux->view.pan = HMM_Add(ux->view.pan, delta);
    break;
  }
  default:
    break;
  }

  ux->mouseDownState = state;
}

static void ux_handle_mouse(CircuitUX *ux) {
  ux->view.hoveredComponent = NO_COMPONENT;
  ux->view.hoveredPort = NO_PORT;

  HMM_Vec2 worldMousePos =
    HMM_DivV2F(HMM_Sub(ux->input.mousePos, ux->view.pan), ux->view.zoom);

  Box mouseBox = {
    .center = worldMousePos,
    .halfSize = HMM_V2(MOUSE_FUDGE, MOUSE_FUDGE),
  };

  for (size_t i = 0; i < arrlen(ux->view.components); i++) {
    ComponentView *componentView = &ux->view.components[i];
    if (box_intersect_box(componentView->box, mouseBox)) {
      ux->view.hoveredComponent = i;
    }
    for (size_t j = view_port_start(&ux->view, i);
         j < view_port_end(&ux->view, i); j++) {
      PortView *portView = &ux->view.ports[j];
      Box portBox = {
        .center = HMM_Add(portView->center, componentView->box.center),
        .halfSize = HMM_V2(
          ux->view.theme.portWidth / 2.0f, ux->view.theme.portWidth / 2.0f),
      };
      if (box_intersect_box(portBox, mouseBox)) {
        ux->view.hoveredPort = j;
      }
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
  float oldZoom = ux->view.zoom;
  ux->view.zoom = newZoom;

  // figure out where the mouse was in "world coords" with the old zoom
  HMM_Vec2 originalMousePos =
    HMM_DivV2F(HMM_Sub(ux->input.mousePos, ux->view.pan), oldZoom);

  // figure out where the mouse is in "world coords" with the new zoom
  HMM_Vec2 newMousePos =
    HMM_DivV2F(HMM_Sub(ux->input.mousePos, ux->view.pan), newZoom);

  // figure out the correction to the pan so that the zoom is centred on the
  // mouse position
  HMM_Vec2 correction =
    HMM_MulV2F(HMM_Sub(newMousePos, originalMousePos), newZoom);
  ux->view.pan = HMM_Add(ux->view.pan, correction);
}

void ux_draw(CircuitUX *ux, Context ctx) {
  float dt = (float)ux->input.frameDuration;
  if (bv_is_set(ux->input.keysDown, KEYCODE_W)) {
    ux->view.pan.Y -= 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_A)) {
    ux->view.pan.X -= 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_S)) {
    ux->view.pan.Y += 600.0f * dt * ux->view.zoom;
  }
  if (bv_is_set(ux->input.keysDown, KEYCODE_D)) {
    ux->view.pan.X += 600.0f * dt * ux->view.zoom;
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

  if (ux->input.scroll.Y > 0.001 || ux->input.scroll.Y < -0.001) {
    ux_zoom(ux);
  }

  ux_handle_mouse(ux);

  view_draw(&ux->view, ctx);

  // avoid_draw_debug_lines(ux->avoid, ctx, ux->view.zoom, ux->view.pan);
}
