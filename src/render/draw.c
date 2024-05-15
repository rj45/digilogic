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

#include "render/draw.h"
#include <assert.h>

#include "core/core.h"
#include "render/polyline.h"
#include "render/render.h"

#include "sokol_gfx.h"
#include "sokol_gp.h"

static inline HMM_Vec2 mat2x3_vec2_mul(const sgp_mat2x3 *m, HMM_Vec2 v) {
  return HMM_V2(
    m->v[0][0] * v.X + m->v[0][1] * v.Y + m->v[0][2],
    m->v[1][0] * v.X + m->v[1][1] * v.Y + m->v[1][2]);
}

static inline HMM_Vec2 mat2x3_vec2_scale(const sgp_mat2x3 *m, HMM_Vec2 v) {
  return HMM_V2(
    m->v[0][0] * v.X + m->v[0][1] * v.Y, m->v[1][0] * v.X + m->v[1][1] * v.Y);
}

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

void draw_init(DrawContext *draw, FONScontext *fontstash) {
  *draw = (DrawContext){
    .pan = HMM_V2(0.0f, 0.0f),
    .zoom = 1.0f,
    .fontstash = fontstash,
    .polyliner = pl_create(),
    .transform =
      (sgp_mat2x3){
        .v =
          {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
          },
      },
  };
}

void draw_free(DrawContext *draw) { pl_free(draw->polyliner); }

HMM_Vec2 draw_screen_to_world(DrawContext *draw, HMM_Vec2 screenPos) {
  sgp_mat2x3 xform = draw->transform;
  sgp_mat2x3 inv = mat2x3_invert(&xform);
  return mat2x3_vec2_mul(&inv, screenPos);
}

HMM_Vec2 draw_scale_screen_to_world(DrawContext *draw, HMM_Vec2 dirvec) {
  sgp_mat2x3 xform = draw->transform;
  sgp_mat2x3 inv = mat2x3_invert(&xform);
  return mat2x3_vec2_scale(&inv, dirvec);
}

HMM_Vec2 draw_world_to_screen(DrawContext *draw, HMM_Vec2 worldPos) {
  sgp_mat2x3 xform = draw->transform;
  return mat2x3_vec2_mul(&xform, worldPos);
}

HMM_Vec2 draw_scale_world_to_screen(DrawContext *draw, HMM_Vec2 dirvec) {
  sgp_mat2x3 xform = draw->transform;
  return mat2x3_vec2_scale(&xform, dirvec);
}

void draw_push_transform(DrawContext *draw) {
  sgp_push_transform();
  sgp_reset_transform();
  sgp_scale(draw->zoom, draw->zoom);
  sgp_translate(draw->pan.X, draw->pan.Y);
}

void draw_pop_transform(DrawContext *draw) { sgp_pop_transform(); }

void draw_set_zoom(DrawContext *draw, float zoom) {
  draw->zoom = zoom;

  draw_push_transform(draw);
  draw->transform = sgp_query_state()->transform;
  draw_pop_transform(draw);
}

void draw_add_pan(DrawContext *draw, HMM_Vec2 pan) {
  draw->pan = HMM_AddV2(draw->pan, pan);

  draw_push_transform(draw);
  draw->transform = sgp_query_state()->transform;
  draw_pop_transform(draw);
}

HMM_Vec2 draw_get_pan(DrawContext *draw) { return draw->pan; }

float draw_get_zoom(DrawContext *draw) { return draw->zoom; }

void draw_begin_frame(DrawContext *draw) {
  draw->strokedRects = 0;
  draw->filledRects = 0;
  draw->lineVertices = 0;
  draw->texts = 0;

  draw_push_transform(draw);
}

void draw_end_frame(DrawContext *draw) { draw_pop_transform(draw); }

void draw_filled_rect(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, float radius,
  HMM_Vec4 color) {
  sgp_set_color(color.R, color.G, color.B, color.A);
  sgp_draw_filled_rect(position.X, position.Y, size.X, size.Y);
  draw->filledRects += 1;
}

void draw_stroked_rect(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  sgp_set_color(color.R, color.G, color.B, color.A);
  pl_reset(draw->polyliner);
  pl_cap_style(draw->polyliner, LC_JOINT);
  pl_thickness(draw->polyliner, line_thickness);
  pl_start(draw->polyliner, HMM_V2(position.X, position.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X + size.X, position.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X + size.X, position.Y + size.Y));
  pl_lineto(draw->polyliner, HMM_V2(position.X, position.Y + size.Y));
  pl_finish(draw->polyliner);
  draw->strokedRects += 1;
}

