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

#include <stdlib.h>

#define FONS_USE_FREETYPE
#define FONTSTASH_IMPLEMENTATION

#ifdef _WIN32
// for MAX_PATH and CP_UTF8
#include <windows.h>
#endif

#include "assert.h"
#include "fons_sgp.h"
#include "sokol_gfx.h"
#include "sokol_gp.h"

#include "shaders/alphaonly.h"

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////

typedef struct _fsgp_t {
  fsgp_desc_t desc;
  sg_shader shd;
  sg_pipeline pip;
  sg_image img;
  sg_sampler smp;
  int cur_width, cur_height;
  bool img_dirty;

  sgp_vertex verts[FONS_VERTEX_COUNT];
} _fsgp_t;

static void _fsgp_clear(void *ptr, size_t size) {
  assert(ptr && (size > 0));
  memset(ptr, 0, size);
}

static void *_fsgp_malloc(const fsgp_allocator_t *allocator, size_t size) {
  assert(allocator && (size > 0));
  void *ptr;
  if (allocator->alloc_fn) {
    ptr = allocator->alloc_fn(size, allocator->user_data);
  } else {
    ptr = malloc(size);
  }
  assert(ptr);
  return ptr;
}

static void *
_fsgp_malloc_clear(const fsgp_allocator_t *allocator, size_t size) {
  void *ptr = _fsgp_malloc(allocator, size);
  _fsgp_clear(ptr, size);
  return ptr;
}

static void _fsgp_free(const fsgp_allocator_t *allocator, void *ptr) {
  assert(allocator);
  if (allocator->free_fn) {
    allocator->free_fn(ptr, allocator->user_data);
  } else {
    free(ptr);
  }
}

static int _fsgp_render_create(void *user_ptr, int width, int height) {
  assert(user_ptr && (width > 8) && (height > 8));
  _fsgp_t *fsgp = (_fsgp_t *)user_ptr;

  // sokol_gp compatible shader which treats RED channel as alpha
  if (fsgp->shd.id == SG_INVALID_ID) {
    fsgp->shd =
      sg_make_shader(alphaonly_program_shader_desc(sg_query_backend()));
  }

  // sokol_gp pipeline object
  if (fsgp->pip.id == SG_INVALID_ID) {
    sgp_pipeline_desc pip_desc;
    _fsgp_clear(&pip_desc, sizeof(pip_desc));
    pip_desc.shader = fsgp->shd;
    pip_desc.has_vs_color = true;
    pip_desc.blend_mode = SGP_BLENDMODE_BLEND;
    fsgp->pip = sgp_make_pipeline(&pip_desc);
  }

  // a sampler object
  if (fsgp->smp.id == SG_INVALID_ID) {
    sg_sampler_desc smp_desc;
    _fsgp_clear(&smp_desc, sizeof(smp_desc));
    smp_desc.min_filter = SG_FILTER_LINEAR;
    smp_desc.mag_filter = SG_FILTER_LINEAR;
    smp_desc.mipmap_filter = SG_FILTER_NONE;
    fsgp->smp = sg_make_sampler(&smp_desc);
  }

  // create or re-create font atlas texture
  if (fsgp->img.id != SG_INVALID_ID) {
    sg_destroy_image(fsgp->img);
    fsgp->img.id = SG_INVALID_ID;
  }
  fsgp->cur_width = width;
  fsgp->cur_height = height;

  assert(fsgp->img.id == SG_INVALID_ID);
  sg_image_desc img_desc;
  _fsgp_clear(&img_desc, sizeof(img_desc));
  img_desc.width = fsgp->cur_width;
  img_desc.height = fsgp->cur_height;
  img_desc.usage = SG_USAGE_DYNAMIC;
  img_desc.pixel_format = SG_PIXELFORMAT_R8;
  fsgp->img = sg_make_image(&img_desc);
  return 1;
}

static int _fsgp_render_resize(void *user_ptr, int width, int height) {
  return _fsgp_render_create(user_ptr, width, height);
}

static void
_fsgp_render_update(void *user_ptr, int *rect, const unsigned char *data) {
  assert(user_ptr && rect && data);
  (void)(rect);
  (void)(data);
  _fsgp_t *fsgp = (_fsgp_t *)user_ptr;
  fsgp->img_dirty = true;
}

