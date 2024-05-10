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
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS

#include "ux/ux.h"
#include "view/view.h"
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

#define FONT_ATLAS_WIDTH 1024
#define FONT_ATLAS_HEIGHT 1024

typedef struct my_app_t {
  CircuitUX circuit;

  const char *filename;

  bool loaded;

  assetsys_t *assetsys;

  FONScontext *fsctx;
  FonsFont fonsFont;

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
    .max_vertices = 64 * 1024, .logger.func = slog_func, .sample_count = 4});

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

  draw_init(&app->draw, app->fsctx);

  ux_init(
    &app->circuit, circuit_component_descs(), &app->draw,
    (FontHandle)&app->fonsFont);

  app->pzoom = draw_get_zoom(&app->draw);
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  draw_free(&app->draw);

  ux_free(&app->circuit);

  fsgp_destroy(app->fsctx);

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

  import_digital(&app->circuit, buffer);
  ux_route(&app->circuit);

  // autoroute_dump_anchor_boxes(app->circuit.router);

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

  int width = sapp_width(), height = sapp_height();
  sgp_begin(width, height);
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);

  if (!app->loaded) {
    if (nk_begin(
          ctx, "Load example file",
          nk_rect(
            (float)width / 3, (float)height / 3, (float)width / 3,
            (float)height / 3),
          NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
      struct nk_vec2 min = nk_window_get_content_region_min(ctx);
      struct nk_vec2 max = nk_window_get_content_region_max(ctx);
      float height = ((max.y - min.y) / 3) - 5;
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

  app->circuit.input.frameDuration = sapp_frame_duration();
  ux_draw(&app->circuit);
  app->circuit.input.scroll = HMM_V2(0, 0);
  app->circuit.input.mouseDelta = HMM_V2(0, 0);

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

  sg_frame_stats stats = sg_query_frame_stats();

  uint64_t avgFrameInterval = 0.0;
  for (int i = 0; i < 60; i++) {
    avgFrameInterval += app->frameIntervals[i];
  }
  avgFrameInterval /= 60;

  char buff[256];
  if (app->circuit.showFPS) {
    snprintf(
      buff, sizeof(buff),
      "Draw time: %1.2f Frame interval: %.2f FPS: %.2f Draws: %d Appends %d",
      stm_ms(app->lastDrawTime), stm_ms(avgFrameInterval),
      1 / stm_sec(avgFrameInterval), stats.num_draw, stats.num_append_buffer);

    Box box = draw_text_bounds(
      &app->draw, HMM_V2(0, 0), buff, strlen(buff), ALIGN_LEFT, ALIGN_TOP, 12.0,
      &app->fonsFont);
    draw_text(
      &app->draw, box, buff, strlen(buff), 16.0, &app->fonsFont,
      HMM_V4(1, 1, 1, 1), HMM_V4(0, 0, 0, 0));
  }

  sgp_flush();
  sgp_end();
  sg_end_pass();
  sg_commit();

  bv_clear_all(app->circuit.input.keysPressed);

  uint64_t frameInterval = stm_laptime(&app->frameIntervalTime);

  app->frameIntervals[app->frameIntervalIndex] = frameInterval;
  app->frameIntervalIndex = (app->frameIntervalIndex + 1) % 60;

  app->lastDrawTime = stm_since(frameStart);
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
  stm_setup();

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
