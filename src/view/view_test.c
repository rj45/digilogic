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

#include "stb_ds.h"
#include "utest.h"

#include "view.h"

typedef enum DrawCmdType {
  DRAW_FILLED_RECT,
  DRAW_STROKED_RECT,
  DRAW_FILLED_CIRCLE,
  DRAW_STROKED_CIRCLE,
  DRAW_STROKED_LINE,
} DrawCmdType;

typedef struct DrawCmd {
  DrawCmdType type;
  HMM_Vec2 position;
  HMM_Vec2 size;
  float radius;
  float line_thickness;
  HMM_Vec4 color;
} DrawCmd;

////////////////////////////////////////
// implementation of the view draw interface
////////////////////////////////////////

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_FILLED_RECT,
    .position = position,
    .size = size,
    .radius = radius,
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_STROKED_RECT,
    .position = position,
    .size = size,
    .radius = radius,
    .line_thickness = line_thickness,
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_FILLED_CIRCLE,
    .position = position,
    .size = size,
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_STROKED_CIRCLE,
    .position = position,
    .size = size,
    .line_thickness = line_thickness,
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_STROKED_LINE,
    .position = start,
    .size = end,
    .line_thickness = line_thickness,
    .color = color,
  };
  arrput(*cmds, cmd);
}