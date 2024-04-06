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

#define MAX_ZOOM 20.0f

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  // ensure the keys array is big enough to fit all the keys
  bv_setlen(app->keys, SAPP_KEYCODE_MENU + 1);
  bv_clear_all(app->keys);

  view_init(&app->circuit, circuit_component_descs());

  ComponentID and =
    view_add_component(&app->circuit, COMP_AND, HMM_V2(100, 100));
  ComponentID or = view_add_component(&app->circuit, COMP_OR, HMM_V2(200, 200));

  view_add_net(
    &app->circuit, view_port_start(&app->circuit, and) + 2,
    view_port_start(&app->circuit, or));

  view_add_net(
    &app->circuit, view_port_start(&app->circuit, and),
    view_port_start(&app->circuit, or) + 2);

  view_add_net(
    &app->circuit, view_port_start(&app->circuit, and) + 1,
    view_port_start(&app->circuit, or) + 1);

  view_route(&app->circuit);

  sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
  });
  snk_setup(&(snk_desc_t){.logger.func = slog_func, .sample_count = 4});
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  bv_free(app->keys);

  view_free(&app->circuit);

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

    if (nk_window_is_hovered(ctx)) {
      float dt = (float)sapp_frame_duration();
      if (bv_is_set(app->keys, SAPP_KEYCODE_W)) {
        app->circuit.pan.Y -= 600.0f * dt * app->circuit.zoom;
      }
      if (bv_is_set(app->keys, SAPP_KEYCODE_A)) {
        app->circuit.pan.X -= 600.0f * dt * app->circuit.zoom;
      }
      if (bv_is_set(app->keys, SAPP_KEYCODE_S)) {
        app->circuit.pan.Y += 600.0f * dt * app->circuit.zoom;
      }
      if (bv_is_set(app->keys, SAPP_KEYCODE_D)) {
        app->circuit.pan.X += 600.0f * dt * app->circuit.zoom;
      }
    }
    view_draw(&app->circuit, canvas.painter);
  }
  canvas_end(ctx, &canvas);
}

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;

  // printf("draw_filled_rect %f %f %f %f\n", );

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
    bv_set(app->keys, event->key_code);
    if (event->key_code == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
    }
    break;
  case SAPP_EVENTTYPE_KEY_UP:
    bv_clear(app->keys, event->key_code);
    break;
  case SAPP_EVENTTYPE_MOUSE_SCROLL: {
    // calculate the new zoom
    app->circuit.zoomExp += event->scroll_y * 0.5f;
    if (app->circuit.zoomExp < -MAX_ZOOM) {
      app->circuit.zoomExp = -MAX_ZOOM;
    } else if (app->circuit.zoomExp > MAX_ZOOM) {
      app->circuit.zoomExp = MAX_ZOOM;
    }
    float newZoom = powf(1.1f, app->circuit.zoomExp);
    float oldZoom = app->circuit.zoom;
    app->circuit.zoom = newZoom;

    // figure out where the mouse was in "world coords" with the old zoom
    HMM_Vec2 originalMousePos = HMM_DivV2F(
      HMM_Sub(HMM_V2(event->mouse_x, event->mouse_y), app->circuit.pan),
      oldZoom);

    // figure out where the mouse is in "world coords" with the new zoom
    HMM_Vec2 newMousePos = HMM_DivV2F(
      HMM_Sub(HMM_V2(event->mouse_x, event->mouse_y), app->circuit.pan),
      newZoom);

    // figure out the correction to the pan so that the zoom is centred on the
    // mouse position
    HMM_Vec2 correction =
      HMM_MulV2F(HMM_Sub(newMousePos, originalMousePos), newZoom);
    app->circuit.pan = HMM_Add(app->circuit.pan, correction);

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
