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

#include "core/core.h"
#include "render/draw.h"

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

typedef struct RoutingConfig {
  bool minimizeGraph;
  bool performCentering;
  bool recordReplay;
} RoutingConfig;

void autoroute_global_init();
AutoRoute *autoroute_create(Circuit *circ);
void autoroute_free(AutoRoute *ar);
void autoroute_route(AutoRoute *ar, RoutingConfig config);
bool autoroute_dump_routing_data(
  AutoRoute *ar, RoutingConfig config, const char *filename);
void autoroute_draw_debug_lines(AutoRoute *ar, DrawContext *ctx);
void autoroute_dump_anchor_boxes(AutoRoute *ar);
RouteTimeStats autoroute_stats(AutoRoute *ar);

void autoroute_replay_rewind(AutoRoute *ar);
bool autoroute_replay_forward(AutoRoute *ar);
bool autoroute_replay_forward_to(AutoRoute *ar, int event);
bool autoroute_replay_forward_skip_path(AutoRoute *ar);
bool autoroute_replay_forward_skip_root(AutoRoute *ar);
bool autoroute_replay_backward(AutoRoute *ar);
bool autoroute_replay_backward_skip_path(AutoRoute *ar);
bool autoroute_replay_backward_skip_root(AutoRoute *ar);
int autoroute_replay_current_event(AutoRoute *ar);
int autoroute_replay_event_count(AutoRoute *ar);
void autoroute_replay_draw(AutoRoute *ar, DrawContext *ctx, FontHandle font);
void autoroute_replay_event_text(AutoRoute *ar, char *buf, size_t maxlen);

#endif // AUTOROUTE_H