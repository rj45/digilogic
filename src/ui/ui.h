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

#ifndef UI_H
#define UI_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_SOFTWARE_FONT

#include "assetsys.h"
#include "nuklear.h"
#include "thread.h"
#include "ux/ux.h"

////////////////////////////////////////
// Circuit UI
////////////////////////////////////////

#define UI_FONT_SIZE 20

typedef struct CircuitUI {
  CircuitUX ux;
  struct nk_context *nk;

  int uiScale;
  float scale;

  ID addingSymbolKind;

  bool saving;
  bool showAbout;
  bool showIntro;
  bool showReplay;

  Circuit saveCopy;
  thread_atomic_int_t saveThreadBusy;
  char saveFilename[1024];
  thread_mutex_t saveMutex;
  uint64_t saveAt;

  // for loading assets, not owned by UI
  assetsys_t *assetsys;
} CircuitUI;

void ui_init(
  CircuitUI *ui, const ComponentDesc *componentDescs, DrawContext *drawCtx,
  FontHandle font);
void ui_free(CircuitUI *ui);
void ui_update(
  CircuitUI *ui, struct nk_context *ctx, float width, float height);
void ui_draw(CircuitUI *ui);
bool ui_background_save(CircuitUI *ui, const char *filename, bool skipWhenBusy);
void ui_reset(CircuitUI *ui);
void ui_set_scale(CircuitUI *ui, int scale);

#endif // UI_H