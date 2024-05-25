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

#ifndef AUTOROUTE_H
#define AUTOROUTE_H

#include "view/view.h"

typedef struct AutoRoute AutoRoute;

typedef struct RouteTimeStats {
  struct {
    uint64_t avg;
    uint64_t min;
    uint64_t max;
  } build;
  struct {
    uint64_t avg;
    uint64_t min;
    uint64_t max;
  } route;
  int samples;
} RouteTimeStats;

void autoroute_global_init();
AutoRoute *autoroute_create(Circuit *circuit);
void autoroute_free(AutoRoute *ar);
void autoroute_route(AutoRoute *ar, bool betterRoutes);

void autoroute_draw_debug_lines(AutoRoute *ar, void *ctx);
void autoroute_dump_anchor_boxes(AutoRoute *ar);
RouteTimeStats autoroute_stats(AutoRoute *ar);

#endif // AUTOROUTE_H