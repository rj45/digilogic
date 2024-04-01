//------------------------------------------------------------------------------
//  sokol_gfx.m
//
//  When using the Metal backend, the implementation source must be
//  Objective-C (.m or .mm), but we want the samples to be in C. Thus
//  move the sokol_gfx implementation into it's own .m file.
//------------------------------------------------------------------------------
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_IMPLEMENTATION

#define SOKOL_IMPL
#define SOKOL_METAL


#include "nuklear.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_nuklear.h"