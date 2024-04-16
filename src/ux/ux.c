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

#define MAX_ZOOM 20.0f
#define MOUSE_FUDGE 1.5f

void ux_init(
  CircuitUX *ux, const ComponentDesc *componentDescs, FontHandle font) {
  *ux = (CircuitUX){
    .avoid = avoid_new(),
  };
  bv_setlen(ux->input.keysDown, KEYCODE_MENU + 1);
  bv_setlen(ux->input.keysPressed, KEYCODE_MENU + 1);
  bv_clear_all(ux->input.keysDown);
  view_init(&ux->view, componentDescs, font);
}

void ux_free(CircuitUX *ux) {
  view_free(&ux->view);
  bv_free(ux->input.keysDown);
  avoid_free(ux->avoid);
}

ComponentID
ux_add_component(CircuitUX *ux, ComponentDescID descID, HMM_Vec2 position) {
  ComponentID id = view_add_component(&ux->view, descID, position);
  Component *component = &ux->view.circuit.components[id];
  Box box = ux->view.components[id].box;
  const ComponentDesc *desc = &ux->view.circuit.componentDescs[component->desc];

  avoid_add_node(
    ux->avoid, id, box.center.X - box.halfSize.X, box.center.Y - box.halfSize.Y,
    box.halfSize.X * 2.0f, box.halfSize.Y * 2.0f);

  PortID portStart = view_port_start(&ux->view, id);
  PortID portEnd = view_port_end(&ux->view, id);

  for (PortID portID = portStart; portID < portEnd; portID++) {
    PortView portView = ux->view.ports[portID];
    PortDirection dir = desc->ports[portID - portStart].direction;
    PortSide side = SIDE_LEFT;
    if (dir != PORT_IN) {
      side = SIDE_RIGHT;
    }
    HMM_Vec2 center = HMM_Add(portView.center, box.halfSize);
    avoid_add_port(ux->avoid, portID, id, side, center.X, center.Y);
  }
  return id;
}

NetID ux_add_net(CircuitUX *ux, PortID portFrom, PortID portTo) {
  NetID id = view_add_net(&ux->view, portFrom, portTo);

  Port *portFromPtr = &ux->view.circuit.ports[portFrom];
  Port *portToPtr = &ux->view.circuit.ports[portTo];
  avoid_add_edge(
    ux->avoid, id, portFromPtr->component, portFrom, portToPtr->component,
    portTo);

  return id;
}

void ux_add_vertex(CircuitUX *ux, NetID net, HMM_Vec2 vertex) {
  view_add_vertex(&ux->view, net, vertex);
}

void ux_rem_vertex(CircuitUX *ux, NetID net) {
  view_rem_vertex(&ux->view, net);
}

