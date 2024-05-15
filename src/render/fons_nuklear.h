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

#ifndef FONS_NUKLEAR_H
#define FONS_NUKLEAR_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_SOFTWARE_FONT

#include "fontstash.h"
#include "nuklear.h"

#include "sokol_app.h"
#include "sokol_gfx.h"

#include "render/sokol_nuklear.h"

typedef struct nuklear_fonstash_font {
  FONScontext *fsctx;
  int font;
} nuklear_fonstash_font;

void nuklear_fontstash_init(
  struct nk_user_font *font, FONScontext *fsctx, int fontNum, float height);

void nuklear_fontstash_free(struct nk_user_font *font);

#endif // FONS_NUKLEAR_H