static void _fsgp_render_draw(
  void *user_ptr, const float *verts, const float *tcoords,
  const unsigned int *colors, int nverts) {
  assert(user_ptr && verts && tcoords && colors && (nverts > 0));
  _fsgp_t *fsgp = (_fsgp_t *)user_ptr;

  sgp_set_image(0, fsgp->img);
  sgp_set_sampler(0, fsgp->smp);
  sgp_set_pipeline(fsgp->pip);

  for (int i = 0; i < nverts; i++) {
    fsgp->verts[i].position.x = verts[2 * i + 0];
    fsgp->verts[i].position.y = verts[2 * i + 1];
    fsgp->verts[i].texcoord.x = tcoords[2 * i + 0];
    fsgp->verts[i].texcoord.y = tcoords[2 * i + 1];
    fsgp->verts[i].color = *((sgp_color_ub4 *)(&colors[i]));
  }

  sgp_draw(SG_PRIMITIVETYPE_TRIANGLES, fsgp->verts, nverts);
  sgp_reset_pipeline();
  sgp_reset_sampler(0);
  sgp_reset_image(0);
}

static void _fsgp_render_delete(void *user_ptr) {
  assert(user_ptr);
  _fsgp_t *fsgp = (_fsgp_t *)user_ptr;
  if (fsgp->img.id != SG_INVALID_ID) {
    sg_destroy_image(fsgp->img);
    fsgp->img.id = SG_INVALID_ID;
  }
  if (fsgp->smp.id != SG_INVALID_ID) {
    sg_destroy_sampler(fsgp->smp);
    fsgp->smp.id = SG_INVALID_ID;
  }
  if (fsgp->pip.id != SG_INVALID_ID) {
    sg_destroy_pipeline(fsgp->pip);
    fsgp->pip.id = SG_INVALID_ID;
  }
  if (fsgp->shd.id != SG_INVALID_ID) {
    sg_destroy_shader(fsgp->shd);
    fsgp->shd.id = SG_INVALID_ID;
  }
}

#define _fsgp_def(val, def) (((val) == 0) ? (def) : (val))

static fsgp_desc_t _fsgp_desc_defaults(const fsgp_desc_t *desc) {
  assert(desc);
  fsgp_desc_t res = *desc;
  res.width = _fsgp_def(res.width, 512);
  res.height = _fsgp_def(res.height, 512);
  return res;
}

FONScontext *fsgp_create(const fsgp_desc_t *desc) {
  assert(desc);
  assert(
    (desc->allocator.alloc_fn && desc->allocator.free_fn) ||
    (!desc->allocator.alloc_fn && !desc->allocator.free_fn));
  _fsgp_t *fsgp =
    (_fsgp_t *)_fsgp_malloc_clear(&desc->allocator, sizeof(_fsgp_t));
  fsgp->desc = _fsgp_desc_defaults(desc);
  FONSparams params;
  _fsgp_clear(&params, sizeof(params));
  params.width = fsgp->desc.width;
  params.height = fsgp->desc.height;
  params.flags = FONS_ZERO_TOPLEFT;
  params.renderCreate = _fsgp_render_create;
  params.renderResize = _fsgp_render_resize;
  params.renderUpdate = _fsgp_render_update;
  params.renderDraw = _fsgp_render_draw;
  params.renderDelete = _fsgp_render_delete;
  params.userPtr = fsgp;
  return fonsCreateInternal(&params);
}

void fsgp_destroy(FONScontext *ctx) {
  assert(ctx);
  _fsgp_t *fsgp = (_fsgp_t *)ctx->params.userPtr;
  fonsDeleteInternal(ctx);
  const fsgp_allocator_t allocator = fsgp->desc.allocator;
  _fsgp_free(&allocator, fsgp);
}

void fsgp_flush(FONScontext *ctx) {
  assert(ctx && ctx->params.userPtr);
  _fsgp_t *fsgp = (_fsgp_t *)ctx->params.userPtr;
  if (fsgp->img_dirty) {
    fsgp->img_dirty = false;
    sg_image_data data;
    _fsgp_clear(&data, sizeof(data));
    data.subimage[0][0].ptr = ctx->texData;
    data.subimage[0][0].size = (size_t)(fsgp->cur_width * fsgp->cur_height);
    sg_update_image(fsgp->img, &data);
  }
}

uint32_t fsgp_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16) |
         ((uint32_t)a << 24);
}
