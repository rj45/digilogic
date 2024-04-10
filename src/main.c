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

#include "view/view.h"
#include <stdio.h>
#define STB_DS_IMPLEMENTATION

#include <stdlib.h>

#include "handmade_math.h"

#include "core/core.h"
#include "main.h"

#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_nuklear.h"
#include "stb_ds.h"

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  ux_init(&app->circuit, circuit_component_descs());

  ComponentID and = ux_add_component(&app->circuit, COMP_AND, HMM_V2(100, 100));
  ComponentID or = ux_add_component(&app->circuit, COMP_OR, HMM_V2(200, 200));

  ux_add_net(
    &app->circuit, view_port_start(&app->circuit.view, and) + 2,
    view_port_start(&app->circuit.view, or));

  ux_add_net(
    &app->circuit, view_port_start(&app->circuit.view, and),
    view_port_start(&app->circuit.view, or) + 2);

  ux_add_net(
    &app->circuit, view_port_start(&app->circuit.view, and) + 1,
    view_port_start(&app->circuit.view, or) + 1);

  ux_route(&app->circuit);

  sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
  });
  snk_setup(&(snk_desc_t){.logger.func = slog_func, .sample_count = 4});
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  ux_free(&app->circuit);

  snk_shutdown();
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

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_line(
    nk_ctx, start.X, start.Y, end.X, end.Y, line_thickness,
    nk_rgba_f(color.R, color.G, color.B, color.A));
}

void frame(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  struct nk_context *ctx = snk_new_frame();
  (void)ctx;

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
  snk_render(sapp_width(), sapp_height());
  sg_end_pass();
  sg_commit();
}

void event(const sapp_event *event, void *user_data) {
  my_app_t *app = (my_app_t *)user_data;
  (void)app;

  if (snk_handle_event(event)) {
    // return;
  }

  switch (event->type) {
  case SAPP_EVENTTYPE_KEY_DOWN:
    bv_set(app->circuit.input.keys, event->key_code);
    app->circuit.input.modifiers = event->modifiers;
    if (event->key_code == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
    }
    break;

  case SAPP_EVENTTYPE_KEY_UP:
    bv_clear(app->circuit.input.keys, event->key_code);
    app->circuit.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_DOWN:
  case SAPP_EVENTTYPE_MOUSE_UP:
    app->circuit.input.modifiers = event->modifiers;
    break;

  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    HMM_Vec2 mousePos = HMM_V2(event->mouse_x, event->mouse_y);
    app->circuit.input.mouseDelta = HMM_Add(
      app->circuit.input.mouseDelta,
      HMM_Sub(app->circuit.input.mousePos, mousePos));
    app->circuit.input.mousePos = mousePos;
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_SCROLL: {
    app->circuit.input.scroll = HMM_Add(
      app->circuit.input.scroll, HMM_V2(event->scroll_x, event->scroll_y));

    break;
  }
  default:
    // ignore
    break;
  }
}

sapp_desc sokol_main(int argc, char *argv[]) {
  my_app_t *app = malloc(sizeof(my_app_t));
  *app = (my_app_t){0};

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
