#if defined(SOKOL_IMPL) && !defined(SOKOL_NUKLEAR_IMPL)
#define SOKOL_NUKLEAR_IMPL
#endif
#ifndef SOKOL_NUKLEAR_INCLUDED
/*
    sokol_nuklear.h -- drop-in Nuklear renderer/event-handler for sokol_gfx.h

    Project URL: https://github.com/floooh/sokol

    Do this:
        #define SOKOL_IMPL or
        #define SOKOL_NUKLEAR_IMPL

    before you include this file in *one* C or C++ file to create the
    implementation.

    The following defines are used by the implementation to select the
    platform-specific embedded shader code (these are the same defines as
    used by sokol_gfx.h and sokol_app.h):

    SOKOL_GLCORE33
    SOKOL_GLES3
    SOKOL_D3D11
    SOKOL_METAL
    SOKOL_WGPU

    Optionally provide the following configuration defines before including the
    implementation:

    SOKOL_NUKLEAR_NO_SOKOL_APP    - don't depend on sokol_app.h (see below for
   details)

    Optionally provide the following macros before including the implementation
    to override defaults:

    SOKOL_ASSERT(c)     - your own assert macro (default: assert(c))
    SOKOL_NUKLEAR_API_DECL- public function declaration prefix (default: extern)
    SOKOL_API_DECL      - same as SOKOL_NUKLEAR_API_DECL
    SOKOL_API_IMPL      - public function implementation prefix (default: -)

    If sokol_nuklear.h is compiled as a DLL, define the following before
    including the declaration or implementation:

    SOKOL_DLL

    On Windows, SOKOL_DLL will define SOKOL_NUKLEAR_API_DECL as
   __declspec(dllexport) or __declspec(dllimport) as needed.

    Include the following headers before sokol_nuklear.h (both before including
    the declaration and implementation):

        sokol_gfx.h
        sokol_app.h     (except SOKOL_NUKLEAR_NO_SOKOL_APP)
        nuklear.h

    NOTE: Unlike most other sokol-headers, the implementation must be compiled
    as C, compiling as C++ isn't supported. The interface is callable
    from C++ of course.


    FEATURE OVERVIEW:
    =================
    sokol_nuklear.h implements the initialization, rendering and event-handling
    code for Nuklear (https://github.com/Immediate-Mode-UI/Nuklear) on top of
    sokol_gfx.h and (optionally) sokol_app.h.

    The sokol_app.h dependency is optional and used for input event handling.
    If you only use sokol_gfx.h but not sokol_app.h in your application,
    define SOKOL_NUKLEAR_NO_SOKOL_APP before including the implementation
    of sokol_nuklear.h, this will remove any dependency to sokol_app.h, but
    you must feed input events into Nuklear yourself.

    sokol_nuklear.h is not thread-safe, all calls must be made from the
    same thread where sokol_gfx.h is running.

    HOWTO:
    ======

    --- To initialize sokol-nuklear, call:

        snk_setup(const snk_desc_t* desc)

        This will initialize Nuklear and create sokol-gfx resources
        (two buffers for vertices and indices, a font texture and a pipeline-
        state-object).

        Use the following snk_desc_t members to configure behaviour:

            int max_vertices
                The maximum number of vertices used for UI rendering, default is
   65536. sokol-nuklear will use this to compute the size of the vertex- and
   index-buffers allocated via sokol_gfx.h

            int image_pool_size
                Number of snk_image_t objects which can be alive at the same
   time. The default is 256.

            sg_pixel_format color_format
                The color pixel format of the render pass where the UI
                will be rendered. The default is SG_PIXELFORMAT_RGBA8

            sg_pixel_format depth_format
                The depth-buffer pixel format of the render pass where
                the UI will be rendered. The default is
   SG_PIXELFORMAT_DEPTHSTENCIL.

            int sample_count
                The MSAA sample-count of the render pass where the UI
                will be rendered. The default is 1.

            float dpi_scale
                DPI scaling factor. Set this to the result of sapp_dpi_scale().
                To render in high resolution on a Retina Mac this would
                typically be 2.0. The default value is 1.0

            bool no_default_font
                Set this to true if you don't want to use Nuklear's default
                font. In this case you need to initialize the font
                yourself after snk_setup() is called.

    --- At the start of a frame, call:

        struct nk_context *snk_new_frame()

        This updates Nuklear's event handling state and then returns
        a Nuklear context pointer which you use to build the UI. For
        example:

        struct nk_context *ctx = snk_new_frame();
        if (nk_begin(ctx, "Demo", nk_rect(50, 50, 200, 200), ...


    --- at the end of the frame, before the sg_end_pass() where you
        want to render the UI, call:

        snk_render(int width, int height)

        where 'width' and 'height' are the dimensions of the rendering surface.
        For example, if you're using sokol_app.h and render to the default
        framebuffer:

        snk_render(sapp_width(), sapp_height());

        This will convert Nuklear's command list into a vertex and index buffer,
        and then render that through sokol_gfx.h

    --- if you're using sokol_app.h, from inside the sokol_app.h event callback,
        call:

        bool snk_handle_event(const sapp_event* ev);

        This will feed the event into Nuklear's event handling code, and return
        true if the event was handled by Nuklear, or false if the event should
        be handled by the application.

    --- finally, on application shutdown, call

        snk_shutdown()

    --- Note that for touch-based systems, like iOS, there is a wrapper around
        nk_edit_string(...), called snk_edit_string(...) which will show
        and hide the onscreen keyboard as required.


    ON USER-PROVIDED IMAGES AND SAMPLERS
    ====================================
    To render your own images via nk_image(), first create an snk_image_t
    object from a sokol-gfx image and sampler object.

        // create a sokol-nuklear image object which associates an sg_image with
   an sg_sampler snk_image_t snk_img = snk_make_image(&(snk_image_desc_t){
            .image = sg_make_image(...),
            .sampler = sg_make_sampler(...),
        });

        // convert the returned image handle into an nk_handle object
        struct nk_handle nk_hnd = snk_nkhandle(snk_img);

        // create a nuklear image from the generic handle (note that there a
   different helper functions for this) struct nk_image nk_img =
   nk_image_handle(nk_hnd);

        // finally specify a Nuklear image UI object
        nk_image(ctx, nk_img);

    snk_image_t objects are small and cheap (literally just the image and
   sampler handle).

    You can omit the sampler handle in the snk_make_image() call, in this case a
    default sampler will be used with nearest-filtering and clamp-to-edge.

    Trying to render with an invalid snk_image_t handle will render a small 8x8
    white default texture instead.

    To destroy a sokol-nuklear image object, call

        snk_destroy_image(snk_img);

    But please be aware that the image object needs to be around until
   snk_render() is called in a frame (if this turns out to be too much of a
   hassle we could introduce some sort of garbage collection where destroyed
   snk_image_t objects are kept around until the snk_render() call).

    You can call:

        snk_image_desc_t desc = snk_query_image_desc(img)

    ...to get the original desc struct, useful if you need to get the sokol-gfx
   image and sampler handle of the snk_image_t object.

    You can convert an nk_handle back into an snk_image_t handle:

        snk_image_t img = snk_image_from_nkhandle(nk_hnd);


    MEMORY ALLOCATION OVERRIDE
    ==========================
    You can override the memory allocation functions at initialization time
    like this:

        void* my_alloc(size_t size, void* user_data) {
            return malloc(size);
        }

        void my_free(void* ptr, void* user_data) {
            free(ptr);
        }

        ...
            snk_setup(&(snk_desc_t){
                // ...
                .allocator = {
                    .alloc_fn = my_alloc,
                    .free_fn = my_free,
                    .user_data = ...;
                }
            });
        ...

    If no overrides are provided, malloc and free will be used.

    This only affects memory allocation calls done by sokol_nuklear.h
    itself though, not any allocations in Nuklear.


    ERROR REPORTING AND LOGGING
    ===========================
    To get any logging information at all you need to provide a logging callback
   in the setup call the easiest way is to use sokol_log.h:

        #include "sokol_log.h"

        snk_setup(&(snk_desc_t){
            .logger.func = slog_func
        });

    To override logging with your own callback, first write a logging function
   like this:

        void my_log(const char* tag,                // e.g. 'snk'
                    uint32_t log_level,             // 0=panic, 1=error, 2=warn,
   3=info uint32_t log_item_id,           // SNK_LOGITEM_* const char*
   message_or_null,    // a message string, may be nullptr in release mode
                    uint32_t line_nr,               // line number in
   sokol_nuklear.h const char* filename_or_null,   // source filename, may be
   nullptr in release mode void* user_data)
        {
            ...
        }

    ...and then setup sokol-nuklear like this:

        snk_setup(&(snk_desc_t){
            .logger = {
                .func = my_log,
                .user_data = my_user_data,
            }
        });

    The provided logging function must be reentrant (e.g. be callable from
    different threads).

    If you don't want to provide your own custom logger it is highly recommended
   to use the standard logger in sokol_log.h instead, otherwise you won't see
   any warnings or errors.


    LICENSE
    =======

    zlib/libpng license

    Copyright (c) 2020 Warren Merrifield

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
#define SOKOL_NUKLEAR_INCLUDED (1)
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_nuklear.h"
#endif
#if !defined(SOKOL_NUKLEAR_NO_SOKOL_APP) && !defined(SOKOL_APP_INCLUDED)
#error "Please include sokol_app.h before sokol_nuklear.h"
#endif
#if !defined(NK_UNDEFINED)
#error "Please include nuklear.h before sokol_nuklear.h"
#endif

#if defined(SOKOL_API_DECL) && !defined(SOKOL_NUKLEAR_API_DECL)
#define SOKOL_NUKLEAR_API_DECL SOKOL_API_DECL
#endif
#ifndef SOKOL_NUKLEAR_API_DECL
#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_NUKLEAR_IMPL)
#define SOKOL_NUKLEAR_API_DECL __declspec(dllexport)
#elif defined(_WIN32) && defined(SOKOL_DLL)
#define SOKOL_NUKLEAR_API_DECL __declspec(dllimport)
#else
#define SOKOL_NUKLEAR_API_DECL extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
  SNK_INVALID_ID = 0,
};

/*
    snk_image_t

    A combined image-sampler pair used to inject custom images and samplers into
   Nuklear

    Create with snk_make_image(), and convert to an nk_handle via
   snk_nkhandle().
*/
typedef struct snk_image_t {
  uint32_t id;
} snk_image_t;

