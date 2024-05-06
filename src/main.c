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

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS

#include "font.h"
#include "ux/ux.h"
#include "view/view.h"
#include <stdio.h>
#include <stdlib.h>

#include <stdlib.h>

#include "handmade_math.h"

#include "core/core.h"
#include "font.h"
#include "import/import.h"

#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gp.h"
#include "sokol_log.h"
#include "sokol_nuklear.h"
#include "stb_image.h"

#include "render/fons_sgp.h"

#define LOG_TAG "main"
#include "common/log.h"

#define FONT_ATLAS_WIDTH 1024
#define FONT_ATLAS_HEIGHT 1024

typedef struct FonsFont {
  FONScontext *fsctx;
  int mainFont;
  int iconFont;
} FonsFont;

typedef struct my_app_t {
  CircuitUX circuit;

  const char *filename;

  FONScontext *fsctx;
  FonsFont fonsFont;

  float pzoom;
} my_app_t;

static void fons_error(void *user_ptr, int error, int val) {
  my_app_t *app = (my_app_t *)user_ptr;
  (void)app;
  fprintf(stderr, "FONS error: %d %d\n", error, val);
}

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  printf("init\n");

  sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
  });
  if (!sg_isvalid()) {
    fprintf(stderr, "Failed to create Sokol GFX context!\n");
    exit(1);
  }

  // initialize Sokol GP
  sgp_desc sgpdesc = {0};
  sgp_setup(&sgpdesc);
  if (!sgp_is_valid()) {
    fprintf(
      stderr, "Failed to create Sokol GP context: %s\n",
      sgp_get_error_message(sgp_get_last_error()));
    exit(1);
  }

  snk_setup(&(snk_desc_t){
    .max_vertices = 64 * 1024, .logger.func = slog_func, .sample_count = 4});

  app->fsctx = fsgp_create(
    &(fsgp_desc_t){.width = FONT_ATLAS_WIDTH, .height = FONT_ATLAS_HEIGHT});
  if (!app->fsctx) {
    fprintf(stderr, "Failed to create FONS context\n");
    exit(1);
  }

  fonsSetErrorCallback(app->fsctx, fons_error, app);

  int mainFont = fonsAddFontMem(
    app->fsctx, "sans", (unsigned char *)notoSansRegular, notoSansRegularLength,
    0);
  int iconFont = fonsAddFontMem(
    app->fsctx, "icons", (unsigned char *)schemalibSymbols,
    schemalibSymbolsLength, 0);
  app->fonsFont = (FonsFont){
    .fsctx = app->fsctx,
    .mainFont = mainFont,
    .iconFont = iconFont,
  };

  ux_init(&app->circuit, circuit_component_descs(), (FontHandle)&app->fonsFont);

  // ComponentID and = ux_add_component(&app->circuit, COMP_AND, HMM_V2(100,
  // 100)); ComponentID or = ux_add_component(&app->circuit, COMP_OR,
  // HMM_V2(300, 200)); ComponentID not = ux_add_component(&app->circuit,
  // COMP_NOT, HMM_V2(200, 150));

  // ux_add_net(
  //   &app->circuit, view_port_start(&app->circuit.view, and) + 2,
  //   view_port_start(&app->circuit.view, not ));

  // ux_add_net(
  //   &app->circuit, view_port_start(&app->circuit.view, not ) + 1,
  //   view_port_start(&app->circuit.view, or));

  // ux_add_net(
  //   &app->circuit, view_port_start(&app->circuit.view, and),
  //   view_port_start(&app->circuit.view, or) + 2);

  // ux_add_net(
  //   &app->circuit, view_port_start(&app->circuit.view, and) + 1,
  //   view_port_start(&app->circuit.view, or) + 1);

  // import_digital(&app->circuit, "testdata/alu_1bit_2inpgate.dig");
  // import_digital(&app->circuit, "testdata/alu_1bit_2gatemux.dig");
  // import_digital(&app->circuit, "testdata/simple_test.dig");
  import_digital(&app->circuit, app->filename);

  // FILE *fp = fopen("circuit.dot", "w");
  // circuit_write_dot(&app->circuit.view.circuit, fp);
  // fclose(fp);

  ux_route(&app->circuit);

  // autoroute_dump_anchor_boxes(app->circuit.router);

  app->pzoom = app->circuit.view.zoom;
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  ux_free(&app->circuit);

  fsgp_destroy(app->fsctx);

  snk_shutdown();
  sgp_shutdown();
  sg_shutdown();
  free(app);
}

struct nk_canvas {
  struct my_app_t *app;
  struct nk_command_buffer *painter;
  struct nk_vec2 item_spacing;
  struct nk_vec2 panel_padding;
  struct nk_style_item window_background;
};

static nk_bool canvas_begin(
  struct nk_context *ctx, struct nk_canvas *canvas, nk_flags flags, int x,
  int y, int width, int height, struct nk_color background_color) {
  /* save style properties which will be overwritten */
  canvas->panel_padding = ctx->style.window.padding;
  canvas->item_spacing = ctx->style.window.spacing;
  canvas->window_background = ctx->style.window.fixed_background;

  /* use the complete window space and set background */
  ctx->style.window.spacing = nk_vec2(0, 0);
  ctx->style.window.padding = nk_vec2(0, 0);
  ctx->style.window.fixed_background = nk_style_item_color(background_color);

  /* create/update window and set position + size */
  if (!nk_begin(
        ctx, "Canvas", nk_rect(x, y, width, height),
        NK_WINDOW_NO_SCROLLBAR | flags))
    return nk_false;

  /* allocate the complete window space for drawing */
  {
    struct nk_rect total_space;
    total_space = nk_window_get_content_region(ctx);
    nk_layout_row_dynamic(ctx, total_space.h, 1);
    nk_widget(&total_space, ctx);
    canvas->painter = nk_window_get_canvas(ctx);
  }

  return nk_true;
}