void draw_filled_circle(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  draw_filled_rect(draw, position, size, 0.0f, color);
}

void draw_stroked_circle(
  DrawContext *draw, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  draw_stroked_rect(draw, position, size, 0.0f, line_thickness, color);
}

void draw_stroked_line(
  DrawContext *draw, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  sgp_set_color(color.R, color.G, color.B, color.A);
  pl_reset(draw->polyliner);
  pl_thickness(draw->polyliner, line_thickness);
  pl_cap_style(draw->polyliner, LC_SQUARE);
  pl_start(draw->polyliner, start);
  pl_lineto(draw->polyliner, end);
  pl_finish(draw->polyliner);

  draw->lineVertices += 2;
}

void draw_text(
  DrawContext *draw, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor) {

  Box xformedBox = (Box){
    .center = draw_world_to_screen(draw, rect.center),
    .halfSize = draw_scale_world_to_screen(draw, rect.halfSize),
  };

  // top left corner of rect
  HMM_Vec2 dot = draw_world_to_screen(draw, box_top_left(rect));

  // position dot in bottom left corner of rect
  dot.Y += rect.halfSize.Y * draw->zoom * 2;

  // already transformed, so reset the current transform
  // this is done so that the text is scaled by font size rather than
  // getting blurry
  sgp_push_transform();
  sgp_reset_transform();

  draw_screen_text(
    draw, xformedBox, text, len, fontSize * draw->zoom, font, fgColor, bgColor);
  sgp_pop_transform();
}

void draw_screen_text(
  DrawContext *draw, Box rect, const char *text, int len, float fontSize,
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

  draw->texts++;
}

void draw_chip(DrawContext *draw, Theme *theme, Box box, DrawFlags flags) {
  HMM_Vec2 center = box.center;
  HMM_Vec2 pos = HMM_SubV2(center, box.halfSize);
  HMM_Vec2 size = HMM_MulV2F(box.halfSize, 2.0f);

  if (flags & DRAW_HOVERED) {
    draw_filled_rect(
      draw,
      HMM_SubV2(
        pos, HMM_V2(theme->borderWidth * 2.0f, theme->borderWidth * 2.0f)),
      HMM_AddV2(
        size, HMM_V2(theme->borderWidth * 4.0f, theme->borderWidth * 4.0f)),
      theme->componentRadius, theme->color.hovered);
  }

  draw_filled_rect(
    draw, pos, size, theme->componentRadius,
    (flags & DRAW_SELECTED) ? theme->color.selected : theme->color.component);
  draw_stroked_rect(
    draw, pos, size, theme->componentRadius, theme->borderWidth,
    theme->color.componentBorder);
}

typedef struct Symbol {
  const char *text;
  HMM_Vec2 offset;
  float scale;
} Symbol;

const Symbol symbolSolid[] = {
  [SHAPE_DEFAULT] = {.text = "", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_AND] = {.text = "\x01", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_OR] = {.text = "\x03", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_XOR] = {.text = "\x05", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_NOT] = {.text = "\x07", .offset = {.X = 0, .Y = 25.5}, .scale = 1.5},
};

const Symbol symbolOutline[] = {
  [SHAPE_DEFAULT] = {.text = "", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_AND] = {.text = "\x02", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_OR] = {.text = "\x04", .offset = {.X = -2, .Y = 26}, .scale = 1.1},
  [SHAPE_XOR] = {.text = "\x06", .offset = {.X = 0, .Y = 26}, .scale = 1.1},
  [SHAPE_NOT] = {.text = "\x08", .offset = {.X = 0, .Y = 25.5}, .scale = 1.5},
};

static void draw_symbol(
  DrawContext *draw, Theme *theme, Box box, HMM_Vec4 color, ShapeType shape,
  bool outline) {

  const Symbol symbol = outline ? symbolOutline[shape] : symbolSolid[shape];

  HMM_Vec2 center = HMM_AddV2(box.center, symbol.offset);
  HMM_Vec2 hs = HMM_MulV2F(box.halfSize, symbol.scale);

  Box bounds = draw_text_bounds(
    draw, center, symbol.text, 1, ALIGN_CENTER, ALIGN_MIDDLE, hs.Height * 2.0f,
    theme->font);

  draw_text(
    draw, bounds, symbol.text, 1, hs.Height * 2.0f, theme->font, color,
    HMM_V4(0, 0, 0, 0));
}

