#pragma once
#define SOKOL_IMPL
#define SOKOL_LINUX_CUSTOM
#include "sokol_app.h"

// Note: This Wayland implementation is based off
// [@fleischie](https://github.com/fleischie)'s [PR
// #425](https://github.com/floooh/sokol/pull/425)

#define _sapp_wl_toggle_fullscreen _sapp_linux_toggle_fullscreen
#define _sapp_wl_update_cursor _sapp_linux_update_cursor
#define _sapp_wl_lock_mouse _sapp_linux_lock_mouse
#define _sapp_wl_update_window_title _sapp_linux_update_window_title
#define _sapp_wl_set_icon _sapp_linux_set_icon
#define _sapp_wl_set_clipboard_string _sapp_linux_set_clipboard_string
#define _sapp_wl_get_clipboard_string _sapp_linux_get_clipboard_string

#define _sapp_fail(line)                                                       \
  _sapp.desc.logger.func(                                                      \
    "sapp", 1, 9999, line, __LINE__, __FILE__,                                 \
    _sapp.desc.logger.user_data); // TODO replace these
#define _sapp_info(line)                                                       \
  _sapp.desc.logger.func(                                                      \
    "sapp", 3, 9999, line, __LINE__, __FILE__, _sapp.desc.logger.user_data);

#include <EGL/egl.h>
#include <linux/input-event-codes.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

/* used for formatting eglChooseConfig() error in _sapp_wl_egl_setup() */
#include <stdio.h>
#include <string.h>

/* manually curated wayland protocol extension section: start */

/* wayland-scanner script generation
protocols = """
/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml
""".strip().splitlines()

for protocol in protocols:
    name = protocol.split('/')[-1].replace('.xml', '-protocol')
    print(f"# {name}:")
    print(f"wayland-scanner client-header \\\n  < {protocol} \\\n  > {name}.h")
    print(f"wayland-scanner private-code \\\n  < {protocol} \\\n  > {name}.c")
    print("")

# print includes
for protocol in protocols:
    name = protocol.split('/')[-1].replace('.xml', '-protocol')
    print(f'#include "{name}.h"')
    print(f'#include "{name}.c"')
*/

/* wayland scanner script
# xdg-shell-protocol:
wayland-scanner client-header \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-protocol.h
wayland-scanner private-code \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-protocol.c

# pointer-constraints-unstable-v1-protocol:
wayland-scanner client-header \
  <
/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
\ > pointer-constraints-unstable-v1-protocol.h
wayland-scanner private-code \
  <
/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
\ > pointer-constraints-unstable-v1-protocol.c

# relative-pointer-unstable-v1-protocol:
wayland-scanner client-header \
  <
/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml
\ > relative-pointer-unstable-v1-protocol.h
wayland-scanner private-code \
  <
/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml
\ > relative-pointer-unstable-v1-protocol.c
 */

#include "pointer-constraints-unstable-v1-protocol.c"
#include "pointer-constraints-unstable-v1-protocol.h"
#include "relative-pointer-unstable-v1-protocol.c"
#include "relative-pointer-unstable-v1-protocol.h"
#include "xdg-shell-protocol.c"
#include "xdg-shell-protocol.h"
/* manually curated wayland protocol extension section: end */

#define _SAPP_WAYLAND_DEFAULT_DPI_SCALE 1.0f
#define _SAPP_WAYLAND_MAX_EPOLL_EVENTS 10
#define _SAPP_WAYLAND_MAX_OUTPUTS 8

struct _sapp_wl_output {
  struct wl_output *output;

  bool active;
  int32_t factor;
};

struct _sapp_wl_touchpoint {
  bool changed;
  bool valid;
  float x;
  float y;
  int32_t id;
};

struct _sapp_wl_cursor {
  struct wl_cursor *cursor;
  struct wl_buffer *buffer;
};

typedef struct {
  /* wayland specific objects/globals */
  struct wl_compositor *compositor;
  struct wl_data_device *data_device;
  struct wl_data_device_manager *data_device_manager;
  struct wl_display *display;
  struct wl_egl_window *egl_window;
  struct wl_event_queue *event_queue;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;
  struct wl_registry *registry;
  struct wl_seat *seat;
  struct wl_shm *shm;
  struct wl_surface *surface;
  struct wl_touch *touch;
  struct xdg_surface *shell;
  struct xdg_toplevel *toplevel;
  struct xdg_wm_base *wm_base;

  /* EGL specific objects */
  EGLContext *egl_context;
  EGLDisplay *egl_display;
  EGLSurface *egl_surface;

  /* custom event loop state */
  int epoll_fd;
  int event_fd;
  struct epoll_event events[_SAPP_WAYLAND_MAX_EPOLL_EVENTS];

  /* xkb specific objects */
  struct xkb_context *xkb_context;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;

  /* repeat information */
  sapp_keycode repeat_key_code;
  uint32_t repeat_key_char;
  struct timespec repeat_delay;
  struct timespec repeat_rate;
  struct timespec repeat_next;

  /* pointer/cursor related data */
  struct _sapp_wl_cursor cursors[_SAPP_MOUSECURSOR_NUM];
  struct wl_surface *cursor_surface;
  struct zwp_locked_pointer_v1 *locked_pointer;
  struct zwp_pointer_constraints_v1 *pointer_constraints;
  struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
  struct zwp_relative_pointer_v1 *relative_pointer;
  uint32_t serial;

  /* accumulated touch state */
  struct _sapp_wl_touchpoint touchpoints[SAPP_MAX_TOUCHPOINTS];

  /* output data for scaling/rotating/etc. */
  unsigned char max_outputs;
  struct _sapp_wl_output outputs[_SAPP_WAYLAND_MAX_OUTPUTS];

  /* dnd data */
  struct wl_data_offer *data_offer;

  /* surface reference for focus/unfocused event */
  struct wl_surface *focus;
} _sapp_wl_t;
static _sapp_wl_t _sapp_wl;

/* based loosely on <sys/time.h> `timercmp` macro
 * compare two timespec structs and return 1 if a > b, -1 if a < b, 0 if a == b
 */
_SOKOL_PRIVATE int
_sapp_wl_timespec_cmp(struct timespec *a, struct timespec *b) {
  if (a->tv_sec > b->tv_sec)
    return 1;
  if (a->tv_sec < b->tv_sec)
    return -1;

  if (a->tv_nsec > b->tv_nsec)
    return 1;
  if (a->tv_nsec < b->tv_nsec)
    return -1;

  return 0;
}

/* based on <sys/time.h> `timeradd` macro adapted for timespec structs
 * calculate sum of two timespec structs
 */
_SOKOL_PRIVATE void _sapp_wl_timespec_add(
  struct timespec *a, struct timespec *b, struct timespec *result) {
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_nsec = a->tv_nsec + b->tv_nsec;
  if (result->tv_nsec >= 1000000000L) {
    ++result->tv_sec;
    result->tv_nsec -= 1000000000L;
  }
}

