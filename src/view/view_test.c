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

UTEST(View, view_add_component) {
  CircuitView view = {0};

  view_init(&view, circuit_component_descs());

  view_add_component(&view, COMP_AND, HMM_V2(100, 100));
  view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  ASSERT_EQ(arrlen(view.components), 2);
  ASSERT_EQ(view.components[0].position.X, 100);
  ASSERT_EQ(view.components[0].position.Y, 100);
  ASSERT_EQ(view.components[1].position.X, 200);
  ASSERT_EQ(view.components[1].position.Y, 200);

  view_free(&view);
}

UTEST(View, view_draw_components) {
  CircuitView view = {0};
  DrawCmd *cmds = NULL;

  view_init(&view, circuit_component_descs());

  view_add_component(&view, COMP_AND, HMM_V2(100, 100));

  view_draw(&view, (Context)&cmds);

  ASSERT_EQ(arrlen(cmds), 8);
  ASSERT_EQ(cmds[0].type, DRAW_FILLED_RECT);
  ASSERT_EQ(cmds[0].position.X, 100);
  ASSERT_EQ(cmds[0].position.Y, 100);
  ASSERT_EQ(cmds[1].type, DRAW_STROKED_RECT);
  ASSERT_EQ(cmds[1].position.X, 100);
  ASSERT_EQ(cmds[1].position.Y, 100);
  ASSERT_EQ(cmds[2].type, DRAW_FILLED_CIRCLE);
  ASSERT_EQ(cmds[3].type, DRAW_STROKED_CIRCLE);
  ASSERT_EQ(cmds[4].type, DRAW_FILLED_CIRCLE);
  ASSERT_EQ(cmds[5].type, DRAW_STROKED_CIRCLE);
  ASSERT_EQ(cmds[6].type, DRAW_FILLED_CIRCLE);
  ASSERT_EQ(cmds[7].type, DRAW_STROKED_CIRCLE);

  view_free(&view);
  arrfree(cmds);
}