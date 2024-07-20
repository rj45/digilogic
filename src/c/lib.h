#pragma once

#include "core/core.h"
#include "import/import.h"
#include "ux/ux.h"

typedef struct CLib {
  CircuitUX ux;
  ErrStack errs;
} CLib;

void clib_init(CLib *lib, DrawContext *drawctx, FontHandle font);
void clib_free(CLib *lib);
void clib_update(CLib *lib);
void clib_draw(CLib *lib);