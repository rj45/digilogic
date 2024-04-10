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

#ifndef AVOID_H
#define AVOID_H

#include "core/core.h"

#ifdef __cplusplus
#define EXPORT_C extern "C"
#include <cstddef>
#else
#define EXPORT_C
#include <stddef.h>
#endif

typedef void AvoidRouter;

typedef enum PortSide {
  SIDE_TOP,
  SIDE_RIGHT,
  SIDE_BOTTOM,
  SIDE_LEFT,
} PortSide;

EXPORT_C AvoidRouter *avoid_new();
EXPORT_C void avoid_free(AvoidRouter *a);

EXPORT_C void avoid_add_node(
  AvoidRouter *a, ComponentID nodeID, float x, float y, float w, float h);
EXPORT_C void avoid_add_port(
  AvoidRouter *a, PortID portID, ComponentID nodeID, PortSide side,
  float centerX, float centerY);
EXPORT_C void avoid_add_edge(
  AvoidRouter *a, NetID edgeID, ComponentID srcNodeID, PortID srcPortID,
  ComponentID dstNodeID, PortID dstPortID);
EXPORT_C void avoid_route(AvoidRouter *a);
EXPORT_C size_t
avoid_get_edge_path(AvoidRouter *a, NetID edgeID, float *coords, size_t maxLen);
EXPORT_C void
avoid_move_node(AvoidRouter *a, ComponentID nodeID, float x, float y);

#endif // AVOID_H