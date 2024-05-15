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

#include <stdint.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_STANDARD_VARARGS

#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>

#include <stdlib.h>

#include "handmade_math.h"

#include "assets.h"
#include "core/core.h"
#include "import/import.h"

#include "assetsys.h"
#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gp.h"
#include "sokol_log.h"
#include "sokol_nuklear.h"
#include "sokol_time.h"
#include "stb_image.h"

#include "render/fons_sgp.h"

#include "render/render.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

#define FONT_ATLAS_WIDTH 1536
#define FONT_ATLAS_HEIGHT 1536

#define UI_FONT_SIZE 20

typedef struct my_app_t {
  CircuitUI circuit;

  const char *filename;

  bool loaded;

  assetsys_t *assetsys;

  FONScontext *fsctx;
  FonsFont fonsFont;

  struct nk_font *latin;
  struct nk_font_atlas atlas;
  sg_image font_img;
  sg_sampler font_smp;
  snk_image_t default_font;

  DrawContext draw;

  float pzoom;

  uint64_t lastDrawTime;

  uint64_t frameIntervals[60];
  int frameIntervalIndex;

  uint64_t frameIntervalTime;
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
  sgp_desc sgpdesc = {
    .sample_count = 4,
    .max_vertices = 1024 * 1024,
  };
  sgp_setup(&sgpdesc);
  if (!sgp_is_valid()) {
    fprintf(
      stderr, "Failed to create Sokol GP context: %s\n",
      sgp_get_error_message(sgp_get_last_error()));
    exit(1);
  }

  sg_enable_frame_stats();

  snk_setup(&(snk_desc_t){
    .max_vertices = 64 * 1024,
    .logger.func = slog_func,
    .sample_count = 4,
    .no_default_font = true,
  });

  app->fsctx = fsgp_create(
    &(fsgp_desc_t){.width = FONT_ATLAS_WIDTH, .height = FONT_ATLAS_HEIGHT});
  if (!app->fsctx) {
    fprintf(stderr, "Failed to create FONS context\n");
    exit(1);
  }

  fonsSetErrorCallback(app->fsctx, fons_error, app);

  app->assetsys = assetsys_create(0);
  assetsys_mount_from_memory(app->assetsys, assets_zip, assets_zip_len, "/");

  assetsys_file_t file;
  assetsys_file(app->assetsys, "/assets/NotoSans-Regular.ttf", &file);
  int mainFontSize = assetsys_file_size(app->assetsys, file);
  unsigned char *mainFontData = malloc(mainFontSize);
  assetsys_file_load(
    app->assetsys, file, &mainFontSize, mainFontData, mainFontSize);
  int mainFont =
    fonsAddFontMem(app->fsctx, "sans", mainFontData, mainFontSize, 1);

  assetsys_file(app->assetsys, "/assets/symbols.ttf", &file);
  int symbolsFontSize = assetsys_file_size(app->assetsys, file);
  unsigned char *symbolsFontData = malloc(symbolsFontSize);
  assetsys_file_load(
    app->assetsys, file, &symbolsFontSize, symbolsFontData, symbolsFontSize);
  int iconFont =
    fonsAddFontMem(app->fsctx, "icons", symbolsFontData, symbolsFontSize, 1);

  app->fonsFont = (FonsFont){
    .fsctx = app->fsctx,
    .mainFont = mainFont,
    .iconFont = iconFont,
  };

  struct nk_font_config cfg_latin = nk_font_config(UI_FONT_SIZE);
  cfg_latin.range = nk_font_default_glyph_ranges();
  cfg_latin.oversample_h = cfg_latin.oversample_v = 4;
  cfg_latin.pixel_snap = false;

  app->font_smp = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_LINEAR,
    .mag_filter = SG_FILTER_LINEAR,
    .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    .label = "sokol-nuklear-font-sampler",
  });

  nk_font_atlas_init_default(&app->atlas);
  nk_font_atlas_begin(&app->atlas);
  int font_width = 0, font_height = 0;
  app->latin = nk_font_atlas_add_from_memory(
    &app->atlas, (void *)mainFontData, mainFontSize, UI_FONT_SIZE, &cfg_latin);
  const void *pixels = nk_font_atlas_bake(
    &app->atlas, &font_width, &font_height, NK_FONT_ATLAS_RGBA32);
  assert((font_width > 0) && (font_height > 0));
  app->font_img = sg_make_image(&(sg_image_desc){
    .width = font_width,
    .height = font_height,
    .pixel_format = SG_PIXELFORMAT_RGBA8,
    .data.subimage[0][0] =
      {.ptr = pixels,
       .size = (size_t)(font_width * font_height) * sizeof(uint32_t)},
    .label = "sokol-nuklear-font"});
  app->default_font = snk_make_image(&(snk_image_desc_t){
    .image = app->font_img,
    .sampler = app->font_smp,
  });
  nk_font_atlas_end(&app->atlas, snk_nkhandle(app->default_font), 0);
  nk_font_atlas_cleanup(&app->atlas);
  snk_set_atlas(&app->atlas);

  draw_init(&app->draw, app->fsctx);

  ui_init(
    &app->circuit, circuit_component_descs(), &app->draw,
    (FontHandle)&app->fonsFont);

  app->pzoom = draw_get_zoom(&app->draw);
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  draw_free(&app->draw);

  ui_free(&app->circuit);

  sg_destroy_sampler(app->font_smp);
  sg_destroy_image(app->font_img);

  fsgp_destroy(app->fsctx);

  assetsys_destroy(app->assetsys);

  snk_shutdown();
  sgp_shutdown();
  sg_shutdown();
  free(app);
}

