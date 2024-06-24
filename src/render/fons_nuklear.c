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

#include <stdlib.h>

#include "fons_nuklear.h"

static float nuklear_fontstash_width(
  nk_handle handle, float height, const char *text, int len) {
  nuklear_fonstash_font *nufons = handle.ptr;
  FONScontext *fsctx = nufons->fsctx;
  float bounds[4];
  fonsPushState(fsctx);
  fonsClearState(fsctx);
  fonsSetSize(fsctx, height);
  fonsSetFont(fsctx, nufons->font);
  fonsSetAlign(fsctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  fonsTextBounds(fsctx, 0, 0, text, text + len, bounds);
  fonsPopState(fsctx);
  return bounds[2] - bounds[0];
}

static void nuklear_fontstash_query_glyph(
  nk_handle handle, float font_height, struct nk_user_font_glyph *glyph,
  nk_rune codepoint, nk_rune next_codepoint) {
  nuklear_fonstash_font *nufons = handle.ptr;
  FONScontext *fsctx = nufons->fsctx;

  FONStextIter iter;
  FONSquad quad1, quad2, quad2solo;

  fonsPushState(fsctx);
  fonsClearState(fsctx);
  fonsSetSize(fsctx, font_height);
  fonsSetFont(fsctx, nufons->font);
  fonsSetAlign(fsctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

  // TODO: convert codepoints to utf-8
  char text[3] = {codepoint, next_codepoint, 0};

  fonsTextIterInit(fsctx, &iter, 0, 0, text, text + (next_codepoint ? 2 : 1));
  fonsTextIterNext(fsctx, &iter, &quad1);
  if (next_codepoint) {
    fonsTextIterNext(fsctx, &iter, &quad2);
    fonsTextIterInit(fsctx, &iter, 0, 0, text + 1, text + 2);
    fonsTextIterNext(fsctx, &iter, &quad2solo);
  }
  fonsPopState(fsctx);

  glyph->width = quad1.x1 - quad1.x0;
  glyph->height = quad1.y1 - quad1.y0;
  if (next_codepoint) {
    glyph->xadvance = quad2.x0 - (quad1.x0 + quad2solo.x0);
  } else {
    glyph->xadvance = quad1.x1 - quad1.x0;
  }
  glyph->uv[0].x = quad1.s0;
  glyph->uv[0].y = quad1.t0;
  glyph->uv[1].x = quad1.s1;
  glyph->uv[1].y = quad1.t1;

  // not sure about this
  glyph->offset.x = quad1.x0;
  glyph->offset.y = quad1.y0;
}

void fsgp_query_texture(FONScontext *ctx, sg_image *img, sg_sampler *smp);

void nuklear_fontstash_init(
  struct nk_user_font *font, FONScontext *fsctx, int fontNum, float height) {
  nuklear_fonstash_font *nukfonsfont = malloc(sizeof(nuklear_fonstash_font));
  *nukfonsfont = (nuklear_fonstash_font){.fsctx = fsctx, .font = fontNum};

  sg_image img;
  sg_sampler smp;
  fsgp_query_texture(fsctx, &img, &smp);

  snk_image_t snk_img = snk_make_image(&(snk_image_desc_t){
    .image = img,
    .sampler = smp,
  });

  *font = (struct nk_user_font){
    .userdata = nk_handle_ptr(nukfonsfont),
    .height = height,
    .width = nuklear_fontstash_width,
    .query = nuklear_fontstash_query_glyph,
    .texture = snk_nkhandle(snk_img),
  };
}

void nuklear_fontstash_free(struct nk_user_font *font) {
  free(font->userdata.ptr);
  font->userdata.ptr = NULL;
}
