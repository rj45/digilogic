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

static void
save_id(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, ID id) {
  char idStr[128];
  int len = snprintf(
    idStr, sizeof(idStr), "%x:%x:%x", id_flags(id), id_gen(id), id_index(id));

  yyjson_mut_obj_add_strncpy(doc, obj, key, idStr, len);
}

static void save_id_arr(yyjson_mut_doc *doc, yyjson_mut_val *arr, ID id) {
  char idStr[128];
  int len = snprintf(
    idStr, sizeof(idStr), "%x:%x:%x", id_flags(id), id_gen(id), id_index(id));

  yyjson_mut_arr_add_strncpy(doc, arr, idStr, len);
}

static void save_vec2(
  yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, HMM_Vec2 pos) {
  yyjson_mut_val *boxNode = yyjson_mut_obj_add_arr(doc, obj, key);

  yyjson_mut_arr_add_real(doc, boxNode, pos.X);
  yyjson_mut_arr_add_real(doc, boxNode, pos.Y);
}

static void save_symbol(
  yyjson_mut_doc *doc, yyjson_mut_val *components, Circuit *circ, ID symbolID) {
  yyjson_mut_val *symbolNode = yyjson_mut_arr_add_obj(doc, components);

  save_id(doc, symbolNode, "id", symbolID);

  ID symbolKindID = circ_get(circ, symbolID, SymbolKindID);

  if (circ_has(circ, circ_get(circ, symbolKindID, ModuleID))) {
    save_id(doc, symbolNode, "symbolKindID", symbolKindID);
  } else {
    const char *typeName =
      circ_str_get(circ, circ_get(circ, symbolKindID, Name));
    yyjson_mut_obj_add_str(doc, symbolNode, "symbolKindName", typeName);
  }

  save_vec2(doc, symbolNode, "position", circ_get(circ, symbolID, Position));

  yyjson_mut_obj_add_int(
    doc, symbolNode, "number", circ_get(circ, symbolID, Number));
}

static void save_endpoint(
  yyjson_mut_doc *doc, yyjson_mut_val *endpoints, Circuit *circ,
  ID endpointID) {
  yyjson_mut_val *endpointNode = yyjson_mut_arr_add_obj(doc, endpoints);
  save_id(doc, endpointNode, "id", endpointID);
  save_vec2(
    doc, endpointNode, "position", circ_get(circ, endpointID, Position));
  yyjson_mut_val *portrefNode =
    yyjson_mut_obj_add_obj(doc, endpointNode, "portref");
  PortRef portref = circ_get(circ, endpointID, PortRef);
  ModuleID moduleID = NO_ID;
  if (circ_has(circ, portref.port)) {
    SymbolKindID symbolKindID = circ_get(circ, portref.symbol, SymbolKindID);
    moduleID = circ_get(circ, symbolKindID, ModuleID);
  }

  if (circ_has(circ, moduleID) || !circ_has(circ, portref.port)) {
    save_id(doc, portrefNode, "port", portref.port);
  } else {
    const char *portName =
      circ_str_get(circ, circ_get(circ, portref.port, Name));
    yyjson_mut_obj_add_str(doc, portrefNode, "portName", portName);
  }
  save_id(doc, portrefNode, "symbol", portref.symbol);

  yyjson_mut_val *waypoints =
    yyjson_mut_obj_add_arr(doc, endpointNode, "waypoints");
  LinkedListIter it = circ_lliter(circ, endpointID);
  while (circ_lliter_next(&it)) {
    ID waypointID = it.current;
    yyjson_mut_val *waypointNode = yyjson_mut_arr_add_obj(doc, waypoints);
    save_id(doc, waypointNode, "id", waypointID);
    save_vec2(
      doc, waypointNode, "position", circ_get(circ, waypointID, Position));
  }
}