void load_file(my_app_t *app, const char *filename) {
  assetsys_file_t file;
  assetsys_file(app->assetsys, filename, &file);
  int fileSize = assetsys_file_size(app->assetsys, file);
  char *buffer = malloc(fileSize + 1);
  assetsys_file_load(app->assetsys, file, &fileSize, buffer, fileSize);
  buffer[fileSize] = 0;

  log_info("Loading file %s, %d bytes\n", filename, fileSize);

  import_digital(&app->circuit.ux, buffer);
  free(buffer);

  ux_route(&app->circuit.ux);

  // autoroute_dump_anchor_boxes(app->circuit.ux.router);

  app->loaded = true;
}

void frame(void *user_data) {
  uint64_t frameStart = stm_now();

  my_app_t *app = (my_app_t *)user_data;

  if (app->pzoom != draw_get_zoom(&app->draw)) {
    // on zoom change, reset the font atlas
    app->pzoom = draw_get_zoom(&app->draw);
    fonsResetAtlas(app->fsctx, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT);
  }

  struct nk_context *ctx = snk_new_frame();

  nk_style_set_font(ctx, &app->latin->handle);

  int width = sapp_width(), height = sapp_height();
  sgp_begin(width, height);
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);

  ui_update(&app->circuit, ctx, width, height);

  if (!app->loaded) {
    if (nk_begin(
          ctx, "Load example file",
          nk_rect(
            (float)width / 3, (float)height / 3, (float)width / 3,
            (float)height / 3),
          NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
      struct nk_vec2 min = nk_window_get_content_region_min(ctx);
      struct nk_vec2 max = nk_window_get_content_region_max(ctx);
      float height = ((max.y - min.y) / 3) - 6;
      nk_layout_row_dynamic(ctx, height, 1);
      if (nk_button_label(ctx, "Load small sized test circuit")) {
        load_file(app, "/assets/testdata/simple_test.dig");
      }

      if (nk_button_label(ctx, "Load medium sized test circuit")) {
        load_file(app, "/assets/testdata/alu_1bit_2gatemux.dig");
      }

      if (nk_button_label(ctx, "Load large sized test circuit")) {
        load_file(app, "/assets/testdata/alu_1bit_2inpgate.dig");
      }
    }
    nk_end(ctx);
  }

  draw_begin_frame(&app->draw);
  app->circuit.ux.input.frameDuration = sapp_frame_duration();
  ui_draw(&app->circuit);
  app->circuit.ux.input.scroll = HMM_V2(0, 0);
  app->circuit.ux.input.mouseDelta = HMM_V2(0, 0);
  draw_end_frame(&app->draw);

  sg_frame_stats stats = sg_query_frame_stats();

  uint64_t avgFrameInterval = 0.0;
  for (int i = 0; i < 60; i++) {
    avgFrameInterval += app->frameIntervals[i];
  }
  avgFrameInterval /= 60;

  char buff[256];
  if (app->circuit.ux.showFPS) {
    snprintf(
      buff, sizeof(buff),
      "Draw time: %1.2f Frame interval: %.2f FPS: %.2f Draws: %d Pipelines: %d "
      "LineVerts: %d FilledRects: %d StrokedRects: %d Texts: %d",
      stm_ms(app->lastDrawTime), stm_ms(avgFrameInterval),
      1 / stm_sec(avgFrameInterval), stats.num_draw, stats.num_apply_pipeline,
      app->draw.lineVertices, app->draw.filledRects, app->draw.strokedRects,
      app->draw.texts);

    Box box = draw_text_bounds(
      &app->draw, HMM_V2(0, height), buff, strlen(buff), ALIGN_LEFT,
      ALIGN_BOTTOM, 12.0, &app->fonsFont);
    draw_screen_text(
      &app->draw, box, buff, strlen(buff), 25.0, &app->fonsFont,
      HMM_V4(1, 1, 1, 1), HMM_V4(0, 0, 0, 0));
  }

  sg_pass pass = {
    .action =
      {
        .colors[0] =
          {.load_action = SG_LOADACTION_CLEAR,
           .store_action = SG_STOREACTION_STORE,
           .clear_value = {0.08f, 0.1f, 0.12f, 1.0f}},
      },
    .swapchain = sglue_swapchain()};
  sg_begin_pass(&pass);

  fsgp_flush(app->fsctx);

  sgp_flush();

  snk_render(sapp_width(), sapp_height());

  sgp_end();
  sg_end_pass();
  sg_commit();

  bv_clear_all(app->circuit.ux.input.keysPressed);

  uint64_t frameInterval = stm_laptime(&app->frameIntervalTime);

  app->frameIntervals[app->frameIntervalIndex] = frameInterval;
  app->frameIntervalIndex = (app->frameIntervalIndex + 1) % 60;

  app->lastDrawTime = stm_since(frameStart);
}

