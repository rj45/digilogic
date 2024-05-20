/*
github.com/jnguyen1098/seethe

ISC License (ISC)

Copyright 2021 Jason Nguyen

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
*/
#ifndef SEETHE_H
#define SEETHE_H

#include <stdio.h>
#include <time.h>

/* Default level */
#ifndef LOG_LEVEL
#define LOG_LEVEL LL_WARNING
#endif

/* Colour customization */
#define DEBUG_COLOUR ""
#define INFO_COLOUR "\x1B[36m"
#define NOTICE_COLOUR "\x1B[32;1m"
#define WARNING_COLOUR "\x1B[33m"
#define ERROR_COLOUR "\x1B[31m"
#define CRITICAL_COLOUR "\x1B[41;1m"

/* Do not change this. */
#define RESET_COLOUR "\x1B[0m"

/* Formatting prefs. */
#define MSG_ENDING "\n"
#define TIME_FORMAT "%T"
#define BORDER "-"

/* Enabler flags */
#define DISPLAY_COLOUR 1
#define DISPLAY_TIME 1
#define DISPLAY_LEVEL 1
#define DISPLAY_FUNC 1
#define DISPLAY_FILE 1
#define DISPLAY_LINE 1
#define DISPLAY_BORDER 1
#define DISPLAY_MESSAGE 1
#define DISPLAY_ENDING 1
#define DISPLAY_RESET 1

/* Log to screen */
#define log_emit(colour, level, file, func, line, ...)                         \
  do {                                                                         \
                                                                               \
    /* notate the time */                                                      \
    time_t raw_time = time(NULL);                                              \
    char time_buffer[80];                                                      \
    strftime(time_buffer, 80, TIME_FORMAT, localtime(&raw_time));              \
                                                                               \
    /* enable colour */                                                        \
    printf("%s", DISPLAY_COLOUR ? colour : "");                                \
                                                                               \
    /* display the time */                                                     \
    printf("%s%s", DISPLAY_TIME ? time_buffer : "", DISPLAY_TIME ? " " : "");  \
                                                                               \
    /* display the level */                                                    \
    printf("%10s%s", DISPLAY_LEVEL ? level : "", DISPLAY_LEVEL ? " " : "");    \
                                                                               \
    char *fixedfile = file;                                                    \
    for (int i = strlen(file) - 1; i >= 0; i--) {                              \
      if (file[i] == '/' || file[i] == '\\') {                                 \
        fixedfile = &file[i + 1];                                              \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    /* display the function doing the logging */                               \
    printf("%s%s", DISPLAY_FUNC ? func : "", DISPLAY_FUNC ? " " : "");         \
                                                                               \
    /* display the file and/or the line number */                              \
    printf(                                                                    \
      "%s%s%s%.d%s%s",                                                         \
      DISPLAY_FUNC && (DISPLAY_FILE || DISPLAY_LINE) ? "(" : "",               \
      DISPLAY_FILE ? fixedfile : "", DISPLAY_FILE && DISPLAY_LINE ? ":" : "",  \
      DISPLAY_LINE ? line : 0,                                                 \
      DISPLAY_FUNC && (DISPLAY_FILE || DISPLAY_LINE) ? ") " : "",              \
      !DISPLAY_FUNC && (DISPLAY_FILE || DISPLAY_LINE) ? " " : "");             \
                                                                               \
    /* display message border */                                               \
    printf("%s%s", DISPLAY_BORDER ? BORDER : "", DISPLAY_BORDER ? " " : "");   \
                                                                               \
    /* display the callee's message */                                         \
    if (DISPLAY_MESSAGE)                                                       \
      printf(__VA_ARGS__);                                                     \
                                                                               \
    /* add the message ending (usually '\n') */                                \
    printf("%s", DISPLAY_ENDING ? MSG_ENDING : "");                            \
                                                                               \
    /* reset the colour */                                                     \
    printf("%s", DISPLAY_RESET ? RESET_COLOUR : "");                           \
                                                                               \
  } while (0)

/* Level enum */
#define LL_DEBUG 0
#define LL_INFO 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_ERROR 4
#define LL_CRITICAL 5
#define LL_SILENT 6

/* DEBUG LOG */
#define log_debug(...)                                                         \
  do {                                                                         \
    if (LOG_LEVEL == LL_DEBUG) {                                               \
      log_emit(                                                                \
        DEBUG_COLOUR, "[DEBUG]", __FILE__, __func__, __LINE__, __VA_ARGS__);   \
    }                                                                          \
  } while (0)

/* INFO LOG */
#define log_info(...)                                                          \
  do {                                                                         \
    if (LOG_LEVEL <= LL_INFO) {                                                \
      log_emit(                                                                \
        INFO_COLOUR, "[INFO]", __FILE__, __func__, __LINE__, __VA_ARGS__);     \
    }                                                                          \
  } while (0)

/* NOTICE LOG */
#define log_notice(...)                                                        \
  do {                                                                         \
    if (LOG_LEVEL <= LL_NOTICE) {                                              \
      log_emit(                                                                \
        NOTICE_COLOUR, "[NOTICE]", __FILE__, __func__, __LINE__, __VA_ARGS__); \
    }                                                                          \
  } while (0)

/* WARNING LOG */
#define log_warning(...)                                                       \
  do {                                                                         \
    if (LOG_LEVEL <= LL_WARNING) {                                             \
      log_emit(                                                                \
        WARNING_COLOUR, "[WARNING]", __FILE__, __func__, __LINE__,             \
        __VA_ARGS__);                                                          \
    }                                                                          \
  } while (0)

/* ERROR LOG */
#define log_error(...)                                                         \
  do {                                                                         \
    if (LOG_LEVEL <= LL_ERROR) {                                               \
      log_emit(                                                                \
        ERROR_COLOUR, "[ERROR]", __FILE__, __func__, __LINE__, __VA_ARGS__);   \
    }                                                                          \
  } while (0)

/* CRITICAL LOG */
#define log_critical(...)                                                      \
  do {                                                                         \
    if (LOG_LEVEL <= LL_CRITICAL) {                                            \
      log_emit(                                                                \
        CRITICAL_COLOUR, "[CRITICAL]", __FILE__, __func__, __LINE__,           \
        __VA_ARGS__);                                                          \
    }                                                                          \
  } while (0)

#endif // seethe.h