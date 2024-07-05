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
#include "autoroute/autoroute.h"
#include "core/core.h"
#include "import/import.h"
#include "nfd.h"
#include "nfd/include/nfd.h"
#include "sokol_app.h"
#include "thread.h"
#include "ux/ux.h"
#include <stdbool.h>

#include "sokol_time.h"

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void ui_init(
  CircuitUI *ui, const SymbolDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font) {
  *ui = (CircuitUI){.showIntro = true, .scale = 1.0f};
  ux_init(&ui->ux, componentDescs, drawCtx, font);
  circ_init(&ui->saveCopy);
  thread_mutex_init(&ui->saveMutex);
}

void ui_reset(CircuitUI *ui) {
  ui->showAbout = false;
  ui->addingSymbolKind = NO_ID;

  ux_reset(&ui->ux);
}

void ui_free(CircuitUI *ui) {
  ux_free(&ui->ux);
  circ_free(&ui->saveCopy);
  thread_mutex_term(&ui->saveMutex);
}

static inline int sv(CircuitUI *ui, int value) {
  float newValue = (float)value * ui->scale;
  return (int)(newValue + 0.5);
}

bool ui_open_file_browser(CircuitUI *ui, bool saving, char *filename) {
  const char *filters = "dlc";

  char *outfile = NULL;
  nfdresult_t result;

  if (saving) {
    result = NFD_SaveDialog(filters, "untitled.dlc", &outfile);
  } else {
    result = NFD_OpenDialog(filters, "untitled.dlc", &outfile);
  }

  if (result != NFD_OKAY) {
    return false;
  }

  if (outfile != NULL) {
    printf("Chosen file: %s\n", outfile);
    strncpy(filename, outfile, 1024);
    free((void *)outfile);
  }

  return outfile != NULL;
}

