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

#include <assert.h>

#include "render/polyline.h"
#include "render/render.h"

#include "sokol_gfx.h"
#include "sokol_gp.h"

static inline sgp_vec2 mat2x3_vec2_mul(const sgp_mat2x3 *m, const sgp_vec2 *v) {
  sgp_vec2 u = {
    m->v[0][0] * v->x + m->v[0][1] * v->y + m->v[0][2],
    m->v[1][0] * v->x + m->v[1][1] * v->y + m->v[1][2]};
  return u;
}

// static inline sgp_vec2
// mat2x3_vec2_scale(const sgp_mat2x3 *m, const sgp_vec2 *v) {
//   sgp_vec2 u = {
//     m->v[0][0] * v->x + m->v[0][1] * v->y,
//     m->v[1][0] * v->x + m->v[1][1] * v->y};
//   return u;
// }

static inline float mat2x3_det(const sgp_mat2x3 *m) {
  return m->v[0][0] * m->v[1][1] - m->v[0][1] * m->v[1][0];
}

static inline sgp_mat2x3 mat2x3_invert(const sgp_mat2x3 *m) {
  float det = mat2x3_det(m);
  assert(det != 0.0f); // "sgp_mat2x3_invert: matrix is not invertible"

  sgp_mat2x3 inv = {
    .v =
      {
        {
          m->v[1][1] / det,
          -m->v[0][1] / det,
          (-m->v[1][1] * m->v[0][2] + m->v[0][1] * m->v[1][2]) / det,
        },
        {
          -m->v[1][0] / det,
          m->v[0][0] / det,
          (m->v[1][0] * m->v[0][2] - m->v[0][0] * m->v[1][2]) / det,
        },
      },
  };

  return inv;
}

void draw_init(Draw *draw, FONScontext *fontstash) {
  *draw = (Draw){.fontstash = fontstash, .polyliner = pl_create()};
}

void draw_free(Draw *draw) { pl_free(draw->polyliner); }

HMM_Vec2 draw_screen_to_world(HMM_Vec2 screenPos) {
  sgp_mat2x3 xform = sgp_query_state()->transform;
  sgp_mat2x3 inv = mat2x3_invert(&xform);
  sgp_vec2 screen = {screenPos.X, screenPos.Y};
  sgp_vec2 world = mat2x3_vec2_mul(&inv, &screen);
  return HMM_V2(world.x, world.y);
}

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  sgp_set_color(color.R, color.G, color.B, color.A);
  sgp_draw_filled_rect(position.X, position.Y, size.X, size.Y);
}

void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  Draw *draw = ctx;
  sgp_set_color(color.R, color.G, color.B, color.A);
  pl_reset(draw->polyliner);
  pl_cap_style(draw->polyliner, LC_JOINT);
  pl_thickness(draw->polyliner, line_thickness);
  pl_start(draw->polyliner, HMM_V2(position.X, position.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X + size.X, position.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X + size.X, position.Y + size.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X, position.Y + size.Y));
  pl_finish(draw->polyliner);
}

void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  draw_filled_rect(ctx, position, size, 0.0f, color);
}

void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  draw_stroked_rect(ctx, position, size, 0.0f, line_thickness, color);
}

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  Draw *draw = ctx;
  sgp_set_color(color.R, color.G, color.B, color.A);
  pl_reset(draw->polyliner);
  pl_thickness(draw->polyliner, line_thickness);
  pl_cap_style(draw->polyliner, LC_SQUARE);
  pl_start(draw->polyliner, start);
  pl_lineto(draw->polyliner, end);
  pl_finish(draw->polyliner);
}

void draw_text(
  Context ctx, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor) {
  FonsFont *f = (FonsFont *)font;
  FONScontext *fsctx = f->fsctx;

  // top left corner of rect
  HMM_Vec2 dot = box_top_left(rect);

  // position dot in bottom left corner of rect
  dot.Y += rect.halfSize.Y * 2;

  fonsPushState(fsctx);
  fonsSetSize(fsctx, fontSize);
  fonsSetColor(
    fsctx, fsgp_rgba(
             (uint8_t)(fgColor.R * 255.0f), (uint8_t)(fgColor.G * 255.0f),
             (uint8_t)(fgColor.B * 255.0f), (uint8_t)(fgColor.A * 255.0f)));
  fonsSetAlign(fsctx, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);
  if (len > 0 && text[0] < ' ') {
    fonsSetFont(fsctx, f->iconFont);
  } else {
    fonsSetFont(fsctx, f->mainFont);
  }

  fonsDrawText(fsctx, dot.X, dot.Y, text, text + len);

  fonsPopState(fsctx);
}

Box draw_text_bounds(
  HMM_Vec2 pos, const char *text, int len, HorizAlign horz, VertAlign vert,
  float fontSize, FontHandle font) {
  FonsFont *f = (FonsFont *)font;
  FONScontext *fsctx = f->fsctx;

  fonsPushState(fsctx);
  int align = 0;
  switch (horz) {
  case ALIGN_LEFT:
    align |= FONS_ALIGN_LEFT;
    break;
  case ALIGN_CENTER:
    align |= FONS_ALIGN_CENTER;
    break;
  case ALIGN_RIGHT:
    align |= FONS_ALIGN_RIGHT;
    break;
  }
  switch (vert) {
  case ALIGN_TOP:
    align |= FONS_ALIGN_TOP;
    break;
  case ALIGN_MIDDLE:
    align |= FONS_ALIGN_MIDDLE;
    break;
  case ALIGN_BOTTOM:
    align |= FONS_ALIGN_BOTTOM;
    break;
  }
  fonsSetAlign(fsctx, align);
  fonsSetSize(fsctx, fontSize);

  if (len > 0 && text[0] < ' ') {
    fonsSetFont(fsctx, f->iconFont);
  } else {
    fonsSetFont(fsctx, f->mainFont);
  }

  float bounds[4];

  fonsTextBounds(fsctx, pos.X, pos.Y, text, text + len, bounds);

  fonsPopState(fsctx);

  HMM_Vec2 halfSize =
    HMM_V2((bounds[2] - bounds[0]) / 2.0f, (bounds[3] - bounds[1]) / 2.0f);
  HMM_Vec2 center = HMM_V2(bounds[0] + halfSize.X, bounds[1] + halfSize.Y);

  return (Box){.center = center, .halfSize = halfSize};
}
