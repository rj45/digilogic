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
#include "core/core.h"
#include "nvdialog.h"
#include "sokol_app.h"
#include "ux/ux.h"
#include <stdbool.h>

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void ui_init(
  CircuitUI *ui, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ui = (CircuitUI){0};
  ux_init(&ui->ux, componentDescs, drawCtx, font);
}

void ui_free(CircuitUI *ui) { ux_free(&ui->ux); }

bool ui_open_file_browser(CircuitUI *ui, bool saving, char *filename) {
  const char *filters = ".dlc;.dig";

  NvdFileDialog *dialog;
  if (saving) {
    dialog = nvd_save_file_dialog_new("Save File", "untitled.dlc");
  } else {
    dialog = nvd_open_file_dialog_new("Open File", filters);
  }

  if (!dialog) {
    return false;
  }

  const char *outfile = NULL;

  nvd_get_file_location(dialog, &outfile);

  if (outfile != NULL) {
    printf("Chosen file: %s\n", outfile);
    strncpy(filename, outfile, 1024);
    free((void *)outfile);
  }

  nvd_free_object(dialog);

  return outfile != NULL;
}

static void ui_menu_bar(CircuitUI *ui, struct nk_context *ctx, float width) {
  if (nk_begin(
        ctx, "Menubar", nk_rect(0, 0, width, ctx->style.font->height + 15),
        0)) {
    nk_menubar_begin(ctx);
    nk_layout_row_begin(ctx, NK_STATIC, ctx->style.font->height + 8, 4);
    nk_layout_row_push(ctx, 45);
    if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "New", NK_TEXT_LEFT)) {
        circuit_clear(&ui->ux.view.circuit);
        ux_route(&ui->ux);
        log_info("New");
      }
      if (nk_menu_item_label(ctx, "Load", NK_TEXT_LEFT)) {
        char filename[1024];

        if (ui_open_file_browser(ui, false, filename)) {
          char *loadfile = filename;
          if (strncmp(filename, "file://", 7) == 0) {
            loadfile += 7;
          }
          if (strncmp(loadfile + strlen(loadfile) - 4, ".dlc", 4) != 0) {
            strncat(loadfile, ".dlc", 1024);
          }
          circuit_clear(&ui->ux.view.circuit);
          circuit_load_file(&ui->ux.view.circuit, loadfile);
          ux_route(&ui->ux);
        }
      }
      if (nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT)) {
        char filename[1024];

        if (ui_open_file_browser(ui, true, filename)) {
          char *savefile = filename;
          if (strncmp(filename, "file://", 7) == 0) {
            savefile += 7;
          }
          if (strncmp(savefile + strlen(savefile) - 4, ".dlc", 4) != 0) {
            strncat(savefile, ".dlc", 1024);
          }
          circuit_save_file(&ui->ux.view.circuit, savefile);
        }
      }
      if (nk_menu_item_label(ctx, "Quit", NK_TEXT_LEFT)) {
        sapp_request_quit();
      }
      nk_menu_end(ctx);
    }
    if (nk_menu_begin_label(ctx, "Edit", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "Undo", NK_TEXT_LEFT)) {
        ux_undo(&ui->ux);
      }
      if (nk_menu_item_label(ctx, "Redo", NK_TEXT_LEFT)) {
        ux_redo(&ui->ux);
      }
      if (nk_menu_item_label(ctx, "Select All", NK_TEXT_LEFT)) {
        ux_select_all(&ui->ux);
      }
      if (nk_menu_item_label(ctx, "Select None", NK_TEXT_LEFT)) {
        ux_select_none(&ui->ux);
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
}

static void
ui_about(CircuitUI *ui, struct nk_context *ctx, float width, float height) {
  // todo: display the entire NOTICE file here
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
}

void ui_update(
  CircuitUI *ui, struct nk_context *ctx, float width, float height) {

  ui_menu_bar(ui, ctx, width);
  ui_about(ui, ctx, width, height);

  if (nk_begin(
        ctx, "Toolbar", nk_rect(0, 30, 180, 480),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE |
          NK_WINDOW_TITLE)) {

    nk_layout_row_dynamic(ctx, 30, 1);
    for (ComponentDescID descID = 0; descID < COMP_COUNT; descID++) {
      const ComponentDesc *desc = &ui->ux.view.circuit.componentDescs[descID];
      if (nk_option_label(ctx, desc->typeName, ui->addingComponent == descID)) {
        if (ui->addingComponent == COMP_NONE && descID != COMP_NONE) {
          ux_start_adding_component(&ui->ux, descID);
        } else if (ui->addingComponent != COMP_NONE && descID == COMP_NONE) {
          ux_stop_adding_component(&ui->ux);
        } else if (ui->addingComponent != descID) {
          ux_change_adding_component(&ui->ux, descID);
        }
        ui->addingComponent = descID;
      }
    }
  }
  nk_end(ctx);

  ux_update(&ui->ux);
}

void ui_draw(CircuitUI *ui) { ux_draw(&ui->ux); }