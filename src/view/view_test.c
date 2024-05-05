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
#include "utest.h"

#include "view.h"
#include <Security/Security.h>
#include <stdbool.h>

typedef enum DrawCmdType {
  DRAW_FILLED_RECT,
  DRAW_STROKED_RECT,
  DRAW_FILLED_CIRCLE,
  DRAW_STROKED_CIRCLE,
  DRAW_FILLED_ARC,
  DRAW_STROKED_ARC,
  DRAW_STROKED_LINE,
  DRAW_STROKED_CURVE,
  DRAW_TEXT,
} DrawCmdType;

typedef struct DrawCmd {
  DrawCmdType type;
  HMM_Vec2 position;
  HMM_Vec2 size;
  float radius;
  float line_thickness;
  HMM_Vec4 color;
  char *text;
  HMM_Vec4 bgColor;
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

void draw_filled_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_FILLED_ARC,
    .position = position,
    .size = HMM_V2(radius, radius),
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_stroked_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  float line_thickness, HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_STROKED_ARC,
    .position = position,
    .size = HMM_V2(radius, radius),
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

void draw_stroked_curve(
  Context ctx, HMM_Vec2 a, HMM_Vec2 ctrl0, HMM_Vec2 ctrl1, HMM_Vec2 b,
  float line_thickness, HMM_Vec4 color) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_STROKED_CURVE,
    .position = a,
    .size = b,
    .line_thickness = line_thickness,
    .color = color,
  };
  arrput(*cmds, cmd);
}

void draw_text(
  Context ctx, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor) {
  DrawCmd **cmds = (DrawCmd **)ctx;
  DrawCmd cmd = {
    .type = DRAW_TEXT,
    .position = HMM_SubV2(rect.center, rect.halfSize),
    .size = HMM_MulV2F(rect.halfSize, 2),
    .text = strndup(text, len),
    .color = fgColor,
    .bgColor = bgColor,
  };
  arrput(*cmds, cmd);
}

Box draw_text_bounds(
  HMM_Vec2 pos, const char *text, int len, HorizAlign horz, VertAlign vert,
  float fontSize, FontHandle font) {

  float ascender = -1.069000f * fontSize;
  float descender = 0.293000f * fontSize;
  float width = 0.987464f * fontSize * len;
  float height = 1.362000f * fontSize;
  HMM_Vec2 center = pos;
  switch (horz) {
  case ALIGN_LEFT:
    center.X += width / 2;
    break;
  case ALIGN_CENTER:
    break;
  case ALIGN_RIGHT:
    center.X -= width / 2;
    break;
  }
  // correct for baseline
  center.Y -= height / 2 - descender;
  switch (vert) {
  case ALIGN_TOP:
    center.Y += ascender;
    break;
  case ALIGN_MIDDLE:
    center.Y += ascender / 2;
    break;
  case ALIGN_BOTTOM:
    break;
  }
  return (Box){.center = center, .halfSize = HMM_V2(width / 2, height / 2)};
}

////////////////////////////////////////

