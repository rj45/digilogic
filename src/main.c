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

#include "core/core.h"
#define STB_DS_IMPLEMENTATION

#include "stb_ds.h"
#include <stdlib.h>

#include "main.h"
#include "view/view.h"

// #include <assert.h>
// #define NK_ASSERT(expr) assert(expr)

#include "nuklear.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_nuklear.h"

typedef struct {
  CircuitView circuit;
} my_app_t;

static void init(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

  view_init(&app->circuit, circuit_component_descs());

  view_add_component(&app->circuit, COMP_AND, HMM_V2(100, 100));
  view_add_component(&app->circuit, COMP_OR, HMM_V2(200, 200));

  sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
  });
  snk_setup(&(snk_desc_t){.logger.func = slog_func});
}

void cleanup(void *user_data) {
  my_app_t *app = (my_app_t *)user_data;

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

#include <stdio.h>

static void canvas(struct nk_context *ctx, my_app_t *app) {
  struct nk_canvas canvas;
  if (canvas_begin(
        ctx, &canvas, 0, 0, 0, sapp_width(), sapp_height(),
        nk_rgb(0x22, 0x29, 0x33))) {

    view_draw(&app->circuit, canvas.painter);

    // nk_fill_rect(
    //   canvas.painter, nk_rect(x + 15, y + 15, 210, 210), 5,
    //   nk_rgb(247, 230, 154));
    // nk_fill_rect(
    //   canvas.painter, nk_rect(x + 20, y + 20, 200, 200), 5,
    //   nk_rgb(188, 174, 118));
    // /* nk_draw_text(canvas.painter, nk_rect(x + 30, y + 30, 150, 20), "Text
    // to
    //  * draw", 12, &font->handle, nk_rgb(188,174,118), nk_rgb(0,0,0)); */
    // nk_fill_rect(
    //   canvas.painter, nk_rect(x + 250, y + 20, 100, 100), 0, nk_rgb(0, 0,
    //   255));
    // nk_fill_circle(
    //   canvas.painter, nk_rect(x + 20, y + 250, 100, 100), nk_rgb(255, 0, 0));
    // nk_fill_triangle(
    //   canvas.painter, x + 250, y + 250, x + 350, y + 250, x + 300, y + 350,
    //   nk_rgb(0, 255, 0));
    // nk_fill_arc(
    //   canvas.painter, x + 300, y + 420, 50, 0, 3.141592654f * 3.0f / 4.0f,
    //   nk_rgb(255, 255, 0));

    // {
    //   float points[12];
    //   points[0] = x + 200;
    //   points[1] = y + 250;
    //   points[2] = x + 250;
    //   points[3] = y + 350;
    //   points[4] = x + 225;
    //   points[5] = y + 350;
    //   points[6] = x + 200;
    //   points[7] = y + 300;
    //   points[8] = x + 175;
    //   points[9] = y + 350;
    //   points[10] = x + 150;
    //   points[11] = y + 350;
    //   nk_fill_polygon(canvas.painter, points, 6, nk_rgb(0, 0, 0));
    // }

    // {
    //   float points[12];
    //   points[0] = x + 200;
    //   points[1] = y + 370;
    //   points[2] = x + 250;
    //   points[3] = y + 470;
    //   points[4] = x + 225;
    //   points[5] = y + 470;
    //   points[6] = x + 200;
    //   points[7] = y + 420;
    //   points[8] = x + 175;
    //   points[9] = y + 470;
    //   points[10] = x + 150;
    //   points[11] = y + 470;
    //   nk_stroke_polygon(canvas.painter, points, 6, 4, nk_rgb(0, 0, 0));
    // }

    // {
    //   float points[8];
    //   points[0] = x + 250;
    //   points[1] = y + 200;
    //   points[2] = x + 275;
    //   points[3] = y + 220;
    //   points[4] = x + 325;
    //   points[5] = y + 170;
    //   points[6] = x + 350;
    //   points[7] = y + 200;
    //   nk_stroke_polyline(canvas.painter, points, 4, 2, nk_rgb(255, 128, 0));
    // }

    // nk_stroke_line(
    //   canvas.painter, x + 15, y + 10, x + 200, y + 10, 2.0f,
    //   nk_rgb(189, 45, 75));
    // nk_stroke_rect(
    //   canvas.painter, nk_rect(x + 370, y + 20, 100, 100), 10, 3,
    //   nk_rgb(0, 0, 255));
    // nk_stroke_curve(
    //   canvas.painter, x + 380, y + 200, x + 405, y + 270, x + 455, y + 120,
    //   x + 480, y + 200, 2, nk_rgb(0, 150, 220));
    // nk_stroke_circle(
    //   canvas.painter, nk_rect(x + 20, y + 370, 100, 100), 5,
    //   nk_rgb(0, 255, 120));
    // nk_stroke_triangle(
    //   canvas.painter, x + 370, y + 250, x + 470, y + 250, x + 420, y + 350,
    //   6, nk_rgb(255, 0, 143));
    // nk_stroke_arc(
    //   canvas.painter, x + 420, y + 420, 50, 0, 3.141592654f * 3.0f / 4.0f, 5,
    //   nk_rgb(0, 255, 255));
  }
  canvas_end(ctx, &canvas);
}

void draw_filled_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;

  // printf("draw_filled_rect %f %f %f %f\n", );

  nk_fill_rect(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    (struct nk_color){
      color.R * 255, color.G * 255, color.B * 255, color.A * 255});
}

void draw_stroked_rect(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float radius,
  float line_thickness, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_rect(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), radius,
    line_thickness,
    (struct nk_color){
      color.R * 255, color.G * 255, color.B * 255, color.A * 255});
}

void draw_filled_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_fill_circle(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y),
    (struct nk_color){
      color.R * 255, color.G * 255, color.B * 255, color.A * 255});
}

void draw_stroked_circle(
  Context ctx, HMM_Vec2 position, HMM_Vec2 size, float line_thickness,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_circle(
    nk_ctx, nk_rect(position.X, position.Y, size.X, size.Y), line_thickness,
    (struct nk_color){
      color.R * 255, color.G * 255, color.B * 255, color.A * 255});
}

void draw_stroked_line(
  Context ctx, HMM_Vec2 start, HMM_Vec2 end, float line_thickness,
  HMM_Vec4 color) {
  struct nk_command_buffer *nk_ctx = (struct nk_command_buffer *)ctx;
  nk_stroke_line(
    nk_ctx, start.X, start.Y, end.X, end.Y, line_thickness,
    (struct nk_color){
      color.R * 255, color.G * 255, color.B * 255, color.A * 255});
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

  snk_handle_event(event);
}

sapp_desc sokol_main(int argc, char *argv[]) {
  my_app_t *app = malloc(sizeof(my_app_t));
  return (sapp_desc){
    .width = 1024,
    .height = 768,
    .user_data = app,
    .init_userdata_cb = init,
    .frame_userdata_cb = frame,
    .cleanup_userdata_cb = cleanup,
    .event_userdata_cb = event,
    .logger.func = slog_func,
  };
}