_SOKOL_PRIVATE sapp_keycode _sapp_wl_translate_key(xkb_keysym_t sym) {
  switch (sym) {
  case XKB_KEY_KP_Space:
    return SAPP_KEYCODE_SPACE;
  case XKB_KEY_apostrophe:
    return SAPP_KEYCODE_APOSTROPHE;
  case XKB_KEY_comma:
    return SAPP_KEYCODE_COMMA;
  case XKB_KEY_minus:
    return SAPP_KEYCODE_MINUS;
  case XKB_KEY_period:
    return SAPP_KEYCODE_PERIOD;
  case XKB_KEY_slash:
    return SAPP_KEYCODE_SLASH;
  case XKB_KEY_0:
    return SAPP_KEYCODE_0;
  case XKB_KEY_1:
    return SAPP_KEYCODE_1;
  case XKB_KEY_2:
    return SAPP_KEYCODE_2;
  case XKB_KEY_3:
    return SAPP_KEYCODE_3;
  case XKB_KEY_4:
    return SAPP_KEYCODE_4;
  case XKB_KEY_5:
    return SAPP_KEYCODE_5;
  case XKB_KEY_6:
    return SAPP_KEYCODE_6;
  case XKB_KEY_7:
    return SAPP_KEYCODE_7;
  case XKB_KEY_8:
    return SAPP_KEYCODE_8;
  case XKB_KEY_9:
    return SAPP_KEYCODE_9;
  case XKB_KEY_semicolon:
    return SAPP_KEYCODE_SEMICOLON;
  case XKB_KEY_equal:
    return SAPP_KEYCODE_EQUAL;
  case XKB_KEY_A:
  case XKB_KEY_a:
    return SAPP_KEYCODE_A;
  case XKB_KEY_B:
  case XKB_KEY_b:
    return SAPP_KEYCODE_B;
  case XKB_KEY_C:
  case XKB_KEY_c:
    return SAPP_KEYCODE_C;
  case XKB_KEY_D:
  case XKB_KEY_d:
    return SAPP_KEYCODE_D;
  case XKB_KEY_E:
  case XKB_KEY_e:
    return SAPP_KEYCODE_E;
  case XKB_KEY_F:
  case XKB_KEY_f:
    return SAPP_KEYCODE_F;
  case XKB_KEY_G:
  case XKB_KEY_g:
    return SAPP_KEYCODE_G;
  case XKB_KEY_H:
  case XKB_KEY_h:
    return SAPP_KEYCODE_H;
  case XKB_KEY_I:
  case XKB_KEY_i:
    return SAPP_KEYCODE_I;
  case XKB_KEY_J:
  case XKB_KEY_j:
    return SAPP_KEYCODE_J;
  case XKB_KEY_K:
  case XKB_KEY_k:
    return SAPP_KEYCODE_K;
  case XKB_KEY_L:
  case XKB_KEY_l:
    return SAPP_KEYCODE_L;
  case XKB_KEY_M:
  case XKB_KEY_m:
    return SAPP_KEYCODE_M;
  case XKB_KEY_N:
  case XKB_KEY_n:
    return SAPP_KEYCODE_N;
  case XKB_KEY_O:
  case XKB_KEY_o:
    return SAPP_KEYCODE_O;
  case XKB_KEY_P:
  case XKB_KEY_p:
    return SAPP_KEYCODE_P;
  case XKB_KEY_Q:
  case XKB_KEY_q:
    return SAPP_KEYCODE_Q;
  case XKB_KEY_R:
  case XKB_KEY_r:
    return SAPP_KEYCODE_R;
  case XKB_KEY_S:
  case XKB_KEY_s:
    return SAPP_KEYCODE_S;
  case XKB_KEY_T:
  case XKB_KEY_t:
    return SAPP_KEYCODE_T;
  case XKB_KEY_U:
  case XKB_KEY_u:
    return SAPP_KEYCODE_U;
  case XKB_KEY_V:
  case XKB_KEY_v:
    return SAPP_KEYCODE_V;
  case XKB_KEY_W:
  case XKB_KEY_w:
    return SAPP_KEYCODE_W;
  case XKB_KEY_X:
  case XKB_KEY_x:
    return SAPP_KEYCODE_X;
  case XKB_KEY_Y:
  case XKB_KEY_y:
    return SAPP_KEYCODE_Y;
  case XKB_KEY_Z:
  case XKB_KEY_z:
    return SAPP_KEYCODE_Z;
  case XKB_KEY_bracketleft:
    return SAPP_KEYCODE_LEFT_BRACKET;
  case XKB_KEY_backslash:
    return SAPP_KEYCODE_BACKSLASH;
  case XKB_KEY_bracketright:
    return SAPP_KEYCODE_RIGHT_BRACKET;
  case XKB_KEY_grave:
    return SAPP_KEYCODE_GRAVE_ACCENT;
  case XKB_KEY_Escape:
    return SAPP_KEYCODE_ESCAPE;
  case XKB_KEY_Return:
    return SAPP_KEYCODE_ENTER;
  case XKB_KEY_Tab:
    return SAPP_KEYCODE_TAB;
  case XKB_KEY_BackSpace:
    return SAPP_KEYCODE_BACKSPACE;
  case XKB_KEY_Insert:
    return SAPP_KEYCODE_INSERT;
  case XKB_KEY_Delete:
    return SAPP_KEYCODE_DELETE;
  case XKB_KEY_Right:
    return SAPP_KEYCODE_RIGHT;
  case XKB_KEY_Left:
    return SAPP_KEYCODE_LEFT;
  case XKB_KEY_Down:
    return SAPP_KEYCODE_DOWN;
  case XKB_KEY_Up:
    return SAPP_KEYCODE_UP;
  case XKB_KEY_Page_Up:
    return SAPP_KEYCODE_PAGE_UP;
  case XKB_KEY_Page_Down:
    return SAPP_KEYCODE_PAGE_DOWN;
  case XKB_KEY_Home:
    return SAPP_KEYCODE_HOME;
  case XKB_KEY_End:
    return SAPP_KEYCODE_END;
  case XKB_KEY_Caps_Lock:
    return SAPP_KEYCODE_CAPS_LOCK;
  case XKB_KEY_Scroll_Lock:
    return SAPP_KEYCODE_SCROLL_LOCK;
  case XKB_KEY_Num_Lock:
    return SAPP_KEYCODE_NUM_LOCK;
  case XKB_KEY_Print:
    return SAPP_KEYCODE_PRINT_SCREEN;
  case XKB_KEY_Pause:
    return SAPP_KEYCODE_PAUSE;
  case XKB_KEY_F1:
    return SAPP_KEYCODE_F1;
  case XKB_KEY_F2:
    return SAPP_KEYCODE_F2;
  case XKB_KEY_F3:
    return SAPP_KEYCODE_F3;
  case XKB_KEY_F4:
    return SAPP_KEYCODE_F4;
  case XKB_KEY_F5:
    return SAPP_KEYCODE_F5;
  case XKB_KEY_F6:
    return SAPP_KEYCODE_F6;
  case XKB_KEY_F7:
    return SAPP_KEYCODE_F7;
  case XKB_KEY_F8:
    return SAPP_KEYCODE_F8;
  case XKB_KEY_F9:
    return SAPP_KEYCODE_F9;
  case XKB_KEY_F10:
    return SAPP_KEYCODE_F10;
  case XKB_KEY_F11:
    return SAPP_KEYCODE_F11;
  case XKB_KEY_F12:
    return SAPP_KEYCODE_F12;
  case XKB_KEY_F13:
    return SAPP_KEYCODE_F13;
  case XKB_KEY_F14:
    return SAPP_KEYCODE_F14;
  case XKB_KEY_F15:
    return SAPP_KEYCODE_F15;
  case XKB_KEY_F16:
    return SAPP_KEYCODE_F16;
  case XKB_KEY_F17:
    return SAPP_KEYCODE_F17;
  case XKB_KEY_F18:
    return SAPP_KEYCODE_F18;
  case XKB_KEY_F19:
    return SAPP_KEYCODE_F19;
  case XKB_KEY_F20:
    return SAPP_KEYCODE_F20;
  case XKB_KEY_F21:
    return SAPP_KEYCODE_F21;
  case XKB_KEY_F22:
    return SAPP_KEYCODE_F22;
  case XKB_KEY_F23:
    return SAPP_KEYCODE_F23;
  case XKB_KEY_F24:
    return SAPP_KEYCODE_F24;
  case XKB_KEY_F25:
    return SAPP_KEYCODE_F25;
  case XKB_KEY_KP_0:
    return SAPP_KEYCODE_KP_0;
  case XKB_KEY_KP_1:
    return SAPP_KEYCODE_KP_1;
  case XKB_KEY_KP_2:
    return SAPP_KEYCODE_KP_2;
  case XKB_KEY_KP_3:
    return SAPP_KEYCODE_KP_3;
  case XKB_KEY_KP_4:
    return SAPP_KEYCODE_KP_4;
  case XKB_KEY_KP_5:
    return SAPP_KEYCODE_KP_5;
  case XKB_KEY_KP_6:
    return SAPP_KEYCODE_KP_6;
  case XKB_KEY_KP_7:
    return SAPP_KEYCODE_KP_7;
  case XKB_KEY_KP_8:
    return SAPP_KEYCODE_KP_8;
  case XKB_KEY_KP_9:
    return SAPP_KEYCODE_KP_9;
  case XKB_KEY_KP_Decimal:
    return SAPP_KEYCODE_KP_DECIMAL;
  case XKB_KEY_KP_Divide:
    return SAPP_KEYCODE_KP_DIVIDE;
  case XKB_KEY_KP_Multiply:
    return SAPP_KEYCODE_KP_MULTIPLY;
  case XKB_KEY_KP_Subtract:
    return SAPP_KEYCODE_KP_SUBTRACT;
  case XKB_KEY_KP_Add:
    return SAPP_KEYCODE_KP_ADD;
  case XKB_KEY_KP_Enter:
    return SAPP_KEYCODE_KP_ENTER;
  case XKB_KEY_KP_Equal:
    return SAPP_KEYCODE_KP_EQUAL;
  case XKB_KEY_Shift_L:
    return SAPP_KEYCODE_LEFT_SHIFT;
  case XKB_KEY_Control_L:
    return SAPP_KEYCODE_LEFT_CONTROL;
  case XKB_KEY_Meta_L:
  case XKB_KEY_Alt_L:
    return SAPP_KEYCODE_LEFT_ALT;
  case XKB_KEY_Super_L:
    return SAPP_KEYCODE_LEFT_SUPER;
  case XKB_KEY_Shift_R:
    return SAPP_KEYCODE_RIGHT_SHIFT;
  case XKB_KEY_Control_R:
    return SAPP_KEYCODE_RIGHT_CONTROL;
  case XKB_KEY_Meta_R:
  case XKB_KEY_Alt_R:
    return SAPP_KEYCODE_RIGHT_ALT;
  case XKB_KEY_Super_R:
    return SAPP_KEYCODE_RIGHT_SUPER;
  case XKB_KEY_Menu:
    return SAPP_KEYCODE_MENU;
  case SAPP_KEYCODE_WORLD_1:
  case SAPP_KEYCODE_WORLD_2:
  default:
    return SAPP_KEYCODE_INVALID;
  }
}

_SOKOL_PRIVATE uint32_t _sapp_wl_get_modifiers(void) {
  uint32_t modifiers = 0;

  enum xkb_state_component active_mask = (enum xkb_state_component)(
    XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED | XKB_STATE_MODS_LOCKED);

  if (
    0 < xkb_state_mod_name_is_active(
          _sapp_wl.xkb_state, XKB_MOD_NAME_SHIFT, active_mask)) {
    modifiers |= SAPP_MODIFIER_SHIFT;
  }
  if (
    0 < xkb_state_mod_name_is_active(
          _sapp_wl.xkb_state, XKB_MOD_NAME_CTRL, active_mask)) {
    modifiers |= SAPP_MODIFIER_CTRL;
  }
  if (
    0 < xkb_state_mod_name_is_active(
          _sapp_wl.xkb_state, XKB_MOD_NAME_ALT, active_mask)) {
    modifiers |= SAPP_MODIFIER_ALT;
  }
  if (
    0 < xkb_state_mod_name_is_active(
          _sapp_wl.xkb_state, XKB_MOD_NAME_LOGO, active_mask)) {
    modifiers |= SAPP_MODIFIER_SUPER;
  }

  return modifiers;
}

_SOKOL_PRIVATE struct _sapp_wl_touchpoint *_sapp_wl_get_touchpoint(int32_t id) {
  int point_id = -1;
  for (int i = 0; i < SAPP_MAX_TOUCHPOINTS; i++) {
    if (id == _sapp_wl.touchpoints[i].id) {
      _sapp_wl.touchpoints[i].changed = true;
      _sapp_wl.touchpoints[i].valid = true;
      return &_sapp_wl.touchpoints[i];
    }
    if (-1 == point_id && !_sapp_wl.touchpoints[i].valid) {
      point_id = i;
    }
  }
  if (-1 == point_id) {
    return NULL;
  }

  _sapp_wl.touchpoints[point_id].valid = true;
  _sapp_wl.touchpoints[point_id].id = id;
  return &_sapp_wl.touchpoints[point_id];
}