const char *drawNames[] = {
  [DRAW_FILLED_RECT] = "filled_rect",
  [DRAW_STROKED_RECT] = "stroked_rect",
  [DRAW_FILLED_CIRCLE] = "filled_circle",
  [DRAW_STROKED_CIRCLE] = "stroked_circle",
  [DRAW_FILLED_ARC] = "filled_arc",
  [DRAW_STROKED_ARC] = "stroked_arc",
  [DRAW_STROKED_LINE] = "stroked_line",
  [DRAW_STROKED_CURVE] = "stroked_curve",
  [DRAW_TEXT] = "text",
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

    uint8_t r = (uint8_t)(cmd.color.R * 255);
    uint8_t g = (uint8_t)(cmd.color.G * 255);
    uint8_t b = (uint8_t)(cmd.color.B * 255);
    uint8_t a = (uint8_t)(cmd.color.A * 255);
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

    r = (uint8_t)(cmd.bgColor.R * 255);
    g = (uint8_t)(cmd.bgColor.G * 255);
    b = (uint8_t)(cmd.bgColor.B * 255);
    a = (uint8_t)(cmd.bgColor.A * 255);
    int bgcolorid = -1;
    for (int j = 0; j < arrlen(colors); j++) {
      if (
        colors[j].r == r && colors[j].g == g && colors[j].b == b &&
        colors[j].a == a) {
        bgcolorid = j;
        break;
      }
    }
    if (bgcolorid < 0) {
      Color colorName = {r, g, b, a};
      bgcolorid = arrlen(colors);
      arrput(colors, colorName);
    }

    if (cmd.type == DRAW_STROKED_LINE) {
      start += snprintf(
        start, end - start, "%s v%d v%d c%d\n", drawNames[cmd.type], vertid,
        vertid2, colorid);
      continue;
    }

    if (cmd.type == DRAW_TEXT) {
      start += snprintf(
        start, end - start, "%s '%s' v%d fg c%d bg c%d\n", drawNames[cmd.type],
        cmd.text, vertid, colorid, bgcolorid);
      free(cmd.text);
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

  view_init(&view, circuit_component_descs(), NULL);

  view_add_component(&view, COMP_AND, HMM_V2(100, 100));
  view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  ASSERT_EQ(circuit_component_len(&view.circuit), 2);
  ASSERT_EQ(view.components[0].box.center.X, 100);
  ASSERT_EQ(view.components[0].box.center.Y, 100);
  ASSERT_EQ(view.components[1].box.center.X, 200);
  ASSERT_EQ(view.components[1].box.center.Y, 200);

  view_free(&view);
}

UTEST(View, view_draw_components) {
  CircuitView view = {0};
  DrawCmd *cmds = NULL;

  view_init(&view, circuit_component_descs(), NULL);

  view_add_component(&view, COMP_OR, HMM_V2(100, 100));

  view_draw(&view, (Context)&cmds);

  ASSERT_DRAW(
    "filled_rect v0 c0\n"
    "stroked_rect v0 c2\n"
    "text 'OR' v1 fg c3 bg c1\n"
    "text 'X1' v2 fg c2 bg c1\n"
    "filled_circle v3 c2\n"
    "stroked_circle v3 c4\n"
    "text 'A' v4 fg c3 bg c1\n"
    "filled_circle v5 c2\n"
    "stroked_circle v5 c4\n"
    "text 'B' v6 fg c3 bg c1\n"
    "filled_circle v7 c2\n"
    "stroked_circle v7 c4\n"
    "text 'Y' v8 fg c3 bg c1\n",
    cmds);

  view_free(&view);
  arrfree(cmds);
}

UTEST(View, view_draw_component_with_wires) {
  CircuitView view = {0};
  DrawCmd *cmds = NULL;

  view_init(&view, circuit_component_descs(), NULL);
  ComponentID and = view_add_component(&view, COMP_AND, HMM_V2(100, 100));
  ComponentID or = view_add_component(&view, COMP_OR, HMM_V2(200, 200));

  Component *andComp = circuit_component_ptr(&view.circuit, and);
  PortID from = circuit_port_ptr(
                  &view.circuit,
                  circuit_port_ptr(&view.circuit, andComp->portFirst)->compNext)
                  ->compNext;

  Component *orComp = circuit_component_ptr(&view.circuit, or);
  PortID to = orComp->portFirst;

  NetID net = view_add_net(&view);
  view_add_wire(&view, net, from, to);

  view_draw(&view, (Context)&cmds);

  ASSERT_DRAW(
    "stroked_line v0 v1 c0\n"
    "stroked_line v2 v3 c0\n"
    "stroked_line v4 v5 c0\n"
    "stroked_curve v6 c0\n"
    "stroked_curve v7 c0\n"
    "text 'X1' v8 fg c0 bg c1\n"
    "filled_circle v9 c0\n"
    "stroked_circle v9 c2\n"
    "filled_circle v10 c0\n"
    "stroked_circle v10 c2\n"
    "filled_circle v11 c0\n"
    "stroked_circle v11 c2\n"
    "filled_rect v12 c3\n"
    "stroked_rect v12 c0\n"
    "text 'OR' v13 fg c4 bg c1\n"
    "text 'X2' v14 fg c0 bg c1\n"
    "filled_circle v15 c0\n"
    "stroked_circle v15 c2\n"
    "text 'A' v16 fg c4 bg c1\n"
    "filled_circle v17 c0\n"
    "stroked_circle v17 c2\n"
    "text 'B' v18 fg c4 bg c1\n"
    "filled_circle v19 c0\n"
    "stroked_circle v19 c2\n"
    "text 'Y' v20 fg c4 bg c1\n",
    cmds);

  view_free(&view);
  arrfree(cmds);
}