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

#ifndef VIEW_H
#define VIEW_H

#include "core/core.h"
#include "render/draw.h"

typedef struct CircuitView {
  Circuit circuit;
  Circuit2 circuit2;

  Theme theme;
  DrawContext *drawCtx;

  arr(BVHLeaf) hovered;
  arr(ID) selected;

  PortID selectedPort;

  Box selectionBox;

  bool debugMode;
} CircuitView;

typedef void *Context;

void view_init(
  CircuitView *view, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font);
void view_free(CircuitView *view);

// wires all endpoints in the net together in a star pattern,
// mainly only useful in tests
void view_direct_wire_nets(CircuitView *view);

void view_draw(CircuitView *view);

Box view_label_size(
  CircuitView *view, const char *text, HMM_Vec2 pos, HorizAlign horz,
  VertAlign vert, float fontSize);

#endif // VIEW_H