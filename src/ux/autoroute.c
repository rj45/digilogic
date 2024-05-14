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
#include "stb_ds.h"
#include "view/view.h"

#include "autoroute/autoroute.h"
#include "ux.h"

void ux_route(CircuitUX *ux) {
  autoroute_route(ux->router, ux->betterRoutes);
  // Circuit *circuit = &ux->view.circuit;
  // for (int i = 0; i < circuit_net_len(circuit); i++) {
  //   Net *net = &circuit->nets[i];

  //   int portIndex = 0;
  //   PortID portID = net->portFirst;
  //   while (portID != NO_PORT) {
  //     Port *port = circuit_port_ptr(circuit, portID);

  //     WireID wireID = port->wire;
  //     if (port->wire == NO_WIRE) {
  //       wireID =
  //         circuit_add_wire(circuit, circuit_net_id(circuit, i), portID,
  //         NO_ID);
  //     }
  //     WireView *wireView = view_wire_ptr(&ux->view, wireID);
  //     HMM_Vec2 *vertices = NULL;
  //     size_t length =
  //       autoroute_get_net_port_vertices(ux->router, i, portIndex, &vertices);
  //     while ((wireView->vertexEnd - wireView->vertexStart) > length) {
  //       view_rem_vertex(&ux->view, wireID);
  //     }
  //     while ((wireView->vertexEnd - wireView->vertexStart) < length) {
  //       view_add_vertex(&ux->view, wireID, HMM_V2(0, 0));
  //     }
  //     memcpy(
  //       ux->view.vertices + wireView->vertexStart, vertices,
  //       length * sizeof(HMM_Vec2));

  //     portID = port->netNext;
  //     portIndex++;
  //   }
  // }
}