static void canvas_end(struct nk_context *ctx, struct nk_canvas *canvas) {
  nk_end(ctx);
  ctx->style.window.spacing = canvas->panel_padding;
  ctx->style.window.padding = canvas->item_spacing;
  ctx->style.window.fixed_background = canvas->window_background;
}

static void canvas(struct nk_context *ctx, my_app_t *app) {
  struct nk_canvas canvas = {.app = app};
  if (canvas_begin(
        ctx, &canvas, 0, 0, 0, sapp_width(), sapp_height(),
        nk_rgb(0x22, 0x29, 0x33))) {
    app->circuit.input.frameDuration = sapp_frame_duration();

    ux_draw(&app->circuit, &canvas);

    app->circuit.input.scroll = HMM_V2(0, 0);
    app->circuit.input.mouseDelta = HMM_V2(0, 0);
  }
  canvas_end(ctx, &canvas);
}

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;

  nk_fill_rect(
    canvas->painter, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_stroke_rect(
    canvas->painter, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    line_thickness, nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_fill_circle(
    canvas->painter, nk_rect(position.X, position.Y, size.X, size.Y),
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_stroke_circle(
    canvas->painter, nk_rect(position.X, position.Y, size.X, size.Y),
    line_thickness, nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  float line_thickness, HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_stroke_arc(
    canvas->painter, position.X, position.Y, radius, aMin, aMax, line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_filled_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_fill_arc(
    canvas->painter, position.X, position.Y, radius, aMin, aMax,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_curve(
  Context ctx, HMM_Vec2 a, HMM_Vec2 ctrl0, HMM_Vec2 ctrl1, HMM_Vec2 b,
  float line_thickness, HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_stroke_curve(
    canvas->painter, a.X, a.Y, ctrl0.X, ctrl0.Y, ctrl1.X, ctrl1.Y, b.X, b.Y,
    line_thickness, nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  nk_stroke_line(
    canvas->painter, start.X, start.Y, end.X, end.Y, line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_text(
  Context ctx, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor) {
  struct nk_canvas *canvas = (struct nk_canvas *)ctx;
  (void)canvas;
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

void frame(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  if (app->pzoom != app->circuit.view.zoom) {
    // on zoom change, reset the font atlas
    app->pzoom = app->circuit.view.zoom;
    fonsResetAtlas(app->fsctx, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT);
  }

  struct nk_context *ctx = snk_new_frame();
  (void)ctx;

  int width = sapp_width(), height = sapp_height();
  sgp_begin(width, height);

  canvas(ctx, app);

  sg_pass pass = {
    .action =
      {
        .colors[0] =
          {.load_action = SG_LOADACTION_CLEAR,
           .store_action = SG_STOREACTION_STORE,
           .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}},
      },
    .swapchain = sglue_swapchain()};
  sg_begin_pass(&pass);

  fsgp_flush(app->fsctx);

  snk_render(sapp_width(), sapp_height());
  sgp_flush();
  sgp_end();
  sg_end_pass();
  sg_commit();

  bv_clear_all(app->circuit.input.keysPressed);
}

void event(const sapp_event *event, void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  if (snk_handle_event(event)) {
    // return;
  }

  switch (event->type) {
  case SAPP_EVENTTYPE_KEY_DOWN:
    bv_set(app->circuit.input.keysDown, event->key_code);
    bv_set(app->circuit.input.keysPressed, event->key_code);
    app->circuit.input.modifiers = event->modifiers;
    if (event->key_code == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
    }
    break;

  case SAPP_EVENTTYPE_KEY_UP:
    bv_clear(app->circuit.input.keysDown, event->key_code);
    app->circuit.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_DOWN:
  case SAPP_EVENTTYPE_MOUSE_UP:
    app->circuit.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    HMM_Vec2 mousePos = HMM_V2(event->mouse_x, event->mouse_y);
    app->circuit.input.mouseDelta = HMM_AddV2(
      app->circuit.input.mouseDelta,
      HMM_SubV2(app->circuit.input.mousePos, mousePos));
    app->circuit.input.mousePos = mousePos;
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_SCROLL: {
    app->circuit.input.scroll = HMM_AddV2(
      app->circuit.input.scroll, HMM_V2(event->scroll_x, event->scroll_y));

    break;
  }
  default:
    // ignore
    break;
  }
}

#ifdef __APPLE__
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
  void *array[150];
  size_t size;

  size = backtrace(array, 150);

  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}
#endif

sapp_desc sokol_main(int argc, char *argv[]) {
  ux_global_init();

  my_app_t *app = malloc(sizeof(my_app_t));
  *app = (my_app_t){
    .filename = "testdata/alu_1bit_2gatemux.dig",
  };

  if (argc > 1) {
    app->filename = argv[1];
  }

#ifdef __APPLE__
  signal(SIGSEGV, handler);
  signal(SIGABRT, handler);
  signal(SIGTRAP, handler);
#endif

  return (sapp_desc){
    .width = 1024,
    .height = 768,
    .user_data = app,
    .init_userdata_cb = init,
    .frame_userdata_cb = frame,
    .cleanup_userdata_cb = cleanup,
    .event_userdata_cb = event,
    .logger.func = slog_func,
    .window_title = "digilogic",
    .swap_interval = 2,
    .sample_count = 4,
  };
}
