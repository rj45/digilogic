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
#include "thread.h"
#include "ux/ux.h"
#include <stdbool.h>

#include "sokol_time.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void ui_init(
  CircuitUI *ui, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ui = (CircuitUI){0};
  ux_init(&ui->ux, componentDescs, drawCtx, font);
  circuit_init(&ui->saveCopy, componentDescs);
  thread_mutex_init(&ui->saveMutex);
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
  struct nk_vec2 padding = ctx->style.window.padding;
  float barHeight = ctx->style.font->height + padding.y * 2;
  nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(padding.x, 0));
  // todo: figure out how to remove the bottom padding from the menubar
  if (nk_begin(
        ctx, "Menubar",
        nk_rect(
          0, 0, width,
          barHeight + ctx->style.window.spacing.y +
            ctx->style.window.menu_padding.y),
        0)) {
    nk_menubar_begin(ctx);
    nk_layout_row_static(ctx, barHeight, 45, 4);
    if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200))) {
      nk_layout_row_dynamic(ctx, 25, 1);
      if (nk_menu_item_label(ctx, "New", NK_TEXT_LEFT)) {
        circuit_clear(&ui->ux.view.circuit);
        ux_route(&ui->ux);
        ux_build_bvh(&ui->ux);
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
          ux_build_bvh(&ui->ux);
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

          ui_background_save(ui, savefile, false);
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
  nk_style_pop_vec2(ctx);
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
        "Note: This is open source software, but it took a lot of time and "
        "effort to create!",
        NK_TEXT_CENTERED);
      nk_label(
        ctx,
        "Please consider paying for this software to support its development.",
        NK_TEXT_CENTERED);
      nk_label(ctx, "", NK_TEXT_CENTERED);
      nk_label(
        ctx,
        "You can donate what you would pay for software like this at one of "
        "these links:",
        NK_TEXT_CENTERED);
      nk_label(ctx, "https://ko-fi.com/rj45_creates", NK_TEXT_CENTERED);
      nk_label(ctx, "https://github.com/sponsors/rj45", NK_TEXT_CENTERED);
      nk_label(ctx, "https://www.patreon.com/rj45Creates", NK_TEXT_CENTERED);
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

  if (ui->ux.changed) {
    ui->ux.changed = false;

    if (ui->saveAt == 0) {
      ui->saveAt = stm_now();
    }
  }

  if (ui->saveAt != 0 && stm_sec(stm_since(ui->saveAt)) > 1) {
    log_info("Autosaving to %s", platform_autosave_path());
    if (ui_background_save(ui, platform_autosave_path(), true)) {
      ui->saveAt = 0;
    }
  }
}

void ui_draw(CircuitUI *ui) { ux_draw(&ui->ux); }

static int ui_do_save(void *data) {
  CircuitUI *ui = (CircuitUI *)data;
  thread_mutex_lock(&ui->saveMutex);
  circuit_save_file(&ui->saveCopy, ui->saveFilename);
  thread_atomic_int_store(&ui->saveThreadBusy, 0);
  thread_mutex_unlock(&ui->saveMutex);
  return 0;
}

bool ui_background_save(
  CircuitUI *ui, const char *filename, bool skipWhenBusy) {
  if (skipWhenBusy && thread_atomic_int_load(&ui->saveThreadBusy)) {
    return false;
  }
  thread_mutex_lock(&ui->saveMutex);
  circuit_clone_from(&ui->saveCopy, &ui->ux.view.circuit);
  memcpy(ui->saveFilename, filename, 1024);
  thread_atomic_int_store(&ui->saveThreadBusy, 1);
  thread_mutex_unlock(&ui->saveMutex);

  thread_ptr_t thread =
    thread_create(ui_do_save, ui, THREAD_STACK_SIZE_DEFAULT);
  if (thread == NULL) {
    log_error("Failed to create save thread");
    return false;
  }
  thread_detach(thread);

  return true;
}