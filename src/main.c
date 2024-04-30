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

#include "avoid/avoid.h"
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define STB_DS_IMPLEMENTATION

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
#include "main.h"

#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gp.h"
#include "sokol_log.h"
#include "sokol_nuklear.h"
#include "stb_ds.h"
#include "stb_image.h"

#include "shaders/msdf_shader.h"

#define LOG_TAG "main"
#include "common/log.h"

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  printf("init\n");

  ux_init(
    &app->circuit, circuit_component_descs(), (FontHandle)&notoSansRegular);

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

  printf("circuit size: %td\n", arrlen(app->circuit.view.circuit.components));

  FILE *fp = fopen("circuit.dot", "w");
  circuit_write_dot(&app->circuit.view.circuit, fp);
  fclose(fp);

  ux_route(&app->circuit);

  // avoid_dump_anchor_boxes(app->circuit.avoid);

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
    .max_vertices = 1024 * 1024, .logger.func = slog_func, .sample_count = 4});

  app->msdf_shader = sg_make_shader(msdf_shader_desc(sg_query_backend()));

  if (sg_query_shader_state(app->msdf_shader) != SG_RESOURCESTATE_VALID) {
    fprintf(stderr, "failed to make MSDF shader\n");
    exit(1);
  }

  app->msdf_pipeline = sgp_make_pipeline(&(sgp_pipeline_desc){
    .shader = app->msdf_shader,
    .has_vs_color = true,
    .sample_count = 4,
    .blend_mode = SGP_BLENDMODE_BLEND,
  });

  if (sg_query_pipeline_state(app->msdf_pipeline) != SG_RESOURCESTATE_VALID) {
    fprintf(stderr, "failed to make MSDF pipeline\n");
    exit(1);
  }
  int width, height, channels;
  stbi_uc *data = stbi_load_from_memory(
    (const stbi_uc *)notoSansRegular.png, notoSansRegular.pngSize, &width,
    &height, &channels, 0);
  if (!data) {
    fprintf(stderr, "failed to load image\n");
    exit(1);
  }

  stbi_uc *img = data;

  int colorFormat = sapp_color_format();

  if (channels != 4) {
    img = malloc(width * height * 4);
    for (int i = 0; i < width * height; i++) {
      if (colorFormat == SG_PIXELFORMAT_RGBA8) {
        img[i * 4 + 0] = data[i * channels + 0];
        img[i * 4 + 1] = data[i * channels + 1];
        img[i * 4 + 2] = data[i * channels + 2];
      } else if (colorFormat == SG_PIXELFORMAT_BGRA8) {
        img[i * 4 + 0] = data[i * channels + 2];
        img[i * 4 + 1] = data[i * channels + 1];
        img[i * 4 + 2] = data[i * channels + 0];
      }
      img[i * 4 + 3] = 255;
    }
  }

  app->msdf_tex = sg_make_image(&(sg_image_desc){
    .width = width,
    .height = height,
    .pixel_format = colorFormat,
    .data.subimage[0][0] =
      {
        .ptr = img,
        .size = width * height * sizeof(stbi_uc) * 4,
      },
  });

  if (channels != 4) {
    free(img);
  }

  stbi_image_free(data);

  app->msdf_sampler = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_LINEAR,
    .mag_filter = SG_FILTER_LINEAR,
    .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
  });
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  sg_destroy_sampler(app->msdf_sampler);
  sg_destroy_image(app->msdf_tex);
  sg_destroy_pipeline(app->msdf_pipeline);
  sg_destroy_shader(app->msdf_shader);

  ux_free(&app->circuit);

  snk_shutdown();
  sgp_shutdown();
  sg_shutdown();
  free(app);
}

struct nk_canvas {
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
  struct nk_canvas canvas;
  if (canvas_begin(
        ctx, &canvas, 0, 0, 0, sapp_width(), sapp_height(),
        nk_rgb(0x22, 0x29, 0x33))) {
    app->circuit.input.frameDuration = sapp_frame_duration();

    ux_draw(&app->circuit, canvas.painter);

    app->circuit.input.scroll = HMM_V2(0, 0);
    app->circuit.input.mouseDelta = HMM_V2(0, 0);
  }
  canvas_end(ctx, &canvas);
}

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;

  nk_fill_rect(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_rect(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    line_thickness, nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_fill_circle(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y),
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_circle(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  float line_thickness, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_arc(
    nk_ctx, position.X, position.Y, radius, aMin, aMax, line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_filled_arc(
  Context ctx, HMM_Vec2 position, float radius, float aMin, float aMax,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_fill_arc(
    nk_ctx, position.X, position.Y, radius, aMin, aMax,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_line(
    nk_ctx, start.X, start.Y, end.X, end.Y, line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void draw_text(
  Context ctx, Box rect, const char *text, int len, float fontSize,
  FontHandle font, HMM_Vec4 fgColor, HMM_Vec4 bgColor) {
  Font *f = (Font *)font;
  // top left corner of rect
  HMM_Vec2 dot = box_top_left(rect);

  // position dot in bottom left corner of rect
  dot.Y += rect.halfSize.Y * 2;

  sgp_set_color(fgColor.R, fgColor.G, fgColor.B, fgColor.A);

  for (int i = 0; i < len; i++) {
    const FontGlyph *glyph = &f->glyphs[(int)text[i]];
    sgp_draw_textured_rect(
      0,
      (sgp_rect){
        .x = dot.X + glyph->planeBounds.x * fontSize,
        .y = dot.Y + glyph->planeBounds.y * fontSize,
        .w = glyph->planeBounds.width * fontSize,
        .h = glyph->planeBounds.height * fontSize,
      },
      (sgp_rect){
        .x = glyph->atlasBounds.x,
        .y = glyph->atlasBounds.y,
        .w = glyph->atlasBounds.width,
        .h = glyph->atlasBounds.height,
      });

    dot.X += glyph->advance * fontSize;
  }

  sgp_reset_color();
}

Box draw_text_bounds(
  HMM_Vec2 pos, const char *text, int len, HorizAlign horz, VertAlign vert,
  float fontSize, FontHandle font) {
  Font *f = (Font *)font;
  float width = 0;
  float height = 0;
  for (int i = 0; i < len; i++) {
    width += f->glyphs[(int)text[i]].advance * fontSize;
    float glyphHeight = f->glyphs[(int)text[i]].planeBounds.height * fontSize;
    if (glyphHeight > height) {
      height = glyphHeight;
    }
  }
  // float height = f->lineHeight * fontSize;
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
  switch (vert) {
  case ALIGN_TOP:
    center.Y += height / 2;
    break;
  case ALIGN_MIDDLE:
    break;
  case ALIGN_BOTTOM:
    center.Y -= height / 2;
    break;
  }
  return (Box){.center = center, .halfSize = HMM_V2(width / 2, height / 2)};
}

void frame(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  struct nk_context *ctx = snk_new_frame();
  (void)ctx;

  int width = sapp_width(), height = sapp_height();
  sgp_begin(width, height);
  sgp_set_pipeline(app->msdf_pipeline);

  sgp_set_image(0, app->msdf_tex);
  sgp_set_sampler(0, app->msdf_sampler);
  sgp_set_color(1, 1, 1, 1);

  canvas(ctx, app);

  sgp_reset_color();
  sgp_reset_sampler(0);
  sgp_reset_image(0);
  sgp_reset_pipeline();

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
  my_app_t *app = malloc(sizeof(my_app_t));
  *app = (my_app_t){
    .filename = "testdata/simple_test.dig",
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