static void ui_menu_bar(CircuitUI *ui, struct nk_context *ctx, float width) {
  struct nk_vec2 padding = ctx->style.window.padding;
  float barHeight = ctx->style.font->height + padding.y * 2;
  nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(padding.x, 0));
  // TODO: figure out how to remove the bottom padding from the menubar
  if (nk_begin(
        ctx, "Menubar",
        nk_rect(
          0, 0, width,
          barHeight + ctx->style.window.spacing.y +
            ctx->style.window.menu_padding.y),
        0)) {
    nk_menubar_begin(ctx);
    nk_layout_row_static(ctx, barHeight, sv(ui, 45), 5);
    if (nk_menu_begin_label(
          ctx, "File", NK_TEXT_LEFT, nk_vec2(sv(ui, 120), sv(ui, 200)))) {
      nk_layout_row_dynamic(ctx, sv(ui, 25), 1);
      if (nk_menu_item_label(ctx, "New", NK_TEXT_LEFT)) {
        ui_reset(ui);
        ui->showIntro = false;
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
          circ_clear(&ui->ux.view.circuit);
          circ_load_file(&ui->ux.view.circuit, loadfile);
          ux_route(&ui->ux);
          circ_commit(&ui->ux.view.circuit);
          ux_build_bvh(&ui->ux);
          ui->showIntro = false;
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
    if (nk_menu_begin_label(
          ctx, "Edit", NK_TEXT_LEFT, nk_vec2(sv(ui, 120), sv(ui, 200)))) {
      nk_layout_row_dynamic(ctx, sv(ui, 25), 1);
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
      nk_spacer(ctx);
      if (nk_menu_item_label(ctx, "Renumber", NK_TEXT_LEFT)) {
        circ_renumber_symbols(&ui->ux.view.circuit, ui->ux.view.circuit.top);
      }
      nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(
          ctx, "View", NK_TEXT_LEFT, nk_vec2(sv(ui, 200), sv(ui, 240)))) {
      nk_layout_row_dynamic(ctx, sv(ui, 25), 1);

      ui->showFPS = ui->ux.showFPS;
      ui->showRoutingDebug = ui->ux.rtDebugLines;
      ui->showBVHDebug = ui->ux.bvhDebugLines;

      nk_checkbox_label(ctx, "Show FPS", &ui->showFPS);
      nk_checkbox_label(ctx, "Show Routing Debug", &ui->showRoutingDebug);
      nk_checkbox_label(ctx, "Show BVH Debug", &ui->showBVHDebug);
      nk_checkbox_label(ctx, "Show Routing Replay", &ui->showReplay);

      ui->ux.showFPS = ui->showFPS;
      ui->ux.rtDebugLines = ui->showRoutingDebug;
      ui->ux.view.debugMode = ui->showRoutingDebug;
      ui->ux.bvhDebugLines = ui->showBVHDebug;

      if (nk_option_label(ctx, "Normal Text", ui->uiScale == 0)) {
        ui_set_scale(ui, 0);
      }
      if (nk_option_label(ctx, "Larger Text", ui->uiScale == 1)) {
        ui_set_scale(ui, 1);
      }
      if (nk_option_label(ctx, "Large Text", ui->uiScale == 2)) {
        ui_set_scale(ui, 2);
      }
      if (nk_option_label(ctx, "Huge Text", ui->uiScale == 3)) {
        ui_set_scale(ui, 3);
      }

      nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(
          ctx, "Help", NK_TEXT_LEFT, nk_vec2(sv(ui, 120), sv(ui, 200)))) {
      nk_layout_row_dynamic(ctx, sv(ui, 25), 1);
      if (nk_menu_item_label(ctx, "Intro Dialog", NK_TEXT_LEFT)) {
        ui->showIntro = true;
      }
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
  // TODO: display the entire NOTICE file here
  float posFactor = 1.0f / 5.0f;
  float sizeFactor = 3.0f / 5.0f;
  if (ui->uiScale > 1) {
    posFactor = 1.0f / 10.0f;
    sizeFactor = 8.0f / 10.0f;
  }
  if (ui->showAbout) {
    if (nk_begin(
          ctx, "About",
          nk_rect(
            width * posFactor, height * posFactor, width * sizeFactor,
            height * sizeFactor),
          NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE)) {
      nk_layout_row_dynamic(ctx, sv(ui, 25), 1);

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

static bool ui_import_data(CircuitUI *ui, char *data) {
  ui_reset(ui);

  if (!import_digital(&ui->ux.view.circuit, data)) {
    return false;
  }

  ux_route(&ui->ux);
  circ_commit(&ui->ux.view.circuit);
  ux_build_bvh(&ui->ux);

  // autoroute_dump_anchor_boxes(ui->circuit.ux.router);

  ui->showIntro = false;

  return true;
}

static bool ui_load_example(CircuitUI *ui, const char *filename) {
  assetsys_file_t file;
  assetsys_file(ui->assetsys, filename, &file);
  int fileSize = assetsys_file_size(ui->assetsys, file);
  char *buffer = malloc(fileSize + 1);
  assetsys_file_load(ui->assetsys, file, &fileSize, buffer, fileSize);
  buffer[fileSize] = 0;

  log_info("Loading file %s, %d bytes\n", filename, fileSize);

  return ui_import_data(ui, buffer);
}

bool ui_import(CircuitUI *ui, const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    return false;
  }

  fseek(fp, 0, SEEK_END);
  int fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buffer = malloc(fileSize + 1);
  fread(buffer, 1, fileSize, fp);
  buffer[fileSize] = 0;
  fclose(fp);

  log_info("Loading file %s, %d bytes\n", filename, fileSize);

  return ui_import_data(ui, buffer);
}

static void ui_intro_dialog(
  CircuitUI *ui, struct nk_context *ctx, float width, float height) {
  if (ui->showIntro) {
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
        ui_reset(ui);
        ui->showIntro = false;
      }

      if (hasAutoSave) {
        if (nk_button_label(ctx, "Load auto-save")) {
          circ_load_file(&ui->ux.view.circuit, platform_autosave_path());
          ux_route(&ui->ux);
          circ_commit(&ui->ux.view.circuit);
          ux_build_bvh(&ui->ux);
          ui->showIntro = false;
        }
      }

      if (nk_button_label(ctx, "Load small sized test circuit")) {
        ui_load_example(ui, "/assets/testdata/simple_test.dig");
      }

      if (nk_button_label(ctx, "Load medium sized test circuit")) {
        ui_load_example(ui, "/assets/testdata/alu_1bit_2gatemux.dig");
      }

      if (nk_button_label(ctx, "Load large sized test circuit")) {
        ui_load_example(ui, "/assets/testdata/alu_1bit_2inpgate.dig");
      }
    }
    nk_end(ctx);
  }
}

static void ui_replay_controls(
  CircuitUI *ui, struct nk_context *ctx, float width, float height) {
  if (ui->showReplay) {
    float ww = sv(ui, 700);
    if (nk_begin(
          ctx, "Replay",
          nk_rect((width - ww) / 2, height - sv(ui, 120), ww, sv(ui, 110)),
          NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
      struct nk_vec2 min = nk_window_get_content_region_min(ctx);
      struct nk_vec2 max = nk_window_get_content_region_max(ctx);

      nk_layout_row_dynamic(ctx, (max.y - min.y - 12) / 2, 8);

      if (nk_button_label(ctx, "|<")) {
        autoroute_replay_rewind(ui->ux.router);
      }

      if (nk_button_label(ctx, "<<<")) {
        autoroute_replay_backward_skip_root(ui->ux.router);
      }

      if (nk_button_label(ctx, "<<")) {
        autoroute_replay_backward_skip_path(ui->ux.router);
      }

      if (
        nk_button_label(ctx, "<") ||
        bv_is_set(ui->ux.input.keysPressed, KEYCODE_LEFT)) {
        autoroute_replay_backward(ui->ux.router);
      }
      char buf[256];
      snprintf(
        buf, 64, "%d / %d", autoroute_replay_current_event(ui->ux.router),
        autoroute_replay_event_count(ui->ux.router));

      nk_label(ctx, buf, NK_TEXT_CENTERED);

      if (
        nk_button_label(ctx, ">") ||
        bv_is_set(ui->ux.input.keysPressed, KEYCODE_RIGHT)) {
        autoroute_replay_forward(ui->ux.router);
      }

      if (nk_button_label(ctx, ">>")) {
        autoroute_replay_forward_skip_path(ui->ux.router);
      }

      if (nk_button_label(ctx, ">>>")) {
        autoroute_replay_forward_skip_root(ui->ux.router);
      }

      nk_layout_row_dynamic(ctx, (max.y - min.y - 12) / 2, 1);

      autoroute_replay_event_text(ui->ux.router, buf, sizeof(buf));
      nk_label(ctx, buf, NK_TEXT_CENTERED);
    }
    nk_end(ctx);
  }
}

static void
ui_toolbox(CircuitUI *ui, struct nk_context *ctx, float width, float height) {
  if (!ui->showIntro) {
    if (nk_begin(
          ctx, "Toolbar", nk_rect(0, sv(ui, 40), 180, 480),
          NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE |
            NK_WINDOW_TITLE)) {

      nk_layout_row_dynamic(ctx, sv(ui, 30), 1);
      if (nk_option_label(ctx, "NONE", ui->ux.tool == TOOL_NONE)) {
        ui->ux.tool = TOOL_NONE;
        if (ui->addingSymbolKind != NO_ID) {
          ux_stop_adding_symbol(&ui->ux);
        }
        ui->addingSymbolKind = NO_ID;
        ux_stop_adding_waypoint(&ui->ux);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Routing", NK_MAXIMIZED)) {
        nk_layout_row_dynamic(ctx, sv(ui, 30), 1);
        if (nk_option_label(ctx, "Waypoint", ui->ux.tool == TOOL_WAYPOINT)) {
          ui->ux.tool = TOOL_WAYPOINT;
          ux_start_adding_waypoint(&ui->ux);
          if (ui->addingSymbolKind != NO_ID) {
            ux_stop_adding_symbol(&ui->ux);
          }
          ui->addingSymbolKind = NO_ID;
        }
        nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Components", NK_MAXIMIZED)) {
        nk_layout_row_dynamic(ctx, sv(ui, 30), 1);

        CircuitIter iter = circ_iter(&ui->ux.view.circuit, SymbolKind);
        while (circ_iter_next(&iter)) {
          SymbolKind *table = circ_iter_table(&iter, SymbolKind);
          for (ptrdiff_t i = 0; i < table->length; i++) {
            SymbolKindID symbolKindID = table->id[i];
            Name nameID = circ_get(&ui->ux.view.circuit, symbolKindID, Name);
            if (nameID == 0) {
              continue;
            }
            const char *name = circ_str_get(&ui->ux.view.circuit, nameID);

            if (nk_option_label(
                  ctx, name,
                  ui->ux.tool == TOOL_SYMBOL &&
                    ui->addingSymbolKind == symbolKindID)) {
              ui->ux.tool = TOOL_SYMBOL;
              ux_stop_adding_waypoint(&ui->ux);
              if (ui->addingSymbolKind == NO_ID) {
                ux_start_adding_symbol(&ui->ux, symbolKindID);
              } else if (ui->addingSymbolKind != symbolKindID) {
                ux_change_adding_symbol(&ui->ux, symbolKindID);
              }
              ui->addingSymbolKind = symbolKindID;
            }
          }
        }
        nk_tree_pop(ctx);
      }
    }
    nk_end(ctx);
  }
}

void ui_update(
  CircuitUI *ui, struct nk_context *ctx, float width, float height) {

  ((struct nk_user_font *)ctx->style.font)->height = sv(ui, UI_FONT_SIZE);

  ui_menu_bar(ui, ctx, width);
  ui_about(ui, ctx, width, height);
  ui_intro_dialog(ui, ctx, width, height);
  ui_replay_controls(ui, ctx, width, height);
  ui_toolbox(ui, ctx, width, height);

  ux_update(&ui->ux);

  if (bv_is_set(ui->ux.input.keysPressed, KEYCODE_R)) {
    ui->showReplay = !ui->showReplay;
  }

  ui->ux.view.hideNets = ui->showReplay;
  if (ui->showReplay && !ui->ux.routingConfig.recordReplay) {
    ui->ux.routingConfig.recordReplay = true;
    ux_route(&ui->ux);
  }

  if (ui->ux.changed) {
    ui->ux.changed = false;

    if (ui->saveAt == 0) {
      ui->saveAt = stm_now();
    }
  }

  // FIXME: this somehow causes millions of saves per second, which is really
  // bad
  // if (ui->saveAt != 0 && stm_sec(stm_since(ui->saveAt)) > 1) {
  //   log_info("Autosaving to %s", platform_autosave_path());
  //   if (ui_background_save(ui, platform_autosave_path(), true)) {
  //     ui->saveAt = 0;
  //   }
  // }
}

void ui_draw(CircuitUI *ui) {
  ux_draw(&ui->ux);
  if (ui->showReplay) {
    autoroute_replay_draw(
      ui->ux.router, ui->ux.view.drawCtx, ui->ux.view.theme.font);
  }
}

static int ui_do_save(void *data) {
  CircuitUI *ui = (CircuitUI *)data;
  thread_mutex_lock(&ui->saveMutex);
  circ_save_file(&ui->saveCopy, ui->saveFilename);
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
  circ_clone(&ui->saveCopy, &ui->ux.view.circuit);
  strncpy(ui->saveFilename, filename, 1024);
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

float scaleFactors[] = {1.0f, 1.2f, 1.5f, 2.0f};

void ui_set_scale(CircuitUI *ui, int scale) {
  ui->uiScale = scale;
  ui->scale = scaleFactors[scale];
}
