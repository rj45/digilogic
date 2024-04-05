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
#include <Security/Security.h>
#include <stdbool.h>

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

////////////////////////////////////////

const char *drawNames[] = {
  [DRAW_FILLED_RECT] = "filled_rect",
  [DRAW_STROKED_RECT] = "stroked_rect",
  [DRAW_FILLED_CIRCLE] = "filled_circle",
  [DRAW_STROKED_CIRCLE] = "stroked_circle",
  [DRAW_STROKED_LINE] = "stroked_line",
};

typedef struct Vert {
  int x, y;
} Vert;

typedef struct Color {
  uint8_t r, g, b, a;
} Color;

// dumps the draw commands to a string naming the vertices and colors.
// This abstracts the specifics of draw commands that don't matter too much,
// and allows things that do matter (like relationships between vertices and
// colors) to be tested.
char *dumpDrawCmds(char *start, char *end, DrawCmd *cmds) {
  Vert *verts = NULL;
  Color *colors = NULL;
  for (int i = 0; i < arrlen(cmds); i++) {
    DrawCmd cmd = cmds[i];
    int x = (int)(cmd.position.X + cmd.size.X / 2);
    int y = (int)(cmd.position.Y + cmd.size.Y / 2);

    if (cmd.type == DRAW_STROKED_LINE) {
      x = (int)(cmd.position.X);
      y = (int)(cmd.position.Y);
    }

    int vertid = -1;
    for (int j = 0; j < arrlen(verts); j++) {
      if (verts[j].x == x && verts[j].y == y) {
        vertid = j;
        break;
      }
    }
    if (vertid < 0) {
      Vert vertName = {x, y};
      vertid = arrlen(verts);
      arrput(verts, vertName);
    }

    int vertid2 = -1;
    if (cmd.type == DRAW_STROKED_LINE) {
      x = (int)(cmd.size.X);
      y = (int)(cmd.size.Y);
      for (int j = 0; j < arrlen(verts); j++) {
        if (verts[j].x == x && verts[j].y == y) {
          vertid2 = j;
          break;
        }
      }
      if (vertid2 < 0) {
        Vert vertName = {x, y};
        vertid2 = arrlen(verts);
        arrput(verts, vertName);
      }
    }

    uint8_t r = (uint8_t)(cmd.color.X * 255);
    uint8_t g = (uint8_t)(cmd.color.Y * 255);
    uint8_t b = (uint8_t)(cmd.color.Z * 255);
    uint8_t a = (uint8_t)(cmd.color.W * 255);
    int colorid = -1;
    for (int j = 0; j < arrlen(colors); j++) {
      if (
        colors[j].r == r && colors[j].g == g && colors[j].b == b &&
        colors[j].a == a) {
        colorid = j;
        break;
      }
    }
    if (colorid < 0) {
      Color colorName = {r, g, b, a};
      colorid = arrlen(colors);
      arrput(colors, colorName);
    }

    if (cmd.type == DRAW_STROKED_LINE) {
      start += snprintf(
        start, end - start, "%s v%d v%d c%d\n", drawNames[cmd.type], vertid,
        vertid2, colorid);
      continue;
    }

    start += snprintf(
      start, end - start, "%s v%d c%d\n", drawNames[cmd.type], vertid, colorid);
  }

  arrfree(verts);
  arrfree(colors);
  return start;
}

#define ASSERT_DRAW(expected, cmds)                                            \
  do {                                                                         \
    char buffer[8192];                                                         \
    dumpDrawCmds(buffer, buffer + sizeof(buffer), cmds);                       \
    ASSERT_STREQ(expected, buffer);                                            \
  } while (0);

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

  ASSERT_DRAW(
    "filled_rect v0 c0\n"
    "stroked_rect v0 c1\n"
    "filled_circle v1 c1\n"
    "stroked_circle v1 c2\n"
    "filled_circle v2 c1\n"
    "stroked_circle v2 c2\n"
    "filled_circle v3 c1\n"
    "stroked_circle v3 c2\n",
    cmds);

  view_free(&view);
  arrfree(cmds);
}

UTEST(View, view_draw_component_with_wires) {
  CircuitView view = {0};
  DrawCmd *cmds = NULL;

  view_init(&view, circuit_component_descs());
  ComponentID and = view_add_component(&view, COMP_AND, HMM_V2(100, 100));
  ComponentID or = view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  PortID from = view_port_start(&view, and) + 2;
  PortID to = view_port_start(&view, or);

  view_add_net(&view, from, to);

  view_draw(&view, (Context)&cmds);

  ASSERT_DRAW(
    "filled_rect v0 c0\n"
    "stroked_rect v0 c1\n"
    "filled_circle v1 c1\n"
    "stroked_circle v1 c2\n"
    "filled_circle v2 c1\n"
    "stroked_circle v2 c2\n"
    "filled_circle v3 c1\n"
    "stroked_circle v3 c2\n"
    "filled_rect v4 c0\n"
    "stroked_rect v4 c1\n"
    "filled_circle v5 c1\n"
    "stroked_circle v5 c2\n"
    "filled_circle v6 c1\n"
    "stroked_circle v6 c2\n"
    "filled_circle v7 c1\n"
    "stroked_circle v7 c2\n"
    "stroked_line v3 v5 c3\n",
    cmds);

  view_free(&view);
  arrfree(cmds);
}