_SOKOL_PRIVATE void _sapp_wl_cleanup(void) {
  if (NULL != _sapp_wl.egl_surface)
    eglDestroySurface(_sapp_wl.egl_display, _sapp_wl.egl_surface);
  if (NULL != _sapp_wl.egl_window)
    wl_egl_window_destroy(_sapp_wl.egl_window);
  if (NULL != _sapp_wl.egl_context)
    eglDestroyContext(_sapp_wl.egl_display, _sapp_wl.egl_context);

  for (unsigned int i = 0; i < _sapp_wl.max_outputs; i++) {
    if (NULL != _sapp_wl.outputs[i].output)
      wl_output_destroy(_sapp_wl.outputs[i].output);
  }
  _sapp_wl.max_outputs = 0;

  if (NULL != _sapp_wl.toplevel)
    xdg_toplevel_destroy(_sapp_wl.toplevel);
  if (NULL != _sapp_wl.shell)
    xdg_surface_destroy(_sapp_wl.shell);
  if (NULL != _sapp_wl.wm_base)
    xdg_wm_base_destroy(_sapp_wl.wm_base);
  if (NULL != _sapp_wl.surface)
    wl_surface_destroy(_sapp_wl.surface);
  if (NULL != _sapp_wl.compositor)
    wl_compositor_destroy(_sapp_wl.compositor);
  if (NULL != _sapp_wl.registry)
    wl_registry_destroy(_sapp_wl.registry);
  if (NULL != _sapp_wl.data_device)
    wl_data_device_release(_sapp_wl.data_device);
  if (NULL != _sapp_wl.data_device_manager)
    wl_data_device_manager_destroy(_sapp_wl.data_device_manager);
  if (NULL != _sapp_wl.touch)
    wl_touch_destroy(_sapp_wl.touch);
  if (NULL != _sapp_wl.locked_pointer)
    zwp_locked_pointer_v1_destroy(_sapp_wl.locked_pointer);
  if (NULL != _sapp_wl.pointer_constraints)
    zwp_pointer_constraints_v1_destroy(_sapp_wl.pointer_constraints);
  if (NULL != _sapp_wl.relative_pointer)
    zwp_relative_pointer_v1_destroy(_sapp_wl.relative_pointer);
  if (NULL != _sapp_wl.relative_pointer_manager)
    zwp_relative_pointer_manager_v1_destroy(_sapp_wl.relative_pointer_manager);
  if (NULL != _sapp_wl.pointer)
    wl_pointer_destroy(_sapp_wl.pointer);
  if (NULL != _sapp_wl.cursor_surface)
    wl_surface_destroy(_sapp_wl.cursor_surface);
  if (NULL != _sapp_wl.keyboard)
    wl_keyboard_destroy(_sapp_wl.keyboard);
  if (NULL != _sapp_wl.seat)
    wl_seat_destroy(_sapp_wl.seat);
  if (NULL != _sapp_wl.shm)
    wl_shm_destroy(_sapp_wl.shm);

  if (NULL != _sapp_wl.xkb_keymap)
    xkb_keymap_unref(_sapp_wl.xkb_keymap);
  if (NULL != _sapp_wl.xkb_state)
    xkb_state_unref(_sapp_wl.xkb_state);
  if (NULL != _sapp_wl.xkb_context)
    xkb_context_unref(_sapp_wl.xkb_context);

  for (int i = 0; i < _SAPP_MOUSECURSOR_NUM; i++) {
    struct wl_buffer *buffer = _sapp_wl.cursors[i].buffer;
    if (NULL != buffer)
      wl_buffer_destroy(buffer);
  }

  if (NULL != _sapp_wl.data_offer) {
    wl_data_offer_destroy(_sapp_wl.data_offer);
    // TODO this doesn't seem to be the wl_data_offer that is still attached
    // when destroying the queue it complains that e.g.
    //     warning: queue 0x62f6d5be5b40 destroyed while proxies still attached:
    //       wl_data_offer@4278190080 still attached
    //       wl_shm_pool@26 still attached
  }
  // TODO where is the wl_shm_pool to be destroyed?

  if (NULL != _sapp_wl.event_queue)
    wl_event_queue_destroy(_sapp_wl.event_queue);

  epoll_ctl(_sapp_wl.epoll_fd, EPOLL_CTL_DEL, _sapp_wl.event_fd, NULL);
  close(_sapp_wl.event_fd);
  close(_sapp_wl.epoll_fd);
}

_SOKOL_PRIVATE void
_sapp_wl_update_cursor(sapp_mouse_cursor cursor, bool shown) {
  SOKOL_ASSERT((cursor >= 0) && (cursor < _SAPP_MOUSECURSOR_NUM));
  uint32_t serial = _sapp_wl.serial;

  struct wl_cursor *selected_cursor = _sapp_wl.cursors[cursor].cursor;
  if (NULL == selected_cursor) {
    // don't show error messages or this will spam
    return;
  }

  if (shown) {
    wl_surface_attach(
      _sapp_wl.cursor_surface, _sapp_wl.cursors[cursor].buffer, 0, 0);
    wl_surface_damage_buffer(
      _sapp_wl.cursor_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(_sapp_wl.cursor_surface);

    if (NULL != _sapp_wl.cursor_surface) {
      wl_pointer_set_cursor(
        _sapp_wl.pointer, serial, _sapp_wl.cursor_surface,
        (int32_t)selected_cursor->images[0]->hotspot_x,
        (int32_t)selected_cursor->images[0]->hotspot_y);
    }
  } else {
    wl_pointer_set_cursor(_sapp_wl.pointer, serial, NULL, 0, 0);
  }
}

_SOKOL_PRIVATE void _sapp_wl_locked_pointer_locked(
  void *data, struct zwp_locked_pointer_v1 *locked_pointer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(locked_pointer);
}

_SOKOL_PRIVATE void _sapp_wl_locked_pointer_unlocked(
  void *data, struct zwp_locked_pointer_v1 *locked_pointer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(locked_pointer);
  if (NULL != _sapp_wl.locked_pointer) {
    zwp_locked_pointer_v1_destroy(_sapp_wl.locked_pointer);
    _sapp_wl.locked_pointer = NULL;
  }
}

_SOKOL_PRIVATE const struct zwp_locked_pointer_v1_listener
  _sapp_wl_locked_pointer_listener = {
    .locked = _sapp_wl_locked_pointer_locked,
    .unlocked = _sapp_wl_locked_pointer_unlocked,
};

_SOKOL_PRIVATE void _sapp_wl_lock_mouse(bool lock) {
  if (lock == _sapp.mouse.locked) {
    return;
  }
  _sapp.mouse.dx = 0.0f;
  _sapp.mouse.dy = 0.0f;
  _sapp.mouse.locked = lock;
  if (NULL != _sapp_wl.pointer && NULL != _sapp_wl.pointer_constraints) {
    if (_sapp.mouse.locked) {
      _sapp_wl.locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
        _sapp_wl.pointer_constraints, _sapp_wl.surface, _sapp_wl.pointer, NULL,
        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
      zwp_locked_pointer_v1_add_listener(
        _sapp_wl.locked_pointer, &_sapp_wl_locked_pointer_listener, NULL);
    } else {
      _sapp_wl_locked_pointer_unlocked(NULL, NULL);
    }
  }
}

_SOKOL_PRIVATE void _sapp_wl_update_window_title(void) {
  xdg_toplevel_set_title(_sapp_wl.toplevel, _sapp.window_title);
}

_SOKOL_PRIVATE void
_sapp_linux_set_icon(const sapp_icon_desc *icon_desc, int num_images) {
  ;
}

_SOKOL_PRIVATE void _sapp_wl_set_fullscreen(bool is_fullscreen) {
  if (NULL == _sapp_wl.toplevel) {
    return;
  }

  if (is_fullscreen) {
    xdg_toplevel_set_fullscreen(_sapp_wl.toplevel, NULL);
  } else {
    xdg_toplevel_unset_fullscreen(_sapp_wl.toplevel);
  }
}

_SOKOL_PRIVATE void _sapp_wl_app_event(sapp_event_type type) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(type);
    _sapp_call_event(&_sapp.event);
  }
}

_SOKOL_PRIVATE void _sapp_wl_resize_window(int width, int height) {
  if (_sapp.first_frame || 0 == height || 0 == width) {
    return;
  }

  int32_t max_dpi_scale = 1;
  if (_sapp.desc.high_dpi) {
    /* scale to highest dpi factor of active outputs */
    for (unsigned int i = 0; i < _sapp_wl.max_outputs; i++) {
      if (
        _sapp_wl.outputs[i].active &&
        _sapp_wl.outputs[i].factor > max_dpi_scale) {
        max_dpi_scale = _sapp_wl.outputs[i].factor;
      }
    }
  }
  if (
    height != _sapp.window_height || width != _sapp.window_width ||
    (int32_t)_sapp.dpi_scale != max_dpi_scale) {
    _sapp.window_height = height;
    _sapp.window_width = width;
    _sapp.dpi_scale = (float)max_dpi_scale;
    _sapp.framebuffer_height = _sapp.dpi_scale * _sapp.window_height;
    _sapp.framebuffer_width = _sapp.dpi_scale * _sapp.window_width;
    wl_surface_set_buffer_scale(_sapp_wl.surface, max_dpi_scale);
    if (NULL != _sapp_wl.egl_window) {
      wl_egl_window_resize(
        _sapp_wl.egl_window, _sapp.window_width, _sapp.window_height, 0, 0);
    }
    _sapp_wl_app_event(SAPP_EVENTTYPE_RESIZED);
  }
}

_SOKOL_PRIVATE void _sapp_wl_key_event(
  sapp_event_type type, sapp_keycode key, bool is_repeat, uint32_t modifiers) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(type);
    _sapp.event.key_code = key;
    _sapp.event.key_repeat = is_repeat;
    _sapp.event.modifiers = modifiers;
    _sapp_call_event(&_sapp.event);

    /* check if a CLIPBOARD_PASTED event must be sent too */
    if (
      _sapp.clipboard.enabled && (type == SAPP_EVENTTYPE_KEY_DOWN) &&
      (_sapp.event.modifiers == SAPP_MODIFIER_CTRL) &&
      (_sapp.event.key_code == SAPP_KEYCODE_V)) {
      _sapp_init_event(SAPP_EVENTTYPE_CLIPBOARD_PASTED);
      _sapp_call_event(&_sapp.event);
    }
  }
}

