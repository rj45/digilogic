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

#ifndef LOG_H
#define LOG_H

#include "sokol_log.h"

#define _LOG_ITEMS                                                             \
  _LOGITEM_XMACRO(OK, "Ok")                                                    \
  _LOGITEM_XMACRO(MALLOC_FAILED, "memory allocation failed")                   \
  _LOGITEM_XMACRO(IMAGE_POOL_EXHAUSTED, "image pool exhausted")                \
  _LOGITEM_XMACRO(IMAGE_LOAD_FAILED, "image load failed")                      \
  _LOGITEM_XMACRO(HERE1, "got here 1")                                         \
  _LOGITEM_XMACRO(HERE2, "got here 2")                                         \
  _LOGITEM_XMACRO(HERE3, "got here 3")

#define _LOGITEM_XMACRO(item, msg) LOGITEM_##item,
typedef enum log_item_t { _LOG_ITEMS } log_item_t;
#undef _LOGITEM_XMACRO

#if defined(DEBUG)
#define _LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_log_messages[] = {_LOG_ITEMS};
#undef _LOGITEM_XMACRO
#endif // DEBUG

#ifndef LOG_TAG
#define LOG_TAG "<unk>"
#endif

#define LOG_PANIC(code)                                                        \
  _usr_log(LOG_TAG, LOGITEM_##code, 0, 0, __FILE__, __LINE__)
#define LOG_ERROR(code)                                                        \
  _usr_log(LOG_TAG, LOGITEM_##code, 1, 0, __FILE__, __LINE__)
#define LOG_WARN(code)                                                         \
  _usr_log(LOG_TAG, LOGITEM_##code, 2, 0, __FILE__, __LINE__)
#define LOG_INFO(code)                                                         \
  _usr_log(LOG_TAG, LOGITEM_##code, 3, 0, __FILE__, __LINE__)
#define LOG_INFOMSG(code, msg)                                                 \
  _usr_log(LOGITEM_##code, 3, msg, __FILE__, __LINE__)

static inline void _usr_log(
  const char *tag, log_item_t log_item, uint32_t log_level, const char *msg,
  const char *filename, uint32_t line_nr) {
#if defined(DEBUG)
  if (0 == msg) {
    msg = _log_messages[log_item];
  }
#endif

  slog_func(tag, log_level, log_item, msg, line_nr, filename, 0);
}

#endif // LOG_H