/*
    snk_image_desc_t

    Descriptor struct for snk_make_image(). You must provide
    at least an sg_image handle. Keeping the sg_sampler handle
    zero-initialized will select the builtin default sampler
    which uses linear filtering.
*/
typedef struct snk_image_desc_t {
  sg_image image;
  sg_sampler sampler;
} snk_image_desc_t;

/*
    snk_log_item

    An enum with a unique item for each log message, warning, error
    and validation layer message.
*/
#define _SNK_LOG_ITEMS                                                         \
  _SNK_LOGITEM_XMACRO(OK, "Ok")                                                \
  _SNK_LOGITEM_XMACRO(MALLOC_FAILED, "memory allocation failed")               \
  _SNK_LOGITEM_XMACRO(IMAGE_POOL_EXHAUSTED, "image pool exhausted")

#define _SNK_LOGITEM_XMACRO(item, msg) SNK_LOGITEM_##item,
typedef enum snk_log_item_t { _SNK_LOG_ITEMS } snk_log_item_t;
#undef _SNK_LOGITEM_XMACRO

/*
    snk_allocator_t

    Used in snk_desc_t to provide custom memory-alloc and -free functions
    to sokol_nuklear.h. If memory management should be overridden, both the
    alloc_fn and free_fn function must be provided (e.g. it's not valid to
    override one function but not the other).
*/
typedef struct snk_allocator_t {
  void *(*alloc_fn)(size_t size, void *user_data);
  void (*free_fn)(void *ptr, void *user_data);
  void *user_data;
} snk_allocator_t;