void event(const sapp_event *event, void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  if (snk_handle_event(event)) {
    return;
  }

  switch (event->type) {
  case SAPP_EVENTTYPE_KEY_DOWN:
    bv_set(app->circuit.ux.input.keysDown, event->key_code);
    bv_set(app->circuit.ux.input.keysPressed, event->key_code);
    app->circuit.ux.input.modifiers = event->modifiers;
    if (event->key_code == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
    }
    break;

  case SAPP_EVENTTYPE_KEY_UP:
    bv_clear(app->circuit.ux.input.keysDown, event->key_code);
    app->circuit.ux.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_DOWN:
  case SAPP_EVENTTYPE_MOUSE_UP:
    app->circuit.ux.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    HMM_Vec2 mousePos = HMM_V2(event->mouse_x, event->mouse_y);
    app->circuit.ux.input.mouseDelta = HMM_AddV2(
      app->circuit.ux.input.mouseDelta,
      HMM_SubV2(app->circuit.ux.input.mousePos, mousePos));
    app->circuit.ux.input.mousePos = mousePos;
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_SCROLL: {
    app->circuit.ux.input.scroll = HMM_AddV2(
      app->circuit.ux.input.scroll, HMM_V2(event->scroll_x, event->scroll_y));
    break;
  }
  default:
    // ignore
    break;
  }
}

#ifndef _WIN32
#include "stacktrace.h"
#endif

sapp_desc sokol_main(int argc, char *argv[]) {
  ux_global_init();
  stm_setup();

#ifndef _WIN32
  init_exceptions(argv[0]);
#endif

  my_app_t *app = malloc(sizeof(my_app_t));
  *app = (my_app_t){
    .filename = "testdata/alu_1bit_2gatemux.dig",
  };

  if (argc > 1) {
    app->filename = argv[1];
  }

  return (sapp_desc){
    .width = 1280,
    .height = 720,
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
