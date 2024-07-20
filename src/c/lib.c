#define STRPOOL_IMPLEMENTATION
#define SOKOL_IMPL
#define STB_DS_IMPLEMENTATION

#include "sokol_time.h"
#include "strpool.h"

#include "lib.h"

void clib_init(CLib *lib, DrawContext *drawctx, FontHandle font) {
  *lib = (CLib){0};
  errstack_init(&lib->errs);

  ux_init(&lib->ux, &lib->errs, circuit_symbol_descs(), drawctx, font);
}

void clib_free(CLib *lib) {
  ux_free(&lib->ux);
  // errstack_free(&lib->errs);
}

void clib_update(CLib *lib) { ux_update(&lib->ux); }

void clib_draw(CLib *lib) { ux_draw(&lib->ux); }

// TODO: move this to rust
Box draw_text_bounds(DrawContext *draw, HMM_Vec2 pos, const char *text, int len,
                     HorizAlign horz, VertAlign vert, float fontSize,
                     FontHandle font) {}