/*
    snk_logger

    Used in snk_desc_t to provide a logging function. Please be aware
    that without logging function, sokol-nuklear will be completely
    silent, e.g. it will not report errors, warnings and
    validation layer messages. For maximum error verbosity,
    compile in debug mode (e.g. NDEBUG *not* defined) and install
    a logger (for instance the standard logging function from sokol_log.h).
*/
typedef struct snk_logger_t {
  void (*func)(
    const char *tag,      // always "snk"
    uint32_t log_level,   // 0=panic, 1=error, 2=warning, 3=info
    uint32_t log_item_id, // SNK_LOGITEM_*
    const char
      *message_or_null, // a message string, may be nullptr in release mode
    uint32_t line_nr,   // line number in sokol_imgui.h
    const char
      *filename_or_null, // source filename, may be nullptr in release mode
    void *user_data);
  void *user_data;
} snk_logger_t;

typedef struct snk_desc_t {
  int max_vertices;    // default: 65536
  int image_pool_size; // default: 256
  sg_pixel_format color_format;
  sg_pixel_format depth_format;
  int sample_count;
  float dpi_scale;
  struct nk_user_font *font;
  snk_allocator_t
    allocator; // optional memory allocation overrides (default: malloc/free)
  snk_logger_t logger; // optional log function override
} snk_desc_t;

SOKOL_NUKLEAR_API_DECL void snk_setup(const snk_desc_t *desc);
SOKOL_NUKLEAR_API_DECL struct nk_context *snk_new_frame(void);
SOKOL_NUKLEAR_API_DECL void snk_render(int width, int height);
SOKOL_NUKLEAR_API_DECL snk_image_t snk_make_image(const snk_image_desc_t *desc);
SOKOL_NUKLEAR_API_DECL void snk_destroy_image(snk_image_t img);
SOKOL_NUKLEAR_API_DECL snk_image_desc_t snk_query_image_desc(snk_image_t img);
SOKOL_NUKLEAR_API_DECL nk_handle snk_nkhandle(snk_image_t img);
SOKOL_NUKLEAR_API_DECL snk_image_t snk_image_from_nkhandle(nk_handle handle);
#if !defined(SOKOL_NUKLEAR_NO_SOKOL_APP)
SOKOL_NUKLEAR_API_DECL bool snk_handle_event(const sapp_event *ev);
SOKOL_NUKLEAR_API_DECL nk_flags snk_edit_string(
  struct nk_context *ctx, nk_flags flags, char *memory, int *len, int max,
  nk_plugin_filter filter);
#endif
SOKOL_NUKLEAR_API_DECL void snk_shutdown(void);

#ifdef __cplusplus
} /* extern "C" */

/* reference-based equivalents for C++ */
inline void snk_setup(const snk_desc_t &desc) { return snk_setup(&desc); }
inline snk_image_t snk_make_image(const snk_image_desc_t &desc) {
  return snk_make_image(&desc);
}

#endif
#endif /* SOKOL_NUKLEAR_INCLUDED */

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef SOKOL_NUKLEAR_IMPL

#endif // SOKOL_IMPL