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

#include "autoroute/autoroute.h"
#include "ux.h"

void ux_route(CircuitUX *ux) {
  autoroute_route(ux->router, ux->betterRoutes);

  float coords[1024];

  for (int i = 0; i < circuit_wire_len(&ux->view.circuit); i++) {
    WireView *wireView = &ux->view.wires[i];
    WireID id = circuit_wire_id(&ux->view.circuit, i);

    size_t len = autoroute_wire_vertices(
      ux->router, id, coords, sizeof(coords) / sizeof(coords[0]));
    len /= 2;

    int curSize = wireView->vertexEnd - wireView->vertexStart;

    while (curSize < len) {
      view_add_vertex(&ux->view, id, HMM_V2(0, 0));
      curSize++;
    }
    while (curSize > len) {
      view_rem_vertex(&ux->view, id);
      curSize--;
    }
    for (int j = 0; j < len; j++) {
      view_set_vertex(
        &ux->view, id, j, HMM_V2(coords[j * 2], coords[j * 2 + 1]));
    }
    view_fix_wire_end_vertices(&ux->view, id);
  }

  for (int i = 0; i < circuit_junction_len(&ux->view.circuit); i++) {
    JunctionView *junctionView = &ux->view.junctions[i];
    JunctionID id = circuit_junction_id(&ux->view.circuit, i);
    autoroute_get_junction_pos(
      ux->router, id, &junctionView->pos.X, &junctionView->pos.Y);
  }
}
