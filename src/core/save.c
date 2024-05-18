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

#include <stdint.h>

#include "core/core.h"
#include "yyjson.h"

#define CURRENT_VERSION 1

static void
save_id(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, ID id) {
  char idStr[128];
  int len = snprintf(
    idStr, sizeof(idStr), "%x:%x:%x", id_type(id), id_gen(id), id_index(id));

  yyjson_mut_obj_add_strncpy(doc, obj, key, idStr, len);
}

static void
save_box(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, Box box) {
  yyjson_mut_val *boxNode = yyjson_mut_obj_add_arr(doc, obj, key);

  yyjson_mut_arr_add_real(doc, boxNode, box.center.X);
  yyjson_mut_arr_add_real(doc, boxNode, box.center.Y);
  yyjson_mut_arr_add_real(doc, boxNode, box.halfSize.Width);
  yyjson_mut_arr_add_real(doc, boxNode, box.halfSize.Height);
}

static void save_vec2(
  yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, HMM_Vec2 pos) {
  yyjson_mut_val *boxNode = yyjson_mut_obj_add_arr(doc, obj, key);

  yyjson_mut_arr_add_real(doc, boxNode, pos.X);
  yyjson_mut_arr_add_real(doc, boxNode, pos.Y);
}

static void save_label(
  yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, Circuit *circuit,
  LabelID labelID) {
  Label *label = circuit_label_ptr(circuit, labelID);

  yyjson_mut_val *labelNode = yyjson_mut_obj_add_obj(doc, obj, key);

  save_id(doc, labelNode, "id", labelID);
  save_box(doc, labelNode, "box", label->box);
  yyjson_mut_obj_add_str(
    doc, labelNode, "text", circuit_label_text(circuit, labelID));
}

static void save_port(
  yyjson_mut_doc *doc, yyjson_mut_val *ports, Circuit *circuit, PortID portID) {
  Port *port = circuit_port_ptr(circuit, portID);

  yyjson_mut_val *portNode = yyjson_mut_arr_add_obj(doc, ports);

  save_id(doc, portNode, "id", portID);
  save_vec2(doc, portNode, "position", port->position);
  save_label(doc, portNode, "label", circuit, port->label);
}

static void save_component(
  yyjson_mut_doc *doc, yyjson_mut_val *components, Circuit *circuit, size_t i) {
  Component *component = &circuit->components[i];
  yyjson_mut_val *componentNode = yyjson_mut_arr_add_obj(doc, components);

  save_id(doc, componentNode, "id", circuit_component_id(circuit, i));

  const ComponentDesc *desc = &circuit->componentDescs[component->desc];
  yyjson_mut_obj_add_str(doc, componentNode, "type", desc->typeName);

  save_box(doc, componentNode, "box", component->box);

  save_label(doc, componentNode, "typeLabel", circuit, component->typeLabel);
  save_label(doc, componentNode, "nameLabel", circuit, component->nameLabel);

  yyjson_mut_val *ports = yyjson_mut_obj_add_arr(doc, componentNode, "ports");

  PortID portID = component->portFirst;
  while (portID != NO_PORT) {
    Port *port = circuit_port_ptr(circuit, portID);

    save_port(doc, ports, circuit, portID);

    portID = port->compNext;
  }
}

static void save_net(
  yyjson_mut_doc *doc, yyjson_mut_val *nets, Circuit *circuit, size_t i) {
  Net *net = &circuit->nets[i];
  yyjson_mut_val *netNode = yyjson_mut_arr_add_obj(doc, nets);

  save_id(doc, netNode, "id", circuit_net_id(circuit, i));

  yyjson_mut_val *endpoints = yyjson_mut_obj_add_arr(doc, netNode, "endpoints");
  EndpointID endpointID = net->endpointFirst;
  while (endpointID != NO_ENDPOINT) {
    Endpoint *endpoint = circuit_endpoint_ptr(circuit, endpointID);

    yyjson_mut_val *endpointNode = yyjson_mut_arr_add_obj(doc, endpoints);
    save_id(doc, endpointNode, "id", endpointID);
    save_vec2(doc, endpointNode, "position", endpoint->position);
    save_id(doc, endpointNode, "port", endpoint->port);

    endpointID = endpoint->next;
  }

  yyjson_mut_val *waypoints = yyjson_mut_obj_add_arr(doc, netNode, "waypoints");
  WaypointID waypointID = net->waypointFirst;
  while (waypointID != NO_ENDPOINT) {
    Waypoint *waypoint = circuit_waypoint_ptr(circuit, waypointID);

    yyjson_mut_val *waypointNode = yyjson_mut_arr_add_obj(doc, waypoints);
    save_id(doc, waypointNode, "id", waypointID);
    save_vec2(doc, waypointNode, "position", waypoint->position);

    waypointID = waypoint->next;
  }
}

yyjson_mut_val *circuit_serialize(yyjson_mut_doc *doc, Circuit *circuit) {
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  yyjson_mut_obj_add_int(doc, root, "version", CURRENT_VERSION);

  yyjson_mut_val *components = yyjson_mut_obj_add_arr(doc, root, "components");
  for (size_t i = 0; i < circuit_component_len(circuit); i++) {
    save_component(doc, components, circuit, i);
  }

  yyjson_mut_val *nets = yyjson_mut_obj_add_arr(doc, root, "nets");
  for (size_t i = 0; i < circuit_net_len(circuit); i++) {
    save_net(doc, nets, circuit, i);
  }

  return root;
}

bool circuit_save_file(Circuit *circuit, const char *filename) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

  yyjson_mut_val *root = circuit_serialize(doc, circuit);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_write_err err;

  yyjson_write_flag flags =
    YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END;

  if (!yyjson_mut_write_file(filename, doc, flags, NULL, &err)) {
    fprintf(stderr, "Failed to write JSON file: %s\n", err.msg);
    yyjson_mut_doc_free(doc);
    return false;
  }

  yyjson_mut_doc_free(doc);

  return true;
}
