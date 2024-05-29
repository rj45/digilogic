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

#include "autoroute/autoroute.h"
#include <stdint.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_SOFTWARE_FONT

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
#include "sokol_time.h"
#include "stb_image.h"

#include "render/fons_sgp.h"
#include "render/sokol_nuklear.h"

#include "render/fons_nuklear.h"

#include "render/render.h"

#include "nvdialog.h"

#define THREAD_IMPLEMENTATION
#include "thread.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

#define FONT_ATLAS_WIDTH 2048
#define FONT_ATLAS_HEIGHT 2048

#define UI_FONT_SIZE 20

#ifndef MSAA_SAMPLE_COUNT
#define MSAA_SAMPLE_COUNT 4
#endif

typedef struct my_app_t {
  CircuitUI circuit;

  const char *filename;

  bool loaded;

  assetsys_t *assetsys;

  FONScontext *fsctx;
  FonsFont fonsFont;

  struct nk_user_font nkFont;

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

  switch (error) {
  case FONS_ATLAS_FULL:
    log_error("FONS_ATLAS_FULL: Fontstash atlas full: %d\n", val);
    break;
  case FONS_SCRATCH_FULL:
    log_error("FONS_SCRATCH_FULL: Fontstash scratch full: %d\n", val);
    break;
  case FONS_STATES_OVERFLOW:
    log_error("FONS_STATES_OVERFLOW: Fontstash state overflow: %d\n", val);
    break;
  case FONS_STATES_UNDERFLOW:
    log_error("FONS_STATES_UNDERFLOW: Fonstash state underflow: %d\n", val);
    break;
  default:
    log_error("Unknown fonstash error: %d %d\n", error, val);
    break;
  }
}

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  log_info("Initialized sokol_app");

  int result = nvd_init();
  if (result != 0) {
    fprintf(stderr, "Failed to initialize native file dialog support\n");
    exit(1);
  }

#if defined(__APPLE__)
  nvd_set_parent((NvdParentWindow *)sapp_macos_get_window());
#elif defined(_WIN32)
  nvd_set_parent((NvdParentWindow *)sapp_win32_get_hwnd());
#else // Linux
  // todo: not sure how to get the parent window on Linux
  // nvd_set_parent((NvdParentWindow *)sapp_linux_get_window());
