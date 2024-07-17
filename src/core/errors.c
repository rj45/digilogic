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

#include "core/core.h"
#include <stdarg.h>

void errstack_init(ErrStack *errs) { errs->topOfStack = 0; }

bool errorf_friendly_(
  ErrStack *errs, const char *file, const char *func, int line, ErrorCode code,
  const char *fmt, ...) {

  errs->errStack[errs->topOfStack] = (ErrorInfo){
    .file = file,
    .func = func,
    .line = line,
    .code = code,
  };

  va_list args;
  va_start(args, fmt);
  vsnprintf(
    errs->errStack[errs->topOfStack].userMsg, MAX_USER_MSG_LEN, fmt, args);
  va_end(args);

  errs->topOfStack++;

  return false;
}

bool errorf_detailed(ErrStack *errs, const char *fmt, ...) {
  assert(errs->topOfStack > 0);

  va_list args;
  va_start(args, fmt);
  vsnprintf(
    errs->errStack[errs->topOfStack - 1].devMsg, MAX_DEV_MSG_LEN, fmt, args);
  va_end(args);

  return false;
}

ErrorCode errstack_last(ErrStack *errs) {
  assert(errs->topOfStack > 0);
  return errs->errStack[errs->topOfStack - 1].code;
}

void errstack_clear(ErrStack *errs) { errs->topOfStack = 0; }

void errstack_print(ErrStack *errs) {
  for (int i = 0; i < errs->topOfStack; i++) {
    ErrorInfo *err = &errs->errStack[i];
    fprintf(
      stderr, "%s:%d: %s: %s\n", err->file, err->line, err->func, err->userMsg);
    if (err->devMsg[0] != '\0') {
      fprintf(stderr, "  %s\n", err->devMsg);
    }
  }
}