static void save_subnet(
  yyjson_mut_doc *doc, yyjson_mut_val *subnets, Circuit *circ, ID subnetID) {
  yyjson_mut_val *subnetNode = yyjson_mut_arr_add_obj(doc, subnets);

  save_id(doc, subnetNode, "id", subnetID);

  const char *name = circ_str_get(circ, circ_get(circ, subnetID, Name));
  yyjson_mut_obj_add_str(doc, subnetNode, "name", name);

  SubnetBitsID subnetBitsID = circ_get(circ, subnetID, SubnetBitsID);
  yyjson_mut_val *subnetBitsNode =
    yyjson_mut_obj_add_arr(doc, subnetNode, "subnetBits");
  save_id(doc, subnetBitsNode, "id", subnetBitsID);
  yyjson_mut_val *bitsNode =
    yyjson_mut_obj_add_arr(doc, subnetBitsNode, "bits");

  if (circ_has(circ, subnetBitsID)) {
    LinkedListIter it = circ_lliter(circ, subnetBitsID);
    while (circ_lliter_next(&it)) {
      yyjson_mut_arr_add_int(doc, bitsNode, circ_get(circ, it.current, Number));
    }
  }

  yyjson_mut_val *endpoints =
    yyjson_mut_obj_add_arr(doc, subnetNode, "endpoints");

  LinkedListIter it = circ_lliter(circ, subnetID);
  while (circ_lliter_next(&it)) {
    save_endpoint(doc, endpoints, circ, it.current);
  }
}

static void
save_net(yyjson_mut_doc *doc, yyjson_mut_val *nets, Circuit *circ, ID netID) {
  yyjson_mut_val *netNode = yyjson_mut_arr_add_obj(doc, nets);

  save_id(doc, netNode, "id", netID);

  const char *name = circ_str_get(circ, circ_get(circ, netID, Name));
  yyjson_mut_obj_add_str(doc, netNode, "name", name);

  yyjson_mut_val *subnets = yyjson_mut_obj_add_arr(doc, netNode, "subnets");
  LinkedListIter it = circ_lliter(circ, netID);
  while (circ_lliter_next(&it)) {
    save_subnet(doc, subnets, circ, it.current);
  }
}

static void save_module(
  yyjson_mut_doc *doc, yyjson_mut_val *modules, Circuit *circ, ID moduleID) {
  yyjson_mut_val *module = yyjson_mut_arr_add_obj(doc, modules);

  save_id(doc, module, "id", moduleID);
  ID symbolKindID = circ_get(circ, moduleID, SymbolKindID);
  save_id(doc, module, "symbolKind", symbolKindID);

  const char *name = circ_str_get(circ, circ_get(circ, symbolKindID, Name));
  yyjson_mut_obj_add_str(doc, module, "name", name);

  const char *prefix = circ_str_get(circ, circ_get(circ, symbolKindID, Prefix));
  yyjson_mut_obj_add_str(doc, module, "prefix", prefix);

  yyjson_mut_val *symbols = yyjson_mut_obj_add_arr(doc, module, "symbols");
  LinkedListIter it = circ_lliter(circ, moduleID);
  while (circ_lliter_next(&it)) {
    save_symbol(doc, symbols, circ, it.current);
  }

  // TODO: save the shape once it matters

  // TODO: save the ports once they matter

  yyjson_mut_val *nets = yyjson_mut_obj_add_arr(doc, module, "nets");
  NetlistID netlist = circ_get(circ, moduleID, NetlistID);
  it = circ_lliter(circ, netlist);
  while (circ_lliter_next(&it)) {
    save_net(doc, nets, circ, it.current);
  }
}

yyjson_mut_val *circ_serialize(yyjson_mut_doc *doc, Circuit *circ) {
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  yyjson_mut_obj_add_int(doc, root, "version", SAVE_VERSION);
  yyjson_mut_val *modules = yyjson_mut_obj_add_arr(doc, root, "modules");

  assert(circ_entity_type(Module) == TYPE_MODULE);

  CircuitIter it = circ_iter(circ, Module);
  while (circ_iter_next(&it)) {
    Module *module = circ_iter_table(&it, Module);
    for (size_t i = 0; i < module->length; i++) {
      save_module(doc, modules, circ, module->id[i]);
    }
  }

  return root;
}

bool circ_save_file(Circuit *circ, const char *filename) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

  yyjson_mut_val *root = circ_serialize(doc, circ);
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
