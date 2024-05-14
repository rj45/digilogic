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

void autoroute_global_init();
AutoRoute *autoroute_create(CircuitView *view);
void autoroute_free(AutoRoute *ar);
void autoroute_update_component(AutoRoute *ar, ID id);
void autoroute_update_wire(AutoRoute *ar, ID id);
void autoroute_update_waypoint(AutoRoute *ar, ID id);
void autoroute_update_endpoint(AutoRoute *ar, ID id);
void autoroute_update_net(AutoRoute *ar, NetID id);
void autoroute_route(AutoRoute *ar, bool betterRoutes);
// size_t autoroute_wire_vertices(
//   AutoRoute *ar, WireID wireID, float *coords, size_t maxLen);
// void autoroute_get_junction_pos(
//   AutoRoute *ar, JunctionID junctionID, float *x, float *y);
// size_t autoroute_get_net_port_vertices(
//   AutoRoute *ar, int netIndex, int portIndex, HMM_Vec2 **vertices);

void autoroute_draw_debug_lines(AutoRoute *ar, void *ctx);
void autoroute_dump_anchor_boxes(AutoRoute *ar);

#endif // AUTOROUTE_H