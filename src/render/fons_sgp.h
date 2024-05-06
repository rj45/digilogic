/*
    This is an adaptation of sokol_fonstash.h to work with sokol_gp.h

    LICENSE
    =======
    zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    Copyright (c) 2024 Ryan "rj45" Sanche

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in
   a product, an acknowledgment in the product documentation would be
        appreciated but is not required.

        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.

        3. This notice may not be removed or altered from any source
        distribution.
*/

#ifndef FONS_SGP_H
#define FONS_SGP_H

#include <stddef.h>
#include <stdint.h>

#include "fontstash.h"

typedef struct fsgp_allocator_t {
  void *(*alloc_fn)(size_t size, void *user_data);
  void (*free_fn)(void *ptr, void *user_data);
  void *user_data;
} fsgp_allocator_t;

typedef struct fsgp_desc_t {
  int width; // initial width of font atlas texture (default: 512, must be power
             // of 2)
  int height; // initial height of font atlas texture (default: 512, must be
              // power of 2)
  fsgp_allocator_t allocator; // optional memory allocation overrides
} fsgp_desc_t;

FONScontext *fsgp_create(const fsgp_desc_t *desc);
void fsgp_destroy(FONScontext *ctx);
void fsgp_flush(FONScontext *ctx);
uint32_t fsgp_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif // FONS_SGP_H
