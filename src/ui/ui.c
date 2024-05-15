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

#include "ui/ui.h"
#include "sokol_app.h"

void ui_init(
  CircuitUI *ui, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ui = (CircuitUI){0};
  ux_init(&ui->ux, componentDescs, drawCtx, font);
}

void ui_free(CircuitUI *ui) { ux_free(&ui->ux); }

void ui_update(
  CircuitUI *ui, struct nk_context *ctx, float width, float height) {

  if (nk_begin(
        ctx, "Menubar", nk_rect(0, 0, width, ctx->style.font->height + 15),
        0)) {
    nk_menubar_begin(ctx);
    nk_layout_row_begin(ctx, NK_STATIC, ctx->style.font->height + 8, 4);
    nk_layout_row_push(ctx, 45);
    if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "New", NK_TEXT_LEFT)) {
        printf("New\n");
      }
      if (nk_menu_item_label(ctx, "Load", NK_TEXT_LEFT)) {
        printf("Load\n");
      }
      if (nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT)) {
        printf("Save\n");
      }
      if (nk_menu_item_label(ctx, "Quit", NK_TEXT_LEFT)) {
        sapp_request_quit();
      }
      nk_menu_end(ctx);
    }
    if (nk_menu_begin_label(ctx, "Edit", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "Undo", NK_TEXT_LEFT)) {
        printf("Undo\n");
      }
      if (nk_menu_item_label(ctx, "Redo", NK_TEXT_LEFT)) {
        printf("Redo\n");
      }
      if (nk_menu_item_label(ctx, "Select All", NK_TEXT_LEFT)) {
        printf("Select All\n");
      }
      if (nk_menu_item_label(ctx, "Select None", NK_TEXT_LEFT)) {
        printf("Select None\n");
      }
      nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(ctx, "Help", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "About", NK_TEXT_LEFT)) {
        ui->showAbout = true;
      }
      nk_menu_end(ctx);
    }
    nk_menubar_end(ctx);
  }
  nk_end(ctx);

  if (ui->showAbout) {
    if (nk_begin(
          ctx, "About",
          nk_rect(width / 5, height / 5, width * 3 / 5, height * 3 / 5),
          NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE)) {
      nk_layout_row_dynamic(ctx, 25, 1);
      nk_label(ctx, "digilogic", NK_TEXT_CENTERED);
      nk_label(ctx, "", NK_TEXT_CENTERED);
      nk_label(
        ctx, "Copyright (c) 2024 Ryan \"rj45\" Sanche", NK_TEXT_CENTERED);
      nk_label(ctx, "All rights reserved.", NK_TEXT_CENTERED);
      nk_label(
        ctx, "Licensed under the Apache License, Version 2.0",
        NK_TEXT_CENTERED);
      nk_label(ctx, "", NK_TEXT_CENTERED);
      nk_label(
        ctx,
        "Note: This is open source software, but it took a lot of effort to "
        "create!",
        NK_TEXT_CENTERED);
      nk_label(
        ctx,
        "Please consider donating to support the development of this software!",
        NK_TEXT_CENTERED);
      nk_label(ctx, "", NK_TEXT_CENTERED);
      nk_label(
        ctx, "You can donate at: https://ko-fi.com/rj45_creates",
        NK_TEXT_CENTERED);
    } else {
      ui->showAbout = false;
    }
    nk_end(ctx);
  }

  /*if (nk_begin(
        ctx, "Toolbar", nk_rect(0, 30, 180, 480),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE |
          NK_WINDOW_TITLE)) {

    nk_layout_row_dynamic(ctx, 30, 2);
    if (nk_option_label(ctx, "Select", ui->tool == TOOL_SELECT))
      ui->tool = TOOL_SELECT;
    if (nk_option_label(ctx, "Move", ui->tool == TOOL_MOVE))
      ui->tool = TOOL_MOVE;
    if (nk_option_label(ctx, "Wire", ui->tool == TOOL_WIRE))
      ui->tool = TOOL_WIRE;
    if (nk_option_label(ctx, "Component", ui->tool == TOOL_COMPONENT))
      ui->tool = TOOL_COMPONENT;
    if (nk_option_label(ctx, "Pan", ui->tool == TOOL_PAN))
      ui->tool = TOOL_PAN;
  }
  nk_end(ctx);*/

  ux_update(&ui->ux);
}

void ui_draw(CircuitUI *ui) { ux_draw(&ui->ux); }