void ux_set_vertex(CircuitUX *ux, NetID net, VertexID index, HMM_Vec2 pos) {
  view_set_vertex(&ux->view, net, index, pos);
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
  bool down = ux->input.modifiers & MODIFIER_LMB;
  bool overPort = ux->view.hoveredPort != NO_PORT;
  bool overComponent = ux->view.hoveredComponent != NO_COMPONENT;

  MouseDownState oldState = ux->mouseDownState;
  MouseDownState state = oldState;
  for (;;) {
    bool move = down && HMM_LenV2(HMM_Sub(worldMousePos, ux->downStart)) >
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
      if (down) {
        if (inSelection) {
          state = STATE_MOVE_SELECTION;
        } else if (overPort) {
          state = STATE_CLICK_PORT;
        } else if (overComponent) {
          state = STATE_SELECT_ONE;
        } else {
          state = STATE_DOWN;
        }
      }
      break;
    case STATE_DOWN:
      if (!down) {
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
      if (!down) {
        state = STATE_UP;
      }
      break;
    case STATE_DESELECT:
      if (!down) {
        state = STATE_UP;
      }
      break;
    case STATE_SELECT_AREA:
      if (!down) {
        state = STATE_UP;
      }
      break;
    case STATE_SELECT_ONE:
      if (!down) {
        state = STATE_UP;
      } else if (move) {
        state = STATE_MOVE_SELECTION;
      }
      break;
    case STATE_MOVE_SELECTION:
      if (!down) {
        state = STATE_UP;
      }
      break;
    case STATE_CLICK_PORT:
      if (down) {
        state = STATE_CLICK_WIRING;
      } else if (move) {
        state = STATE_DRAG_WIRING;
      }
      break;
    case STATE_DRAG_WIRING:
      if (overPort && !down) {
        state = STATE_CONNECT_PORT;
      } else if (!overPort && !down) {
        state = STATE_FLOATING_WIRE;
      }
      break;
    case STATE_CLICK_WIRING:
      if (down) {
        if (overPort) {
          state = STATE_CONNECT_PORT;
        } else if (!overPort) {
          state = STATE_FLOATING_WIRE;
        }
      }
      break;
    case STATE_CONNECT_PORT:
      if (!down) {
        state = STATE_UP;
        break;
      }
    case STATE_FLOATING_WIRE:
      if (!down) {
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
}

void ux_route(CircuitUX *ux) {
  avoid_route(ux->avoid);

  float coords[1024];

  for (int i = 0; i < arrlen(ux->view.nets); i++) {
    NetView *netView = &ux->view.nets[i];

    size_t len = avoid_get_edge_path(
      ux->avoid, i, coords, sizeof(coords) / sizeof(coords[0]));
    len /= 2;

    if (len <= 2) {
      continue;
    }

    int curSize = (netView->vertexEnd - netView->vertexStart) + 2;

    while (curSize < len) {
      ux_add_vertex(ux, i, HMM_V2(0, 0));
      curSize++;
    }
    while (curSize > len && curSize > 2) {
      ux_rem_vertex(ux, i);
      curSize--;
    }
    for (int j = 0; j < len - 2; j++) {
      ux_set_vertex(
        ux, i, j, HMM_V2(coords[(j + 1) * 2], coords[(j + 1) * 2 + 1]));
    }
  }
}

static void ux_perform_command(CircuitUX *ux, UndoCommand command) {
  switch (command.verb) {
  case UNDO_NONE:
    break;
  case UNDO_MOVE_SELECTION: {
    for (size_t i = 0; i < arrlen(ux->view.selectedComponents); i++) {
      ComponentID id = ux->view.selectedComponents[i];
      ComponentView *componentView = &ux->view.components[id];
      componentView->box.center =
        HMM_Add(componentView->box.center, command.delta);
      avoid_move_node(ux->avoid, id, command.delta.X, command.delta.Y);
    }
    ux_route(ux);
    ux->view.selectionBox.center =
      HMM_AddV2(ux->view.selectionBox.center, command.delta);
    ux->downStart = HMM_AddV2(ux->downStart, command.delta);
    break;
  }
  case UNDO_SELECT_COMPONENT:
    arrput(ux->view.selectedComponents, command.componentID);
    break;
  case UNDO_SELECT_AREA:
    ux->view.selectionBox = command.area;
    arrsetlen(ux->view.selectedComponents, 0);
    for (size_t i = 0; i < arrlen(ux->view.components); i++) {
      ComponentView *componentView = &ux->view.components[i];
      if (box_intersect_box(componentView->box, command.area)) {
        arrput(ux->view.selectedComponents, i);
      }
    }
    break;
  case UNDO_DESELECT_COMPONENT:
    for (size_t i = 0; i < arrlen(ux->view.selectedComponents); i++) {
      if (ux->view.selectedComponents[i] == command.componentID) {
        arrdel(ux->view.selectedComponents, i);
        break;
      }
    }
    break;
  case UNDO_DESELECT_AREA:
    arrsetlen(ux->view.selectedComponents, 0);
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
        lastCommand->delta = HMM_Add(lastCommand->delta, command.delta);
        return;
      case UNDO_SELECT_COMPONENT:
        lastCommand->componentID = command.componentID;
        return;
      case UNDO_SELECT_AREA:
        lastCommand->area = command.area;
        return;
      case UNDO_DESELECT_COMPONENT:
        if (lastCommand->componentID == command.componentID) {
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
    flip.delta = HMM_V2(-cmd.delta.X, -cmd.delta.Y);
    break;
  case UNDO_SELECT_COMPONENT:
    flip.verb = UNDO_DESELECT_COMPONENT;
    flip.componentID = cmd.componentID;
    break;
  case UNDO_SELECT_AREA:
    flip.verb = UNDO_DESELECT_AREA;
    flip.area = cmd.area;
    break;
  case UNDO_DESELECT_COMPONENT:
    flip.verb = UNDO_SELECT_COMPONENT;
    flip.componentID = cmd.componentID;
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
