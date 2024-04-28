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

#include "stb_ds.h"
#include "view/view.h"

#include "ux.h"

#include "avoid/avoid.h"

void ux_route(CircuitUX *ux) {
  avoid_route(ux->avoid);

  float coords[1024];

  for (int i = 0; i < arrlen(ux->view.wires); i++) {
    WireView *wireView = &ux->view.wires[i];

    size_t len = avoid_get_edge_path(
      ux->avoid, i, coords, sizeof(coords) / sizeof(coords[0]));
    len /= 2;

    int curSize = wireView->vertexEnd - wireView->vertexStart;

    while (curSize < len) {
      view_add_vertex(&ux->view, i, HMM_V2(0, 0));
      curSize++;
    }
    while (curSize > len) {
      view_rem_vertex(&ux->view, i);
      curSize--;
    }
    for (int j = 0; j < len; j++) {
      view_set_vertex(
        &ux->view, i, j, HMM_V2(coords[j * 2], coords[j * 2 + 1]));
    }
    view_fix_wire_end_vertices(&ux->view, i);
  }

  for (int i = 0; i < arrlen(ux->view.junctions); i++) {
    JunctionView *junctionView = &ux->view.junctions[i];
    avoid_get_junction_pos(
      ux->avoid, i, &junctionView->pos.X, &junctionView->pos.Y);
  }
}