_SOKOL_PRIVATE void
_sapp_wl_char_event(uint32_t utf32_char, bool is_repeat, uint32_t modifiers) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(SAPP_EVENTTYPE_CHAR);
    _sapp.event.char_code = utf32_char;
    _sapp.event.key_repeat = is_repeat;
    _sapp.event.modifiers = modifiers;
    _sapp_call_event(&_sapp.event);
  }
}

_SOKOL_PRIVATE void _sapp_wl_mouse_event(
  sapp_event_type type, sapp_mousebutton btn, uint32_t modifiers) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(type);
    if (SAPP_MOUSEBUTTON_INVALID != btn) {
      _sapp.event.mouse_button = btn;
    }
    _sapp.event.modifiers = modifiers;
    _sapp_call_event(&_sapp.event);
  }
}

_SOKOL_PRIVATE void
_sapp_wl_scroll_event(bool is_vertical_axis, double value, uint32_t modifiers) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(SAPP_EVENTTYPE_MOUSE_SCROLL);

    // value is in surface-local coordinates (divide by dpi to get pixel coords)
    // but these aren't sensible values to pass to our scroll event handlers
    float scroll = (float)value;
    // Need to invert this, or else everything is backwards
    scroll = -scroll;

    if (is_vertical_axis) {
      // My mouse: value is +/-15 per click scrolling 75 lines
      // (per ImGui docs for MouseWheel: 1 unit = 5 lines of text)
      // For 3 lines per click, multiply by 3/75 = 1/25
      scroll /= 25.0f;
      _sapp.event.scroll_y = scroll;
    } else {
      // My mouse: value is +/-7 per click scrolling 27 columns
      // For ~5 columns per click, multiply by 5/27 ~= 1/5
      scroll /= 5.0f;
      _sapp.event.scroll_x = scroll;
    }
    _sapp.event.modifiers = modifiers;
    _sapp_call_event(&_sapp.event);
  }
}

_SOKOL_PRIVATE void
_sapp_wl_touch_event(sapp_event_type type, int32_t id, uint32_t modifiers) {
  if (_sapp_events_enabled()) {
    _sapp_init_event(type);
    int max_touchpoints = 0;
    for (int i = 0; i < SAPP_MAX_TOUCHPOINTS; i++) {
      if (_sapp_wl.touchpoints[i].valid) {
        sapp_touchpoint *point = &_sapp.event.touches[max_touchpoints++];
        point->identifier = (uintptr_t)_sapp_wl.touchpoints[i].id;
        point->pos_x = _sapp.dpi_scale * _sapp_wl.touchpoints[i].x;
        point->pos_y = _sapp.dpi_scale * _sapp_wl.touchpoints[i].y;
        point->changed = _sapp_wl.touchpoints[i].changed;

        if (
          id == _sapp_wl.touchpoints[i].id &&
          SAPP_EVENTTYPE_TOUCHES_ENDED == type) {
          _sapp_wl.touchpoints[i].valid = false;
        }
      }
      if (SAPP_EVENTTYPE_TOUCHES_CANCELLED == type) {
        _sapp_wl.touchpoints[i].valid = false;
      }
    }
    if (max_touchpoints > SAPP_MAX_TOUCHPOINTS) {
      max_touchpoints = SAPP_MAX_TOUCHPOINTS;
    }
    _sapp.event.num_touches = max_touchpoints;
    _sapp.event.modifiers = modifiers;
    _sapp_call_event(&_sapp.event);
  }
}

_SOKOL_PRIVATE void _sapp_wl_toggle_fullscreen() {
  _sapp.fullscreen = !_sapp.fullscreen;
  _sapp_wl_set_fullscreen(_sapp.fullscreen);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_action(
  void *data, struct wl_data_source *data_source, uint32_t dnd_action) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_source);
  _SOKOL_UNUSED(dnd_action);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_cancelled(
  void *data, struct wl_data_source *data_source) {
  _SOKOL_UNUSED(data);
  wl_data_source_destroy(data_source);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_dnd_drop_performed(
  void *data, struct wl_data_source *data_source) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_source);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_dnd_finished(
  void *data, struct wl_data_source *data_source) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_source);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_send(
  void *data, struct wl_data_source *data_source, const char *mime_type,
  int32_t fd) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_source);

  if (0 == strcmp(mime_type, "text/plain")) {
    if (write(fd, _sapp.clipboard.buffer, strlen(_sapp.clipboard.buffer)) < 0) {
      _sapp_fail("clipboard send failed");
    }
  } else {
    _sapp_fail("clipboard mime type fail");
  }

  close(fd);
}

_SOKOL_PRIVATE void _sapp_wl_data_source_handle_target(
  void *data, struct wl_data_source *data_source, const char *mime_type) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_source);
  _SOKOL_UNUSED(mime_type);
}

_SOKOL_PRIVATE const struct wl_data_source_listener
  _sapp_wl_data_source_listener = {
    .target = _sapp_wl_data_source_handle_target,
    .send = _sapp_wl_data_source_handle_send,
    .cancelled = _sapp_wl_data_source_handle_cancelled,
    .dnd_drop_performed = _sapp_wl_data_source_handle_dnd_drop_performed,
    .dnd_finished = _sapp_wl_data_source_handle_dnd_finished,
    .action = _sapp_wl_data_source_handle_action,
};

_SOKOL_PRIVATE void _sapp_wl_set_clipboard_string(const char *str) {
  _SOKOL_UNUSED(str);

  if (NULL == _sapp_wl.data_device_manager || NULL == _sapp_wl.data_device) {
    return;
  }

  struct wl_data_source *source =
    wl_data_device_manager_create_data_source(_sapp_wl.data_device_manager);
  wl_data_source_add_listener(source, &_sapp_wl_data_source_listener, NULL);
  wl_data_source_offer(source, "text/plain");

  wl_data_device_set_selection(_sapp_wl.data_device, source, _sapp_wl.serial);
}

_SOKOL_PRIVATE const char *_sapp_wl_get_clipboard_string() {
  return _sapp.clipboard.buffer;
}

_SOKOL_PRIVATE void _sapp_wl_wm_base_ping(
  void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
  _SOKOL_UNUSED(data);
  _sapp_wl.serial = serial;
  xdg_wm_base_pong(wm_base, serial);
}

_SOKOL_PRIVATE const struct xdg_wm_base_listener _sapp_wl_wm_base_listener = {
  .ping = _sapp_wl_wm_base_ping,
};

_SOKOL_PRIVATE void _sapp_wl_keyboard_key(
  void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
  uint32_t key, uint32_t key_state) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(keyboard);
  _SOKOL_UNUSED(time);

  _sapp_wl.serial = serial;

  if (_sapp_events_enabled()) {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(_sapp_wl.xkb_state, key + 8);
    sapp_keycode code = _sapp_wl_translate_key(sym);
    uint32_t modifiers = _sapp_wl_get_modifiers();

    if (WL_KEYBOARD_KEY_STATE_PRESSED == key_state) {
      _sapp_wl_key_event(SAPP_EVENTTYPE_KEY_DOWN, code, false, modifiers);

      const uint32_t utf32_char = xkb_keysym_to_utf32(sym);
      if (utf32_char) {
        _sapp_wl_char_event(utf32_char, false, modifiers);
      }

      /* only repeat non-modifier xkb keys */
      if (sym < XKB_KEY_Shift_L || sym > XKB_KEY_Hyper_R) {
        clock_gettime(CLOCK_MONOTONIC, &_sapp_wl.repeat_next);
        _sapp_wl_timespec_add(
          &_sapp_wl.repeat_next, &_sapp_wl.repeat_delay, &_sapp_wl.repeat_next);

        _sapp_wl.repeat_key_char = utf32_char;
        _sapp_wl.repeat_key_code = code;
      }
    } else if (WL_KEYBOARD_KEY_STATE_RELEASED == key_state) {
      _sapp_wl_key_event(SAPP_EVENTTYPE_KEY_UP, code, false, 0);
      // Don't send modifiers for key release or else this happens:
      // e.g. Press Ctrl and release:
      //  - ImGui inputs will show there is a Key down for "ModCtrl" 662 with a
      //  CTRL modifier,
      //  - this stays on until another input occurs.

      if (_sapp_wl.repeat_key_code == code) {
        _sapp_wl.repeat_key_char = 0;
        _sapp_wl.repeat_key_code = SAPP_KEYCODE_INVALID;
      }
    }
  }
}

_SOKOL_PRIVATE void _sapp_wl_keyboard_enter(
  void *data, struct wl_keyboard *keyboard, uint32_t serial,
  struct wl_surface *surface, struct wl_array *keys) {
  _SOKOL_UNUSED(surface);

  _sapp_wl.serial = serial;

  /* cast to custom array-struct to silence C++-compilation errors,
   * that complain about `void* data` being cast to `int* toplevel_state` */
  struct states_array {
    size_t size;
    size_t alloc;
    uint32_t *data;
  } *arr = (struct states_array *)keys;

  uint32_t *key;
  wl_array_for_each(key, arr) {
    _sapp_wl_keyboard_key(
      data, keyboard, serial, 0, *key, WL_KEYBOARD_KEY_STATE_PRESSED);
  }

  if (NULL == _sapp_wl.focus) {
    _sapp_wl_app_event(SAPP_EVENTTYPE_FOCUSED);
  }
  _sapp_wl.focus = surface;
}

_SOKOL_PRIVATE void _sapp_wl_keyboard_keymap(
  void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd,
  uint32_t size) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(keyboard);

  char *keymap_shm = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (MAP_FAILED == keymap_shm) {
    _sapp_fail("wayland keymap fail");
  }

  switch (format) {
  case WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP:
    _sapp_fail("wayland keyboard no keymap");
    break;
  case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1:
    xkb_keymap_unref(_sapp_wl.xkb_keymap);
    xkb_state_unref(_sapp_wl.xkb_state);
    _sapp_wl.xkb_keymap = xkb_keymap_new_from_string(
      _sapp_wl.xkb_context, keymap_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
    _sapp_wl.xkb_state = xkb_state_new(_sapp_wl.xkb_keymap);
    break;
  default:
    break;
  }

  munmap(keymap_shm, size);
  close(fd);
}

