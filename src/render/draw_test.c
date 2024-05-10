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

// test implementations of draw api

#include "core/core.h"
#include "render/draw.h"

typedef struct Vert {
  int x, y;
} Vert;

typedef struct Color {
  uint8_t r, g, b, a;
} Color;

typedef struct DrawContext {
  arr(char) buildString;

  arr(Vert) verts;

  HMM_Vec2 pan;
  float zoom;
} DrawContext;

DrawContext *draw_create() {
  DrawContext *draw = malloc(sizeof(DrawContext));
  *draw = (DrawContext){
    .pan = HMM_V2(0, 0),
    .zoom = 1.0f,
  };

  return draw;
}
void draw_free(DrawContext *draw) {
  arrfree(draw->buildString);
  arrfree(draw->verts);
  free(draw);
}

char *draw_get_build_string(DrawContext *draw) {
  arrput(draw->buildString, '\0');
  return draw->buildString;
}

void draw_set_zoom(DrawContext *draw, float zoom) { draw->zoom = zoom; }
void draw_add_pan(DrawContext *draw, HMM_Vec2 pan) {
  draw->pan = HMM_AddV2(draw->pan, pan);
}
HMM_Vec2 draw_get_pan(DrawContext *draw) { return draw->pan; }
float draw_get_zoom(DrawContext *draw) { return draw->zoom; }

static int find_vert(DrawContext *draw, HMM_Vec2 pos) {
  Vert vert = {
    .x = (int)(pos.X),
    .y = (int)(pos.Y),
  };
  for (int i = 0; i < arrlen(draw->verts); i++) {
    if (draw->verts[i].x == vert.x && draw->verts[i].y == vert.y) {
      return i;
    }
  }

  int idx = arrlen(draw->verts);
  arrput(draw->verts, vert);
  return idx;
}

const char *shapeStrings[] = {
  [SHAPE_DEFAULT] = "chip", [SHAPE_AND] = "AND", [SHAPE_OR] = "OR",
  [SHAPE_XOR] = "XOR",      [SHAPE_NOT] = "NOT",
};

const char *draw_flags(DrawFlags flags) {
  if (flags == DRAW_HOVERED) {
    return "H";
  }
  if (flags == DRAW_SELECTED) {
    return "S";
  }
  if (flags == (DRAW_HOVERED | DRAW_SELECTED)) {
    return "HS";
  }
  return "-";
}

void draw_component_shape(
  DrawContext *draw, Theme *theme, Box box, ShapeType shape, DrawFlags flags) {
  char buff[256];
  snprintf(
    buff, 256, "component(%s, v%d, %s)\n", shapeStrings[shape],
    find_vert(draw, box.center), draw_flags(flags));

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}
void draw_port(
  DrawContext *draw, Theme *theme, HMM_Vec2 center, DrawFlags flags) {
  char buff[256];
  snprintf(
    buff, 256, "port(v%d, %s)\n", find_vert(draw, center), draw_flags(flags));

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}

void draw_selection_box(
  DrawContext *draw, Theme *theme, Box box, DrawFlags flags) {
  char buff[256];
  snprintf(
    buff, 256, "selection_box(v%d, v%d, %s)\n",
    find_vert(draw, HMM_SubV2(box.center, box.halfSize)),
    find_vert(draw, HMM_AddV2(box.center, box.halfSize)), draw_flags(flags));

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}

void draw_wire(
  DrawContext *draw, Theme *theme, HMM_Vec2 *verts, int numVerts,
  DrawFlags flags) {
  char buff[256];
  snprintf(buff, 256, "wire(");

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }

  for (int i = 0; i < numVerts; i++) {
    snprintf(buff, 256, "v%d, ", find_vert(draw, verts[i]));
    len = strlen(buff);
    for (int i = 0; i < len; i++) {
      arrput(draw->buildString, buff[i]);
    }
  }

  snprintf(buff, 256, "%s)\n", draw_flags(flags));

  len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}

void draw_junction(
  DrawContext *draw, Theme *theme, HMM_Vec2 pos, DrawFlags flags) {
  char buff[256];
  snprintf(
    buff, 256, "junction(v%d, %s)\n", find_vert(draw, pos), draw_flags(flags));

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}

const char *labelStrings[] = {
  [LABEL_COMPONENT_NAME] = "component_name",
  [LABEL_COMPONENT_TYPE] = "component_type",
  [LABEL_PORT] = "port",
  [LABEL_WIRE] = "wire",
};

void draw_label(
  DrawContext *draw, Theme *theme, Box box, const char *text,
  DrawLabelType type, DrawFlags flags) {
  char buff[256];
  snprintf(
    buff, 256, "label(%s, v%d, '%s', %s)\n", labelStrings[type],
    find_vert(draw, box.center), text, draw_flags(flags));

  int len = strlen(buff);
  for (int i = 0; i < len; i++) {
    arrput(draw->buildString, buff[i]);
  }
}

Box draw_text_bounds(
  DrawContext *draw, HMM_Vec2 pos, const char *text, int len, HorizAlign horz,
  VertAlign vert, float fontSize, FontHandle font) {
  return (Box){
    .center = pos,
    .halfSize = HMM_V2(0, 0),
  };
}

// todo: remove this when autoroute doesn't need it anymore
void draw_stroked_line(
  DrawContext *draw, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {}