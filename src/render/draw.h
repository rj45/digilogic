#ifndef DRAW_H
#define DRAW_H

#include "core/core.h"

typedef struct DrawContext DrawContext;
typedef void *FontHandle;

typedef enum VertAlign {
  ALIGN_TOP,
  ALIGN_MIDDLE,
  ALIGN_BOTTOM,
} VertAlign;

typedef enum HorizAlign {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT,
} HorizAlign;

typedef enum DrawFlags {
  DRAW_HOVERED = 1 << 0,
  DRAW_SELECTED = 1 << 1,
  DRAW_DEBUG = 1 << 2,
} DrawFlags;

typedef enum DrawLabelType {
  LABEL_COMPONENT_NAME,
  LABEL_COMPONENT_TYPE,
  LABEL_PORT,
  LABEL_WIRE,
} DrawLabelType;

typedef struct Theme {
  float portSpacing;
  float componentWidth;
  float portWidth;
  float borderWidth;
  float componentRadius;
  float wireThickness;
  float labelPadding;
  float labelFontSize;
  float gateThickness;
  FontHandle font;
  struct {
    HMM_Vec4 component;
    HMM_Vec4 componentBorder;
    HMM_Vec4 port;
    HMM_Vec4 portBorder;
    HMM_Vec4 wire;
    HMM_Vec4 hovered;
    HMM_Vec4 selected;
    HMM_Vec4 selectFill;
    HMM_Vec4 labelColor;
    HMM_Vec4 nameColor;
  } color;
} Theme;

void theme_init(Theme *theme, FontHandle font);

void draw_reset(DrawContext *draw);
void draw_set_zoom(DrawContext *draw, float zoom);
void draw_add_pan(DrawContext *draw, HMM_Vec2 pan);
HMM_Vec2 draw_get_pan(DrawContext *draw);
float draw_get_zoom(DrawContext *draw);
HMM_Vec2 draw_screen_to_world(DrawContext *draw, HMM_Vec2 screenPos);
HMM_Vec2 draw_scale_screen_to_world(DrawContext *draw, HMM_Vec2 dirvec);
HMM_Vec2 draw_world_to_screen(DrawContext *draw, HMM_Vec2 worldPos);
HMM_Vec2 draw_scale_world_to_screen(DrawContext *draw, HMM_Vec2 dirvec);

void draw_symbol_shape(
  DrawContext *draw, Theme *theme, Box box, SymbolShape shape, DrawFlags flags);
void draw_port(
  DrawContext *draw, Theme *theme, HMM_Vec2 center, DrawFlags flags);
void draw_selection_box(
  DrawContext *draw, Theme *theme, Box box, DrawFlags flags);
void draw_wire(
  DrawContext *draw, Theme *theme, HMM_Vec2 *verts, int numVerts,
  DrawFlags flags);
void draw_junction(
  DrawContext *draw, Theme *theme, HMM_Vec2 pos, DrawFlags flags);
void draw_waypoint(
  DrawContext *draw, Theme *theme, HMM_Vec2 pos, DrawFlags flags);
void draw_label(
  DrawContext *draw, Theme *theme, Box box, const char *text,
  DrawLabelType type, DrawFlags flags);
Box draw_text_bounds(
  DrawContext *draw, HMM_Vec2 pos, const char *text, int len, HorizAlign horz,
  VertAlign vert, float fontSize, FontHandle font);

#endif // DRAW_H