_SOKOL_PRIVATE void _sapp_wl_keyboard_leave(
  void *data, struct wl_keyboard *keyboard, uint32_t serial,
  struct wl_surface *surface) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(keyboard);
  _SOKOL_UNUSED(surface);

  _sapp_wl.serial = serial;

  if (NULL != _sapp_wl.focus) {
    _sapp_wl_app_event(SAPP_EVENTTYPE_UNFOCUSED);
    _sapp_wl.repeat_key_char = 0;
    _sapp_wl.repeat_key_code = SAPP_KEYCODE_INVALID;
  }
  _sapp_wl.focus = NULL;
}

_SOKOL_PRIVATE void _sapp_wl_keyboard_modifiers(
  void *data, struct wl_keyboard *keyboard, uint32_t serial,
  uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
  uint32_t group) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(keyboard);

  _sapp_wl.serial = serial;

  xkb_state_update_mask(
    _sapp_wl.xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

_SOKOL_PRIVATE void _sapp_wl_keyboard_repeat_info(
  void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(keyboard);

  /* delay is milliseconds before repeat should start,
   * rate is number of times per second (i.e. per 1_000_000_000L nanoseconds) */
  _sapp_wl.repeat_delay =
    (struct timespec){.tv_sec = 0, .tv_nsec = 1000000L * delay};
  _sapp_wl.repeat_rate =
    (struct timespec){.tv_sec = 0, .tv_nsec = 1000000000L / rate};
}

_SOKOL_PRIVATE const struct wl_keyboard_listener _sapp_wl_keyboard_listener = {
  .keymap = _sapp_wl_keyboard_keymap,
  .enter = _sapp_wl_keyboard_enter,
  .leave = _sapp_wl_keyboard_leave,
  .key = _sapp_wl_keyboard_key,
  .modifiers = _sapp_wl_keyboard_modifiers,
  .repeat_info = _sapp_wl_keyboard_repeat_info,
};

_SOKOL_PRIVATE void _sapp_wl_pointer_axis(
  void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis,
  wl_fixed_t value) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(time);

  _sapp_wl_scroll_event(
    WL_POINTER_AXIS_VERTICAL_SCROLL == axis, wl_fixed_to_double(value),
    _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_pointer_axis_discrete(
  void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(axis);
  _SOKOL_UNUSED(discrete);
}

_SOKOL_PRIVATE void _sapp_wl_pointer_axis_source(
  void *data, struct wl_pointer *pointer, uint32_t axis_source) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(axis_source);
}

_SOKOL_PRIVATE void _sapp_wl_pointer_axis_stop(
  void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(time);
  _SOKOL_UNUSED(axis);
}

_SOKOL_PRIVATE void _sapp_wl_pointer_button(
  void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time,
  uint32_t button, uint32_t button_state) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(time);

  _sapp_wl.serial = serial;

  sapp_event_type type = SAPP_EVENTTYPE_INVALID;
  switch (button_state) {
  case WL_POINTER_BUTTON_STATE_PRESSED:
    type = SAPP_EVENTTYPE_MOUSE_DOWN;
    break;
  case WL_POINTER_BUTTON_STATE_RELEASED:
    type = SAPP_EVENTTYPE_MOUSE_UP;
    break;
  default:
    break;
  }

  sapp_mousebutton btn = SAPP_MOUSEBUTTON_INVALID;
  switch (button) {
  case BTN_LEFT:
    btn = SAPP_MOUSEBUTTON_LEFT;
    break;
  case BTN_RIGHT:
    btn = SAPP_MOUSEBUTTON_RIGHT;
    break;
  case BTN_MIDDLE:
    btn = SAPP_MOUSEBUTTON_MIDDLE;
    break;
  default:
    break;
  }

  _sapp_wl_mouse_event(type, btn, _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_pointer_enter(
  void *data, struct wl_pointer *pointer, uint32_t serial,
  struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(surface);
  _SOKOL_UNUSED(surface_x);
  _SOKOL_UNUSED(surface_y);

  _sapp_wl.serial = serial;

  _sapp_wl_mouse_event(
    SAPP_EVENTTYPE_MOUSE_ENTER, SAPP_MOUSEBUTTON_INVALID,
    _sapp_wl_get_modifiers());
  _sapp_wl_update_cursor(_sapp.mouse.current_cursor, _sapp.mouse.shown);
}

_SOKOL_PRIVATE void
_sapp_wl_pointer_frame(void *data, struct wl_pointer *pointer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
}

_SOKOL_PRIVATE void _sapp_wl_pointer_leave(
  void *data, struct wl_pointer *pointer, uint32_t serial,
  struct wl_surface *surface) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(surface);

  _sapp_wl.serial = serial;

  /* unlock mouse on leave event */
  if (_sapp.mouse.locked) {
    _sapp_wl_lock_mouse(false);
  }
  _sapp_wl_mouse_event(
    SAPP_EVENTTYPE_MOUSE_LEAVE, SAPP_MOUSEBUTTON_INVALID,
    _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_pointer_motion(
  void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t surface_x,
  wl_fixed_t surface_y) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(pointer);
  _SOKOL_UNUSED(time);
  _SOKOL_UNUSED(surface_x);
  _SOKOL_UNUSED(surface_y);

  float new_x = (float)wl_fixed_to_double(surface_x) * _sapp.dpi_scale;
  float new_y = (float)wl_fixed_to_double(surface_y) * _sapp.dpi_scale;
  _sapp.mouse.x = new_x;
  _sapp.mouse.y = new_y;
  _sapp_wl_mouse_event(
    SAPP_EVENTTYPE_MOUSE_MOVE, SAPP_MOUSEBUTTON_INVALID,
    _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE const struct wl_pointer_listener _sapp_wl_pointer_listener = {
  .enter = _sapp_wl_pointer_enter,
  .leave = _sapp_wl_pointer_leave,
  .motion = _sapp_wl_pointer_motion,
  .button = _sapp_wl_pointer_button,
  .axis = _sapp_wl_pointer_axis,
  .frame = _sapp_wl_pointer_frame,
  .axis_source = _sapp_wl_pointer_axis_source,
  .axis_stop = _sapp_wl_pointer_axis_stop,
  .axis_discrete = _sapp_wl_pointer_axis_discrete,
};

_SOKOL_PRIVATE void _sapp_wl_relative_pointer_motion(
  void *data, struct zwp_relative_pointer_v1 *relative_pointer,
  uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy,
  wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(relative_pointer);
  _SOKOL_UNUSED(utime_hi);
  _SOKOL_UNUSED(utime_lo);
  _SOKOL_UNUSED(dx_unaccel);
  _SOKOL_UNUSED(dy_unaccel);

  /* don't update dx/dy in the very first update */
  if (_sapp.mouse.pos_valid) {
    _sapp.mouse.dx = (float)wl_fixed_to_double(dx);
    _sapp.mouse.dy = (float)wl_fixed_to_double(dy);
  }
  _sapp.mouse.pos_valid = true;
  _sapp_wl_mouse_event(
    SAPP_EVENTTYPE_MOUSE_MOVE, SAPP_MOUSEBUTTON_INVALID,
    _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE const struct zwp_relative_pointer_v1_listener
  _sapp_wl_relative_pointer_listener = {
    .relative_motion = _sapp_wl_relative_pointer_motion,
};

_SOKOL_PRIVATE void _sapp_wl_touch_cancel(void *data, struct wl_touch *touch) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);

  _sapp_wl_touch_event(
    SAPP_EVENTTYPE_TOUCHES_CANCELLED, 0, _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_touch_down(
  void *data, struct wl_touch *touch, uint32_t serial, uint32_t time,
  struct wl_surface *surface, int32_t id, wl_fixed_t x, wl_fixed_t y) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
  _SOKOL_UNUSED(time);
  _SOKOL_UNUSED(surface);

  _sapp_wl.serial = serial;

  struct _sapp_wl_touchpoint *point = _sapp_wl_get_touchpoint(id);
  if (NULL != point) {
    point->x = (float)wl_fixed_to_double(x);
    point->y = (float)wl_fixed_to_double(y);
  }
  _sapp_wl_touch_event(
    SAPP_EVENTTYPE_TOUCHES_BEGAN, id, _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_touch_frame(void *data, struct wl_touch *touch) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
}

_SOKOL_PRIVATE void _sapp_wl_touch_motion(
  void *data, struct wl_touch *touch, uint32_t time, int32_t id, wl_fixed_t x,
  wl_fixed_t y) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
  _SOKOL_UNUSED(time);

  struct _sapp_wl_touchpoint *point = _sapp_wl_get_touchpoint(id);
  if (NULL != point) {
    point->x = (float)wl_fixed_to_double(x);
    point->y = (float)wl_fixed_to_double(y);
  }
  _sapp_wl_touch_event(
    SAPP_EVENTTYPE_TOUCHES_MOVED, id, _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE void _sapp_wl_touch_orientation(
  void *data, struct wl_touch *touch, int32_t id, wl_fixed_t orientation) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
  _SOKOL_UNUSED(id);
  _SOKOL_UNUSED(orientation);
}

_SOKOL_PRIVATE void _sapp_wl_touch_shape(
  void *data, struct wl_touch *touch, int32_t id, wl_fixed_t major,
  wl_fixed_t minor) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
  _SOKOL_UNUSED(id);
  _SOKOL_UNUSED(major);
  _SOKOL_UNUSED(minor);
}

_SOKOL_PRIVATE void _sapp_wl_touch_up(
  void *data, struct wl_touch *touch, uint32_t serial, uint32_t time,
  int32_t id) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(touch);
  _SOKOL_UNUSED(time);

  _sapp_wl.serial = serial;

  _sapp_wl_get_touchpoint(id);
  _sapp_wl_touch_event(
    SAPP_EVENTTYPE_TOUCHES_ENDED, id, _sapp_wl_get_modifiers());
}

_SOKOL_PRIVATE const struct wl_touch_listener _sapp_wl_touch_listener = {
  .down = _sapp_wl_touch_down,
  .up = _sapp_wl_touch_up,
  .motion = _sapp_wl_touch_motion,
  .frame = _sapp_wl_touch_frame,
  .cancel = _sapp_wl_touch_cancel,
  .shape = _sapp_wl_touch_shape,
  .orientation = _sapp_wl_touch_orientation,
};

_SOKOL_PRIVATE void _sapp_wl_create_cursor(
  sapp_mouse_cursor cursor, struct wl_cursor_theme *theme, const char *name) {
  SOKOL_ASSERT((cursor >= 0) && (cursor < _SAPP_MOUSECURSOR_NUM));

  struct wl_cursor *curr_cursor = wl_cursor_theme_get_cursor(theme, name);
  if (NULL != curr_cursor) {
    _sapp_wl.cursors[cursor].cursor = curr_cursor;
    struct wl_buffer *curr_buffer =
      wl_cursor_image_get_buffer(curr_cursor->images[0]);
    _sapp_wl.cursors[cursor].buffer = curr_buffer;
  }
}

_SOKOL_PRIVATE void _sapp_wl_seat_handle_capabilities(
  void *data, struct wl_seat *seat, uint32_t capabilities) {
  _SOKOL_UNUSED(data);

  bool has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  bool has_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
  bool has_touch = capabilities & WL_SEAT_CAPABILITY_TOUCH;

  if (has_keyboard && NULL == _sapp_wl.keyboard) {
    _sapp_wl.keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(
      _sapp_wl.keyboard, &_sapp_wl_keyboard_listener, NULL);
  } else if (!has_keyboard && NULL != _sapp_wl.keyboard) {
    wl_keyboard_release(_sapp_wl.keyboard);
    _sapp_wl.keyboard = NULL;
  }

  if (has_pointer && NULL == _sapp_wl.pointer) {
    _sapp_wl.pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(_sapp_wl.pointer, &_sapp_wl_pointer_listener, NULL);

    if (NULL != _sapp_wl.relative_pointer_manager) {
      _sapp_wl.relative_pointer =
        zwp_relative_pointer_manager_v1_get_relative_pointer(
          _sapp_wl.relative_pointer_manager, _sapp_wl.pointer);
      zwp_relative_pointer_v1_add_listener(
        _sapp_wl.relative_pointer, &_sapp_wl_relative_pointer_listener, NULL);
    }

    if (NULL == _sapp_wl.cursor_surface) {
      struct wl_cursor_theme *cursor_theme =
        wl_cursor_theme_load(NULL, 24, _sapp_wl.shm);

      /* TODO: support for rtl default arrow? */
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_DEFAULT, cursor_theme, "left_ptr");
      _sapp_wl_create_cursor(SAPP_MOUSECURSOR_ARROW, cursor_theme, "right_ptr");
      _sapp_wl_create_cursor(SAPP_MOUSECURSOR_IBEAM, cursor_theme, "xterm");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_CROSSHAIR, cursor_theme, "crosshair");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_POINTING_HAND, cursor_theme, "hand2");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_RESIZE_EW, cursor_theme, "sb_h_double_arrow");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_RESIZE_NS, cursor_theme, "sb_v_double_arrow");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_RESIZE_NWSE, cursor_theme, "top_left_corner");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_RESIZE_NESW, cursor_theme, "top_right_corner");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_RESIZE_ALL, cursor_theme, "fleur");
      _sapp_wl_create_cursor(
        SAPP_MOUSECURSOR_NOT_ALLOWED, cursor_theme, "crossed_circle");

      _sapp_wl.cursor_surface =
        wl_compositor_create_surface(_sapp_wl.compositor);
    }
  } else if (!has_pointer && NULL != _sapp_wl.pointer) {
    wl_pointer_release(_sapp_wl.pointer);
    _sapp_wl.pointer = NULL;
  }

  if (has_touch && NULL == _sapp_wl.touch) {
    _sapp_wl.touch = wl_seat_get_touch(seat);
    wl_touch_add_listener(_sapp_wl.touch, &_sapp_wl_touch_listener, NULL);
    memset(&_sapp_wl.touchpoints, 0, sizeof(_sapp_wl.touchpoints));
  } else if (!has_touch && NULL != _sapp_wl.touch) {
    wl_touch_release(_sapp_wl.touch);
    _sapp_wl.touch = NULL;
  }
}

