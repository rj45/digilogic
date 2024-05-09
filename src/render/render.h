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

#ifndef RENDER_H
#define RENDER_H

#include "render/fons_sgp.h"
#include "render/polyline.h"
#include "view/view.h"

typedef void *Context;
typedef void *FontHandle;

typedef struct FonsFont {
  FONScontext *fsctx;
  int mainFont;
  int iconFont;
} FonsFont;

// typedef enum VertAlign {
//   ALIGN_TOP,
//   ALIGN_MIDDLE,
//   ALIGN_BOTTOM,
// } VertAlign;

// typedef enum HorizAlign {
//   ALIGN_LEFT,
//   ALIGN_CENTER,
//   ALIGN_RIGHT,
// } HorizAlign;

typedef struct Draw {
  PolyLiner *polyliner;
  FONScontext *fontstash;
} Draw;

void draw_init(Draw *draw, FONScontext *fontstash);
void draw_free(Draw *draw);

HMM_Vec2 draw_screen_to_world(HMM_Vec2 screenPos);

// void draw_filled_rect(
//   Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4
//   color);
// void draw_stroked_rect(
//   Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
//   float line_thickness, HMM_Vec4 color);
// void draw_filled_circle(
//   Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color);
// void draw_stroked_circle(
//   Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
//   HMM_Vec4 color);
// void draw_stroked_line(
//   Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
//   HMM_Vec4 color);
// void draw_text(
//   Context ctx, Box rect, const char *text, int len, float fontSize,
//   FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor);
// Box draw_text_bounds(
//   HMM_Vec2 pos, const char *text, int len, HorizAlign horz, VertAlign vert,
//   float fontSize, FontHandle font);

#endif // RENDER_H