#endif
  log_info("nvdialog initialized");

  sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
  });
  if (!sg_isvalid()) {
    fprintf(stderr, "Failed to create Sokol GFX context!\n");
    exit(1);
  }
  log_info("sokol_gfx initialized");

  // initialize Sokol GP
  sgp_desc sgpdesc = {
    .sample_count = MSAA_SAMPLE_COUNT,
    .max_vertices = 1024 * 1024,
  };
  sgp_setup(&sgpdesc);
  if (!sgp_is_valid()) {
    fprintf(
      stderr, "Failed to create Sokol GP context: %s\n",
      sgp_get_error_message(sgp_get_last_error()));
    exit(1);
  }
  log_info("sokol_gp initialized");

  sg_enable_frame_stats();

  app->fsctx = fsgp_create(
    &(fsgp_desc_t){.width = FONT_ATLAS_WIDTH, .height = FONT_ATLAS_HEIGHT});
  if (!app->fsctx) {
    fprintf(stderr, "Failed to create FONS context\n");
    exit(1);
  }
  log_info("sokol_fontstash initialized");

  fonsSetErrorCallback(app->fsctx, fons_error, app);

  app->assetsys = assetsys_create(0);
  assetsys_mount_from_memory(app->assetsys, assets_zip, assets_zip_len, "/");
  log_info("assetsys mounted");

  assetsys_file_t file;
  assetsys_file(app->assetsys, "/assets/NotoSans-Regular.ttf", &file);
  int mainFontSize = assetsys_file_size(app->assetsys, file);
  unsigned char *mainFontData = malloc(mainFontSize);
  assetsys_file_load(
    app->assetsys, file, &mainFontSize, mainFontData, mainFontSize);
  int mainFont =
    fonsAddFontMem(app->fsctx, "sans", mainFontData, mainFontSize, 1);
  log_info("main font loaded");

  assetsys_file(app->assetsys, "/assets/symbols.ttf", &file);
  int symbolsFontSize = assetsys_file_size(app->assetsys, file);
  unsigned char *symbolsFontData = malloc(symbolsFontSize);
  assetsys_file_load(
    app->assetsys, file, &symbolsFontSize, symbolsFontData, symbolsFontSize);
  int iconFont =
    fonsAddFontMem(app->fsctx, "icons", symbolsFontData, symbolsFontSize, 1);
  log_info("symbol font loaded");

  app->fonsFont = (FonsFont){
    .fsctx = app->fsctx,
    .mainFont = mainFont,
    .iconFont = iconFont,
  };

  snk_setup(&(snk_desc_t){
    .max_vertices = 64 * 1024,
    .logger.func = slog_func,
    .sample_count = MSAA_SAMPLE_COUNT,
  });
  log_info("sokol_nuklear initialized");

  nuklear_fontstash_init(&app->nkFont, app->fsctx, mainFont, UI_FONT_SIZE);
  log_info("nuklear_fontstash initialized");

  draw_init(&app->draw, app->fsctx);

  ui_init(
    &app->circuit, circuit_component_descs(), &app->draw,
    (FontHandle)&app->fonsFont);

  app->pzoom = draw_get_zoom(&app->draw);

  log_info("initialization complete, entering main loop");
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  draw_free(&app->draw);

  ui_free(&app->circuit);

  nuklear_fontstash_free(&app->nkFont);

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
  ux_build_bvh(&app->circuit.ux);

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
  nk_style_set_font(ctx, &app->nkFont);

  int width = sapp_width(), height = sapp_height();
  sgp_begin(width, height);
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);

  ui_update(&app->circuit, ctx, width, height);

  if (!app->loaded) {
    bool hasAutoSave = false;
    FILE *fp = fopen(platform_autosave_path(), "r");
    if (fp) {
      hasAutoSave = true;
      fclose(fp);
    }

    if (nk_begin(
          ctx, "Load example file",
          nk_rect(
            (float)width / 3, (float)height / 3, (float)width / 3,
            (float)height / 3),
          NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
      struct nk_vec2 min = nk_window_get_content_region_min(ctx);
      struct nk_vec2 max = nk_window_get_content_region_max(ctx);
      float height = ((max.y - min.y) / (hasAutoSave ? 5 : 4)) - 6;
      nk_layout_row_dynamic(ctx, height, 1);

      if (nk_button_label(ctx, "New empty circuit")) {
        app->loaded = true;
      }

      if (hasAutoSave) {
        if (nk_button_label(ctx, "Load auto-save")) {
          circuit_load_file(
            &app->circuit.ux.view.circuit, platform_autosave_path());
          ux_route(&app->circuit.ux);
          ux_build_bvh(&app->circuit.ux);
          app->loaded = true;
        }
      }

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

  // sg_frame_stats stats = sg_query_frame_stats();

  uint64_t avgFrameInterval = 0.0;
  for (int i = 0; i < 60; i++) {
    avgFrameInterval += app->frameIntervals[i];
  }
  avgFrameInterval /= 60;

  char buff[256];
  if (app->circuit.ux.showFPS) {
    RouteTimeStats rtStats = autoroute_stats(app->circuit.ux.router);

    snprintf(
      buff, sizeof(buff), "Draw time: %1.2f Frame interval: %.2f FPS: %.2f",
      stm_ms(app->lastDrawTime), stm_ms(avgFrameInterval),
      1 / stm_sec(avgFrameInterval));

    Box box = draw_text_bounds(
      &app->draw, HMM_V2(0, height), buff, strlen(buff), ALIGN_LEFT,
      ALIGN_BOTTOM, 20.0, &app->fonsFont);
    draw_screen_text(
      &app->draw, box, buff, strlen(buff), 20.0, &app->fonsFont,
      HMM_V4(1, 1, 1, 1), HMM_V4(0, 0, 0, 0));

    snprintf(
      buff, sizeof(buff),
      "Routing: Build: %.3fms min, %.3fms avg, %.3fms max; Pathing: %.3fms "
      "min, %.3fms "
      "avg, %.3fms max; Samples: %d",
      stm_ms(rtStats.build.min), stm_ms(rtStats.build.avg),
      stm_ms(rtStats.build.max), stm_ms(rtStats.route.min),
      stm_ms(rtStats.route.avg), stm_ms(rtStats.route.max), rtStats.samples);

    box = draw_text_bounds(
      &app->draw, HMM_V2(0, (float)height - (box.halfSize.Y * 2 + 8)), buff,
      strlen(buff), ALIGN_LEFT, ALIGN_BOTTOM, 20.0, &app->fonsFont);
    draw_screen_text(
      &app->draw, box, buff, strlen(buff), 20.0, &app->fonsFont,
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

void platform_init();

sapp_desc sokol_main(int argc, char *argv[]) {
  log_info("Starting sokol_main");
  ux_global_init();
  stm_setup();
  platform_init();

  log_info("Global setup complete");

  my_app_t *app = malloc(sizeof(my_app_t));
  *app = (my_app_t){
    .filename = "testdata/alu_1bit_2gatemux.dig",
  };

  if (argc > 1) {
    app->filename = argv[1];
  }

  log_info("Starting sokol_app");

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
    .sample_count = MSAA_SAMPLE_COUNT,
    .win32_console_attach = true,
  };
}