_SOKOL_PRIVATE void
_sapp_wl_seat_handle_name(void *data, struct wl_seat *seat, const char *name) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(seat);
  _SOKOL_UNUSED(name);
}

_SOKOL_PRIVATE const struct wl_seat_listener _sapp_wl_seat_listener = {
  .capabilities = _sapp_wl_seat_handle_capabilities,
  .name = _sapp_wl_seat_handle_name,
};

_SOKOL_PRIVATE void _sapp_wl_output_done(void *data, struct wl_output *output) {
  struct _sapp_wl_output *out = (struct _sapp_wl_output *)data;
  wl_output_set_user_data(output, out);
}

_SOKOL_PRIVATE void _sapp_wl_output_geometry(
  void *data, struct wl_output *output, int32_t x, int32_t y,
  int32_t physical_width, int32_t physical_height, int32_t subpixel,
  const char *make, const char *model, int32_t transform) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(output);
  _SOKOL_UNUSED(x);
  _SOKOL_UNUSED(y);
  _SOKOL_UNUSED(physical_width);
  _SOKOL_UNUSED(physical_height);
  _SOKOL_UNUSED(subpixel);
  _SOKOL_UNUSED(make);
  _SOKOL_UNUSED(model);
  _SOKOL_UNUSED(transform);
}

_SOKOL_PRIVATE void _sapp_wl_output_mode(
  void *data, struct wl_output *output, uint32_t flags, int32_t width,
  int32_t height, int32_t refresh) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(output);
  _SOKOL_UNUSED(flags);
  _SOKOL_UNUSED(width);
  _SOKOL_UNUSED(height);
  _SOKOL_UNUSED(refresh);
}

_SOKOL_PRIVATE void
_sapp_wl_output_scale(void *data, struct wl_output *output, int32_t factor) {
  _SOKOL_UNUSED(output);

  struct _sapp_wl_output *out = (struct _sapp_wl_output *)data;
  out->factor = factor;
}

_SOKOL_PRIVATE const struct wl_output_listener _sapp_wl_output_listener = {
  .geometry = _sapp_wl_output_geometry,
  .mode = _sapp_wl_output_mode,
  .done = _sapp_wl_output_done,
  .scale = _sapp_wl_output_scale,
};

_SOKOL_PRIVATE void _sapp_wl_registry_handle_global(
  void *data, struct wl_registry *registry, uint32_t name,
  const char *interface, uint32_t version) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(version);

  if (0 == strcmp(interface, wl_compositor_interface.name)) {
    /* bind to version 4 for: `wl_surface_damage_buffer` */
    _sapp_wl.compositor = (struct wl_compositor *)wl_registry_bind(
      registry, name, &wl_compositor_interface, 4);
  } else if (0 == strcmp(interface, xdg_wm_base_interface.name)) {
    _sapp_wl.wm_base = (struct xdg_wm_base *)wl_registry_bind(
      registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(
      _sapp_wl.wm_base, &_sapp_wl_wm_base_listener, NULL);
  } else if (0 == strcmp(interface, wl_seat_interface.name)) {
    /* bind to version 3 for:
     * - `wl_keyboard_release`,
     * - `wl_pointer_release`,
     * - `wl_pointer_release`,
     * - `repeat_info` event, */
    _sapp_wl.seat =
      (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, 4);
    wl_seat_add_listener(_sapp_wl.seat, &_sapp_wl_seat_listener, NULL);
  } else if (0 == strcmp(interface, wl_output_interface.name)) {
    if (_SAPP_WAYLAND_MAX_OUTPUTS > _sapp_wl.max_outputs) {
      struct _sapp_wl_output *output = &_sapp_wl.outputs[_sapp_wl.max_outputs];

      /* bind to version 2 for: `wl_output_done` */
      output->output = (struct wl_output *)wl_registry_bind(
        registry, name, &wl_output_interface, 2);
      output->factor = _SAPP_WAYLAND_DEFAULT_DPI_SCALE;

      if (NULL != output->output) {
        wl_output_add_listener(
          output->output, &_sapp_wl_output_listener, output);
        _sapp_wl.max_outputs++;
      }
    }
  } else if (0 == strcmp(interface, wl_shm_interface.name)) {
    _sapp_wl.shm =
      (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (
    0 == strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)) {
    _sapp_wl.relative_pointer_manager =
      (struct zwp_relative_pointer_manager_v1 *)wl_registry_bind(
        registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
  } else if (
    0 == strcmp(interface, zwp_pointer_constraints_v1_interface.name)) {
    _sapp_wl.pointer_constraints =
      (struct zwp_pointer_constraints_v1 *)wl_registry_bind(
        registry, name, &zwp_pointer_constraints_v1_interface, 1);
  } else if (0 == strcmp(interface, wl_data_device_manager_interface.name)) {
    _sapp_wl.data_device_manager =
      (struct wl_data_device_manager *)wl_registry_bind(
        registry, name, &wl_data_device_manager_interface, 3);
  }
}

_SOKOL_PRIVATE void _sapp_wl_registry_handle_global_remove(
  void *data, struct wl_registry *registry, uint32_t name) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(registry);
  _SOKOL_UNUSED(name);
}

_SOKOL_PRIVATE const struct wl_registry_listener _sapp_wl_registry_listener = {
  .global = _sapp_wl_registry_handle_global,
  .global_remove = _sapp_wl_registry_handle_global_remove,
};

_SOKOL_PRIVATE void _sapp_wl_surface_enter(
  void *data, struct wl_surface *surface, struct wl_output *output) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(surface);

  struct _sapp_wl_output *out =
    (struct _sapp_wl_output *)wl_output_get_user_data(output);
  out->active = true;
  _sapp_wl_resize_window(_sapp.window_width, _sapp.window_height);
}

_SOKOL_PRIVATE void _sapp_wl_surface_leave(
  void *data, struct wl_surface *surface, struct wl_output *output) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(surface);

  struct _sapp_wl_output *out =
    (struct _sapp_wl_output *)wl_output_get_user_data(output);
  out->active = false;
  _sapp_wl_resize_window(_sapp.window_width, _sapp.window_height);
}

_SOKOL_PRIVATE const struct wl_surface_listener _sapp_wl_surface_listener = {
  .enter = _sapp_wl_surface_enter,
  .leave = _sapp_wl_surface_leave,
};