void draw_component_shape(
  DrawContext *draw, Theme *theme, Box box, ShapeType shape, DrawFlags flags) {
  if (shape == SHAPE_DEFAULT) {
    draw_chip(draw, theme, box, flags);
    return;
  }

  if (flags & DRAW_HOVERED) {
    Box hoverBox = (Box){
      .center = HMM_AddV2(
        box.center,
        HMM_V2(theme->borderWidth * 1.0f, theme->borderWidth * 3.0f)),
      .halfSize = HMM_AddV2(
        box.halfSize,
        HMM_V2(theme->borderWidth * 4.0f, theme->borderWidth * 4.0f))};

    draw_symbol(draw, theme, hoverBox, theme->color.hovered, shape, false);
  }

  draw_symbol(
    draw, theme, box,
    (flags & DRAW_SELECTED) ? theme->color.selected : theme->color.component,
    shape, false);

  draw_symbol(draw, theme, box, theme->color.componentBorder, shape, true);
}

void draw_port(
  DrawContext *draw, Theme *theme, HMM_Vec2 center, DrawFlags flags) {
  float portWidth = theme->portWidth;
  HMM_Vec2 portPosition =
    HMM_SubV2(center, HMM_V2(portWidth / 2.0f, portWidth / 2.0f));
  HMM_Vec2 portSize = HMM_V2(portWidth, portWidth);

  if (flags & DRAW_HOVERED) {
    draw_filled_circle(
      draw,
      HMM_SubV2(
        portPosition,
        HMM_V2(theme->borderWidth * 2.0f, theme->borderWidth * 2.0f)),
      HMM_AddV2(
        portSize, HMM_V2(theme->borderWidth * 4.0f, theme->borderWidth * 4.0f)),
      theme->color.hovered);
  }

  draw_filled_circle(draw, portPosition, portSize, theme->color.port);
  draw_stroked_circle(
    draw, portPosition, portSize, theme->borderWidth, theme->color.portBorder);
}

void draw_selection_box(
  DrawContext *draw, Theme *theme, Box box, DrawFlags flags) {
  HMM_Vec2 pos = HMM_SubV2(box.center, box.halfSize);
  HMM_Vec2 size = HMM_MulV2F(box.halfSize, 2.0f);

  draw_filled_rect(draw, pos, size, 0, theme->color.selectFill);
}

void draw_wire(
  DrawContext *draw, Theme *theme, HMM_Vec2 *verts, int numVerts,
  DrawFlags flags) {
  if (numVerts < 2) {
    return;
  }

  HMM_Vec2 pos = verts[0];

  HMM_Vec4 color = theme->color.wire;
  float thickness = theme->wireThickness;
  if (flags & DRAW_DEBUG) {
    color = HMM_V4(1.0f, 0.0f, 0.0f, 1.0f);
    thickness *= 2.0f;
  }

  for (int i = 1; i < numVerts; i++) {
    HMM_Vec2 vertex = verts[i];
    draw_stroked_line(draw, pos, vertex, thickness, color);
    pos = vertex;
  }
}

void draw_junction(
  DrawContext *draw, Theme *theme, HMM_Vec2 pos, DrawFlags flags) {
  float factor = flags ? 3.0f : 1.5f;

  HMM_Vec2 halfSize =
    HMM_V2(theme->wireThickness * factor, theme->wireThickness * factor);

  draw_filled_circle(
    draw, HMM_SubV2(pos, halfSize), HMM_MulV2F(halfSize, 2.0f),
    (flags & DRAW_SELECTED) ? theme->color.selected : theme->color.wire);
}

void draw_waypoint(
  DrawContext *draw, Theme *theme, HMM_Vec2 pos, DrawFlags flags) {
  float factor = flags ? 4.0f : 3.0f;

  HMM_Vec2 halfSize =
    HMM_V2(theme->wireThickness * factor, theme->wireThickness * factor);

  draw_stroked_circle(
    draw, HMM_SubV2(pos, halfSize), HMM_MulV2F(halfSize, 2.0f),
    theme->wireThickness,
    (flags & DRAW_SELECTED) ? theme->color.selected
                            : HMM_V4(0.6f, 0.3f, 0.5f, 1.0f));
}

void draw_label(
  DrawContext *draw, Theme *theme, Box box, const char *text,
  DrawLabelType type, DrawFlags flags) {

  HMM_Vec4 color = theme->color.labelColor;
  if (type == LABEL_COMPONENT_NAME) {
    color = theme->color.nameColor;
  }

  draw_text(
    draw, box, text, strlen(text), theme->labelFontSize, theme->font, color,
    HMM_V4(0, 0, 0, 0));
}

Box draw_text_bounds(
  DrawContext *ctx, HMM_Vec2 pos, const char *text, int len, HorizAlign horz,
  VertAlign vert, float fontSize, FontHandle font) {
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
