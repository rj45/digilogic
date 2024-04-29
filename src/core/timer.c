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

static int64_t timer_muldiv(int64_t value, int64_t numer, int64_t denom) {
  int64_t q = value / denom;
  int64_t r = value % denom;
  return q * numer + r * numer / denom;
}

void timer_init(Timer *ts) {
#if defined(__APPLE__)
  mach_timebase_info(&ts->mach.timebase);
  ts->mach.start = mach_absolute_time();
#elif defined(__EMSCRIPTEN__)
  (void)ts;
#elif defined(_WIN32)
  QueryPerformanceFrequency(&ts->win.freq);
  QueryPerformanceCounter(&ts->win.start);
#else
  struct timespec tspec;
  clock_gettime(_SAPP_CLOCK_MONOTONIC, &tspec);
  ts->posix.start =
    (uint64_t)tspec.tv_sec * 1000000000 + (uint64_t)tspec.tv_nsec;
#endif
}

double timer_now(Timer *ts) {
#if defined(__APPLE__)
  const uint64_t traw = mach_absolute_time() - ts->mach.start;
  const uint64_t now = (uint64_t)timer_muldiv(
    (int64_t)traw, (int64_t)ts->mach.timebase.numer,
    (int64_t)ts->mach.timebase.denom);
  return (double)now / 1000000000.0;
#elif defined(__EMSCRIPTEN__)
  (void)ts;
  SOKOL_ASSERT(false);
  return 0.0;
#elif defined(_WIN32)
  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);
  const uint64_t now = (uint64_t)timer_muldiv(
    qpc.QuadPart - ts->win.start.QuadPart, 1000000000, ts->win.freq.QuadPart);
  return (double)now / 1000000000.0;
#else
  struct timespec tspec;
  clock_gettime(TIMER_CLOCK_MONOTONIC, &tspec);
  const uint64_t now =
    ((uint64_t)tspec.tv_sec * 1000000000 + (uint64_t)tspec.tv_nsec) -
    ts->posix.start;
  return (double)now / 1000000000.0;
#endif
}