_SOKOL_PRIVATE void _sapp_wl_shell_handle_configure(
  void *data, struct xdg_surface *shell, uint32_t serial) {
  _SOKOL_UNUSED(data);

  xdg_surface_ack_configure(shell, serial);
  if (NULL != _sapp_wl.egl_display && NULL != _sapp_wl.egl_surface) {
    _sapp_frame();
    eglSwapBuffers(_sapp_wl.egl_display, _sapp_wl.egl_surface);
  }
  wl_surface_damage_buffer(_sapp_wl.surface, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_commit(_sapp_wl.surface);
}

_SOKOL_PRIVATE const struct xdg_surface_listener _sapp_wl_shell_listener = {
  .configure = _sapp_wl_shell_handle_configure,
};

_SOKOL_PRIVATE void _sapp_wl_toplevel_handle_configure(
  void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height,
  struct wl_array *states) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(toplevel);
  _SOKOL_UNUSED(states);

  bool is_resizing = false;

  /* cast to custom array-struct to silence C++-compilation errors,
   * that complain about `void* data` being cast to `int* toplevel_state` */
  struct states_array {
    size_t size;
    size_t alloc;
    int *data;
  } *arr = (struct states_array *)states;

  int *toplevel_state;
  wl_array_for_each(toplevel_state, arr) {
    switch (*toplevel_state) {
    case XDG_TOPLEVEL_STATE_ACTIVATED:
    case XDG_TOPLEVEL_STATE_FULLSCREEN:
    case XDG_TOPLEVEL_STATE_MAXIMIZED:
    case XDG_TOPLEVEL_STATE_RESIZING:
      is_resizing = true;
      break;
    case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
    case XDG_TOPLEVEL_STATE_TILED_LEFT:
    case XDG_TOPLEVEL_STATE_TILED_RIGHT:
    case XDG_TOPLEVEL_STATE_TILED_TOP:
    default:
      break;
    }
  }

  if (is_resizing) {
    _sapp_wl_resize_window((int)width, (int)height);
  }
}

_SOKOL_PRIVATE void _sapp_wl_data_offer_handle_action(
  void *data, struct wl_data_offer *data_offer, uint32_t dnd_action) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_offer);
  _SOKOL_UNUSED(dnd_action);
}

_SOKOL_PRIVATE void _sapp_wl_data_offer_handle_offer(
  void *data, struct wl_data_offer *data_offer, const char *mime_type) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_offer);

  if (0 == strcmp(mime_type, "text/uri-list")) {
    wl_data_offer_accept(_sapp_wl.data_offer, _sapp_wl.serial, mime_type);
  }
}

_SOKOL_PRIVATE void _sapp_wl_data_offer_handle_source_actions(
  void *data, struct wl_data_offer *data_offer, uint32_t source_actions) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_offer);
  _SOKOL_UNUSED(source_actions);
}

_SOKOL_PRIVATE const struct wl_data_offer_listener
  _sapp_wl_data_offer_listener = {
    .offer = _sapp_wl_data_offer_handle_offer,
    .source_actions = _sapp_wl_data_offer_handle_source_actions,
    .action = _sapp_wl_data_offer_handle_action,
};

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_data_offer(
  void *data, struct wl_data_device *data_device, struct wl_data_offer *offer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);

  if (!_sapp.drop.enabled && !_sapp.clipboard.enabled) {
    return;
  }

  if (NULL != _sapp_wl.data_offer) {
    wl_data_offer_destroy(_sapp_wl.data_offer);
  }

  _sapp_wl.data_offer = offer;
  wl_data_offer_add_listener(
    _sapp_wl.data_offer, &_sapp_wl_data_offer_listener, NULL);
}

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_drop(
  void *data, struct wl_data_device *data_device) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);

  if (!_sapp.drop.enabled || NULL == _sapp_wl.data_offer) {
    return;
  }

  int fds[2];
  if (pipe(fds) < 0) {
    _sapp_fail(
      "wayland: pipe() failed to create drag-and-drop communication channel");
  }
  wl_data_offer_receive(_sapp_wl.data_offer, "text/uri-list", fds[1]);
  close(fds[1]);

  wl_display_roundtrip_queue(_sapp_wl.display, _sapp_wl.event_queue);

  size_t buf_size = (size_t)_sapp.drop.buf_size;
  char *buf = (char *)_sapp_malloc(buf_size);
  if (read(fds[0], buf, buf_size) < 0) {
    _sapp_fail("wayland: read() failed to receive drag-and-drop offer");
  }
  close(fds[0]);

  if (_sapp.drop.enabled) {
    if (_sapp_linux_parse_dropped_files_list(buf)) {
      if (_sapp_events_enabled()) {
        _sapp_init_event(SAPP_EVENTTYPE_FILES_DROPPED);
        _sapp_call_event(&_sapp.event);
      }
    }
  }

  wl_data_offer_finish(_sapp_wl.data_offer);
  wl_data_offer_destroy(_sapp_wl.data_offer);
  _sapp_wl.data_offer = NULL;

  _sapp_free(buf);
}

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_enter(
  void *data, struct wl_data_device *data_device, uint32_t serial,
  struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
  struct wl_data_offer *offer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);
  _SOKOL_UNUSED(serial);
  _SOKOL_UNUSED(surface);
  _SOKOL_UNUSED(x);
  _SOKOL_UNUSED(y);
  _SOKOL_UNUSED(offer);

  wl_data_offer_set_actions(
    _sapp_wl.data_offer,
    WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
    WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
}

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_leave(
  void *data, struct wl_data_device *data_device) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);
}

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_motion(
  void *data, struct wl_data_device *data_device, uint32_t time, wl_fixed_t x,
  wl_fixed_t y) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);
  _SOKOL_UNUSED(time);
  _SOKOL_UNUSED(x);
  _SOKOL_UNUSED(y);
}

_SOKOL_PRIVATE void _sapp_wl_data_device_handle_selection(
  void *data, struct wl_data_device *data_device, struct wl_data_offer *offer) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(data_device);
  _SOKOL_UNUSED(offer);

  if (!_sapp.clipboard.enabled || NULL == _sapp_wl.data_offer) {
    return;
  }

  int fds[2];
  if (pipe(fds) < 0) {
    _sapp_fail("wayland: pipe() failed to create communication channel for "
               "clipboard selection");
  }
  wl_data_offer_receive(_sapp_wl.data_offer, "text/plain", fds[1]);
  close(fds[1]);

  wl_display_roundtrip_queue(_sapp_wl.display, _sapp_wl.event_queue);

  size_t max_read = (size_t)_sapp.clipboard.buf_size - 1;
  size_t bytes_read = 0;
  size_t total_bytes_read = 0;
  while (total_bytes_read < max_read) {
    // read() gives a max 65536 bytes at a time, regardless of buf_size
    bytes_read = read(
      fds[0], _sapp.clipboard.buffer + total_bytes_read,
      max_read - total_bytes_read);
    if (bytes_read > 0)
      total_bytes_read += bytes_read;
    else
      break;
  }
  _sapp.clipboard.buffer[total_bytes_read] = 0;

  char buffer[128];
  snprintf(
    buffer, 128, "Read %d bytes into clipboard buffer of size %d and %s",
    total_bytes_read, max_read,
    bytes_read > 0    ? "ran out of clipboard space"
    : bytes_read == 0 ? "reached EOF"
                      : "failed reading");

  if (bytes_read < 0) {
    _sapp_fail(buffer);
  } else {
    _sapp_info(buffer); // Remove after below TODO
  }
  // TODO only accept & receive the offer when Ctrl+V is pressed
  // otherwise every time the window gains focus it grabs a copy of the
  // clipboard as shown by this log message

  close(fds[0]);

  wl_data_offer_finish(_sapp_wl.data_offer);
  wl_data_offer_destroy(_sapp_wl.data_offer);
  _sapp_wl.data_offer = NULL;
}

_SOKOL_PRIVATE const struct wl_data_device_listener
  _sapp_wl_data_device_listener = {
    .data_offer = _sapp_wl_data_device_handle_data_offer,
    .enter = _sapp_wl_data_device_handle_enter,
    .leave = _sapp_wl_data_device_handle_leave,
    .motion = _sapp_wl_data_device_handle_motion,
    .drop = _sapp_wl_data_device_handle_drop,
    .selection = _sapp_wl_data_device_handle_selection,
};

_SOKOL_PRIVATE void
_sapp_wl_toplevel_handle_close(void *data, struct xdg_toplevel *toplevel) {
  _SOKOL_UNUSED(data);
  _SOKOL_UNUSED(toplevel);

  _sapp.quit_requested = true;
}

_SOKOL_PRIVATE const struct xdg_toplevel_listener _sapp_wl_toplevel_listener = {
  .configure = _sapp_wl_toplevel_handle_configure,
  .close = _sapp_wl_toplevel_handle_close,
};

_SOKOL_PRIVATE void _sapp_wl_setup(const sapp_desc *desc) {
  _sapp_wl.display = wl_display_connect(NULL);
  if (NULL == _sapp_wl.display) {
    _sapp_fail("wayland: wl_display_connect() failed");
  }

  struct wl_display *wrapped_display =
    (struct wl_display *)wl_proxy_create_wrapper(_sapp_wl.display);

  _sapp_wl.event_queue = wl_display_create_queue(_sapp_wl.display);
  wl_proxy_set_queue((struct wl_proxy *)wrapped_display, _sapp_wl.event_queue);
  if (NULL == _sapp_wl.event_queue) {
    _sapp_fail("wayland: wl_proxy_set_queue() failed");
  }

  _sapp_wl.registry = wl_display_get_registry(wrapped_display);
  wl_proxy_wrapper_destroy(wrapped_display);

  wl_registry_add_listener(
    _sapp_wl.registry, &_sapp_wl_registry_listener, NULL);
  wl_display_roundtrip_queue(_sapp_wl.display, _sapp_wl.event_queue);

  if (NULL == _sapp_wl.compositor) {
    _sapp_fail("wayland: wl_register_add_listener() failed");
  }

  _sapp_wl.surface = wl_compositor_create_surface(_sapp_wl.compositor);
  if (NULL == _sapp_wl.surface) {
    _sapp_fail("wayland: wl_compositor_create_surface() failed");
  }
  wl_surface_add_listener(_sapp_wl.surface, &_sapp_wl_surface_listener, NULL);

  _sapp_wl.shell =
    xdg_wm_base_get_xdg_surface(_sapp_wl.wm_base, _sapp_wl.surface);
  if (NULL == _sapp_wl.shell) {
    _sapp_fail("wayland: xdg_wm_base_get_xdg_surface() failed");
  }
  xdg_surface_add_listener(_sapp_wl.shell, &_sapp_wl_shell_listener, &_sapp_wl);

  _sapp_wl.toplevel = xdg_surface_get_toplevel(_sapp_wl.shell);
  if (NULL == _sapp_wl.toplevel) {
    _sapp_fail("wayland: xdg_surface_get_toplevel() failed");
  }
  xdg_toplevel_add_listener(
    _sapp_wl.toplevel, &_sapp_wl_toplevel_listener, &_sapp_wl);
  xdg_toplevel_set_title(_sapp_wl.toplevel, desc->window_title);
  wl_surface_commit(_sapp_wl.surface);

  _sapp_wl.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  if (NULL != _sapp_wl.seat && NULL != _sapp_wl.data_device_manager) {
    _sapp_wl.data_device = wl_data_device_manager_get_data_device(
      _sapp_wl.data_device_manager, _sapp_wl.seat);
    wl_data_device_add_listener(
      _sapp_wl.data_device, &_sapp_wl_data_device_listener, NULL);
  }
}

_SOKOL_PRIVATE void _sapp_wl_egl_setup(const sapp_desc *desc) {
#if defined(SOKOL_GLCORE33)
  if (!eglBindAPI(EGL_OPENGL_API)) {
    _sapp_fail("wayland: eglBindAPI() failed");
  }
#else  /* SOKOL_GLCORE33 */
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    _sapp_fail("wayland: eglBindAPI() failed");
  }
#endif /* SOKOL_GLCORE33 */

  _sapp_wl.egl_display =
    (EGLDisplay *)eglGetDisplay((EGLNativeDisplayType)_sapp_wl.display);
  if (EGL_NO_DISPLAY == _sapp_wl.egl_display) {
    _sapp_fail("wayland: eglGetDisplay() failed");
  }

  EGLint major, minor;
  if (EGL_FALSE == eglInitialize(_sapp_wl.egl_display, &major, &minor)) {
    _sapp_fail("wayland: eglInitialize() failed");
  }

  EGLint alpha = desc->alpha ? 8 : 0;
  EGLint sample_buffers = desc->sample_count > 1 ? 1 : 0;

  EGLint total_config_count;
  eglGetConfigs(_sapp_wl.egl_display, NULL, 0, &total_config_count);
  EGLConfig *egl_configs =
    (EGLConfig *)_sapp_malloc((size_t)total_config_count * sizeof(EGLConfig));
  EGLint attribs[] = {
    EGL_SURFACE_TYPE,
    EGL_WINDOW_BIT,

    EGL_RED_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_BLUE_SIZE,
    8,
    EGL_ALPHA_SIZE,
    alpha,

#if defined(SOKOL_GLCORE33)
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_BIT,
#else  /* SOKOL_GLCORE33 */
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES3_BIT,
#endif /* SOKOL_GLCORE33 */

    EGL_DEPTH_SIZE,
    24,
    EGL_STENCIL_SIZE,
    8,
    EGL_SAMPLE_BUFFERS,
    sample_buffers,
    EGL_SAMPLES,
    desc->sample_count,

    EGL_NONE,
  };

  EGLint config_count;
  if (!eglChooseConfig(
        _sapp_wl.egl_display, attribs, egl_configs, total_config_count,
        &config_count)) {
    _sapp_free(egl_configs);

    /* format the egl error into a printable string */
    const char *fmt = "wayland: eglChooseConfig() failed (%x)";
    const size_t len = strlen(fmt) + 16;
    char *msg = (char *)_sapp_malloc(len * sizeof(char));
    snprintf(msg, len, fmt, eglGetError());
    _sapp_fail(msg);
  }

  int32_t egl_config_id = -1;
  for (int32_t i = 0; i < config_count; i++) {
    EGLConfig c = egl_configs[i];
    EGLint r, g, b, a, d, s, m, n;
    EGLBoolean res =
      eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_RED_SIZE, &r);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_GREEN_SIZE, &g);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_BLUE_SIZE, &b);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_ALPHA_SIZE, &a);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_DEPTH_SIZE, &d);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_STENCIL_SIZE, &s);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_SAMPLE_BUFFERS, &m);
    res &= eglGetConfigAttrib(_sapp_wl.egl_display, c, EGL_SAMPLES, &n);

    if (
      EGL_TRUE == res && 8 == r && 8 == g && 8 == b && 8 == alpha && 24 == d &&
      8 == s && sample_buffers == m && desc->sample_count == n) {
      egl_config_id = i;
      break;
    }
  }

  /* use config 0 if no config matches the desired one */
  egl_config_id = 0 <= egl_config_id ? egl_config_id : 0;

  EGLint context_attrib[] = {
#if defined(SOKOL_GLCORE33)
    EGL_CONTEXT_MAJOR_VERSION,
    3,
    EGL_CONTEXT_MINOR_VERSION,
    3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK,
    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#endif /* SOKOL_GLCORE33 */

    EGL_NONE,
  };

  _sapp_wl.egl_context = (EGLContext *)eglCreateContext(
    _sapp_wl.egl_display, egl_configs[egl_config_id], EGL_NO_CONTEXT,
    context_attrib);
  if (EGL_NO_CONTEXT == _sapp_wl.egl_context) {
    _sapp_free(egl_configs);
    _sapp_fail("wayland: eglCreateContext() failed");
  }

  int w = _sapp_def(desc->width, _SAPP_FALLBACK_DEFAULT_WINDOW_WIDTH);
  int h = _sapp_def(desc->height, _SAPP_FALLBACK_DEFAULT_WINDOW_HEIGHT);
  _sapp_wl.egl_window = wl_egl_window_create(_sapp_wl.surface, w, h);
  if (EGL_NO_CONTEXT == _sapp_wl.egl_window) {
    _sapp_free(egl_configs);
    _sapp_fail("wayland: wl_egl_window_create() failed");
  }

  _sapp_wl.egl_surface = (EGLSurface *)eglCreateWindowSurface(
    _sapp_wl.egl_display, egl_configs[egl_config_id],
    (EGLNativeWindowType)_sapp_wl.egl_window, NULL);
  _sapp_free(egl_configs);
  if (EGL_NO_SURFACE == _sapp_wl.egl_window) {
    _sapp_fail("wayland: eglCreateWindowSurface() failed");
  }

  if (!eglMakeCurrent(
        _sapp_wl.egl_display, _sapp_wl.egl_surface, _sapp_wl.egl_surface,
        _sapp_wl.egl_context)) {
    _sapp_fail("wayland: eglMakeCurrent() failed");
  }
  eglSwapInterval(_sapp_wl.egl_display, _sapp.swap_interval);

  int api_type, api_version;
  eglQueryContext(
    _sapp_wl.egl_display, _sapp_wl.egl_context, EGL_CONTEXT_CLIENT_TYPE,
    &api_type);
  eglQueryContext(
    _sapp_wl.egl_display, _sapp_wl.egl_context, EGL_CONTEXT_CLIENT_VERSION,
    &api_version);
}

_SOKOL_PRIVATE void _sapp_wl_sighandler_setup(void) {
  _sapp_wl.epoll_fd = epoll_create1(0);
  if (0 > _sapp_wl.epoll_fd) {
    _sapp_fail("wayland: epoll_create1() failed");
  }

  SOKOL_ASSERT(NULL != _sapp_wl.display);
  _sapp_wl.event_fd = wl_display_get_fd(_sapp_wl.display);
  if (0 > _sapp_wl.event_fd) {
    _sapp_fail("wayland: wl_display_get_fd() failed");
  }

  struct epoll_event ev = {0};
  ev.events = EPOLLIN;
  ev.data.fd = _sapp_wl.event_fd;
  if (0 > epoll_ctl(_sapp_wl.epoll_fd, EPOLL_CTL_ADD, _sapp_wl.event_fd, &ev)) {
    _sapp_fail("wayland: epoll_ctl() failed");
  }
}

_SOKOL_PRIVATE void _sapp_linux_run(const sapp_desc *desc) {
  /* The following lines are here to trigger a linker error instead of an
      obscure runtime error if the user has forgotten to add -pthread to
      the compiler or linker options. They have no other purpose.
  */
  pthread_attr_t pthread_attr;
  pthread_attr_init(&pthread_attr);
  pthread_attr_destroy(&pthread_attr);

  _sapp_init_state(desc);

  _sapp_wl_setup(&_sapp.desc);
  _sapp_wl_egl_setup(&_sapp.desc);
  _sapp_wl_sighandler_setup();
  if (_sapp.fullscreen) {
    _sapp_wl_set_fullscreen(true);
  }

  _sapp.valid = true;
  while (!_sapp.quit_ordered) {
    _sapp_timing_measure(&_sapp.timing);
    wl_display_dispatch_queue(_sapp_wl.display, _sapp_wl.event_queue);
    _sapp_frame();
    eglSwapBuffers(_sapp_wl.egl_display, _sapp_wl.egl_surface);
    wl_surface_damage_buffer(_sapp_wl.surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(_sapp_wl.surface);

    /* emulate key repeats */
    if (_sapp_wl.repeat_key_code != SAPP_KEYCODE_INVALID) {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      if (_sapp_wl_timespec_cmp(&now, &_sapp_wl.repeat_next) > 0) {
        uint32_t modifiers = _sapp_wl_get_modifiers();
        _sapp_wl_key_event(
          SAPP_EVENTTYPE_KEY_DOWN, _sapp_wl.repeat_key_code, true, modifiers);

        if (_sapp_wl.repeat_key_char) {
          _sapp_wl_char_event(_sapp_wl.repeat_key_char, false, modifiers);
        }

        _sapp_wl_timespec_add(
          &now, &_sapp_wl.repeat_rate, &_sapp_wl.repeat_next);
      }
    }

    if (_sapp.quit_requested) {
      _sapp_wl_app_event(SAPP_EVENTTYPE_QUIT_REQUESTED);
      if (_sapp.quit_requested) {
        _sapp.quit_ordered = true;
      }
    }
  }

  _sapp_call_cleanup();
  _sapp_wl_cleanup();
  _sapp_discard_state();
}
