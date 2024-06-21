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

#define LOG_LEVEL LL_DEBUG
#include "log.h"

typedef struct IDLookup {
  char *key;
  ID value;
} IDLookup;

typedef struct LoadContext {
  Circuit *circ;
  yyjson_doc *doc;
  yyjson_val *root;
  IDLookup *ids;
  int version;
} LoadContext;

static const char *load_string(yyjson_val *val) {
  if (val == NULL) {
    return NULL;
  }

  const char *str = yyjson_get_str(val);
  if (str == NULL) {
    return NULL;
  }
  return str;
}

static bool load_id(LoadContext *ctc, yyjson_val *val, ID id) {
  if (val == NULL) {
    return false;
  }

  const char *idStr = load_string(val);
  if (idStr == NULL) {
    return false;
  }
  shput(ctc->ids, idStr, id);

  return true;
}

static bool
load_module_symbol_kind(LoadContext *ctc, yyjson_val *moduleVal, ID moduleID) {
  Circuit *circ = ctc->circ;

  if (!load_id(ctc, yyjson_obj_get(moduleVal, "id"), moduleID)) {
    fprintf(stderr, "Failed to read circuit: Module missing id\n");
    return false;
  }

  ID symbolKindID = circ_get(circ, moduleID, SymbolKindID);
  if (!load_id(ctc, yyjson_obj_get(moduleVal, "symbolKind"), symbolKindID)) {
    fprintf(stderr, "Failed to read circuit: Module missing symbolKind\n");
    return false;
  }

  const char *name = load_string(yyjson_obj_get(moduleVal, "name"));
  if (name == NULL) {
    fprintf(stderr, "Failed to read circuit: Module missing name\n");
    return false;
  }
  StringHandle nameHndl = circ_str_c(circ, name);
  circ_set(circ, symbolKindID, Name, {nameHndl});

  const char *prefix = load_string(yyjson_obj_get(moduleVal, "prefix"));
  if (prefix == NULL) {
    fprintf(stderr, "Failed to read circuit: Module missing prefix\n");
    return false;
  }
  StringHandle prefixHndl = circ_str_c(circ, prefix);
  circ_set(circ, symbolKindID, Prefix, {prefixHndl});

  // todo: load shape

  // todo: load ports

  return true;
}

static bool
load_position(LoadContext *ctx, yyjson_val *val, HMM_Vec2 *position) {
  if (val == NULL) {
    return false;
  }

  if (yyjson_arr_get(val, 0) == NULL || yyjson_arr_get(val, 1) == NULL) {
    return false;
  }

  position->X = yyjson_get_real(yyjson_arr_get(val, 0));
  position->Y = yyjson_get_real(yyjson_arr_get(val, 1));

  return true;
}

static bool load_int(LoadContext *ctx, yyjson_val *val, int *result) {
  if (val == NULL) {
    return false;
  }

  *result = yyjson_get_int(val);
  return true;
}

static bool load_symbol(LoadContext *ctx, yyjson_val *symbolVal, ID moduleID) {
  Circuit *circ = ctx->circ;

  ID symbolKindID = NO_ID;
  const char *symbolKind =
    load_string(yyjson_obj_get(symbolVal, "symbolKindID"));
  if (symbolKind == NULL) {
    symbolKind = load_string(yyjson_obj_get(symbolVal, "symbolKindName"));
    if (symbolKind == NULL) {
      fprintf(
        stderr, "Failed to read circuit: Symbol missing symbolKindID and "
                "symbolKindName\n");
      return false;
    }

    StringHandle handle = circ_str_tmp_c(circ, symbolKind);

    CircuitIter it = circ_iter(circ, SymbolKind);
    while (symbolKindID == NO_ID && circ_iter_next(&it)) {
      SymbolKind *table = circ_iter_table(&it, SymbolKind);
      for (size_t i = 0; i < table->length; i++) {
        if (table->name[i] == handle) {
          symbolKindID = table->id[i];
          break;
        }
      }
    }
  } else {
    symbolKindID = shget(ctx->ids, symbolKind);
  }

  ID symbolID = circ_add_symbol(circ, moduleID, symbolKindID);
  if (!load_id(ctx, yyjson_obj_get(symbolVal, "id"), symbolID)) {
    fprintf(stderr, "Failed to read circuit: Symbol missing id\n");
    return false;
  }

  HMM_Vec2 position;
  if (!load_position(ctx, yyjson_obj_get(symbolVal, "position"), &position)) {
    fprintf(stderr, "Failed to read circuit: Symbol missing position\n");
    return false;
  }
  circ_set_symbol_position(circ, symbolID, position);

  Number number;
  if (!load_int(ctx, yyjson_obj_get(symbolVal, "number"), &number)) {
    fprintf(stderr, "Failed to read circuit: Symbol missing number\n");
    return false;
  }
  circ_set(circ, symbolID, Number, {number});

  log_debug("Added symbol %x at %f %f", symbolID, position.X, position.Y);
  return true;
}

static bool
load_waypoint(LoadContext *ctx, yyjson_val *waypointVal, ID endpointID) {
  Circuit *circ = ctx->circ;

  ID waypointID = circ_add_waypoint(circ, endpointID);
  if (!load_id(ctx, yyjson_obj_get(waypointVal, "id"), waypointID)) {
    fprintf(stderr, "Failed to read circuit: Waypoint missing id\n");
    return false;
  }

  HMM_Vec2 position;
  if (!load_position(ctx, yyjson_obj_get(waypointVal, "position"), &position)) {
    fprintf(stderr, "Failed to read circuit: Waypoint missing position\n");
    return false;
  }
  circ_set_waypoint_position(circ, waypointID, position);

  log_debug(
    "    * Added waypoint %x at %f %f", waypointID, position.X, position.Y);

  return true;
}

static bool
load_endpoint(LoadContext *ctx, yyjson_val *endpointVal, ID subnetID) {
  Circuit *circ = ctx->circ;

  ID endpointID = circ_add_endpoint(circ, subnetID);
  if (!load_id(ctx, yyjson_obj_get(endpointVal, "id"), endpointID)) {
    fprintf(stderr, "Failed to read circuit: Endpoint missing id\n");
    return false;
  }

  HMM_Vec2 position;
  if (!load_position(ctx, yyjson_obj_get(endpointVal, "position"), &position)) {
    fprintf(stderr, "Failed to read circuit: Endpoint missing position\n");
    return false;
  }
  circ_set_endpoint_position(circ, endpointID, position);

  PortRef portRef = {NO_ID, NO_ID};

  yyjson_val *portRefVal = yyjson_obj_get(endpointVal, "portref");

  const char *symbolIDStr = load_string(yyjson_obj_get(portRefVal, "symbol"));
  if (symbolIDStr == NULL) {
    fprintf(
      stderr, "Failed to read circuit: Endpoint missing portRef.symbol\n");
    return false;
  }

  portRef.symbol = shget(ctx->ids, symbolIDStr);

  const char *port = load_string(yyjson_obj_get(portRefVal, "port"));
  if (port == NULL) {
    port = load_string(yyjson_obj_get(portRefVal, "portName"));
    if (port == NULL) {
      fprintf(
        stderr, "Failed to read circuit: Endpoint missing portRef.port and "
                "portRef.portName\n");
      return false;
    }

    StringHandle handle = circ_str_tmp_c(circ, port);

    SymbolKindID symbolKind = circ_get(circ, portRef.symbol, SymbolKindID);

    LinkedListIter it = circ_lliter(circ, symbolKind);
    while (circ_lliter_next(&it)) {
      Name portName = circ_get(circ, circ_lliter_get(&it), Name);
      if (portName == handle) {
        portRef.port = circ_lliter_get(&it);
        break;
      }
    }
    if (portRef.port == NO_ID) {
      fprintf(stderr, "Failed to read circuit: Invalid portRef.portName\n");
      return false;
    }
  } else {
    portRef.port = shget(ctx->ids, port);
  }

  circ_connect_endpoint_to_port(circ, endpointID, portRef.symbol, portRef.port);

  log_debug(
    "  * Added endpoint %x ref {%x, %x} at %f %f", endpointID, portRef.symbol,
    portRef.port, position.X, position.Y);

  yyjson_val *waypointsVal = yyjson_obj_get(endpointVal, "waypoints");
  if (waypointsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Endpoint missing waypoints\n");
    return false;
  }

  for (size_t waypointIndex = 0; waypointIndex < yyjson_arr_size(waypointsVal);
       waypointIndex++) {
    yyjson_val *waypointVal = yyjson_arr_get(waypointsVal, waypointIndex);
    if (waypointVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Net missing waypoint\n");
      return false;
    }

    if (!load_waypoint(ctx, waypointVal, endpointID)) {
      return false;
    }
  }
  return true;
}

static bool load_subnet(LoadContext *ctx, yyjson_val *subnetVal, ID netID) {
  Circuit *circ = ctx->circ;

  ID subnetID = circ_add_subnet(circ, netID);
  if (!load_id(ctx, yyjson_obj_get(subnetVal, "id"), subnetID)) {
    fprintf(stderr, "Failed to read circuit: Subnet missing id\n");
    return false;
  }

  // todo: load subnet bits

  const char *name = load_string(yyjson_obj_get(subnetVal, "name"));
  if (name == NULL) {
    fprintf(stderr, "Failed to read circuit: Subnet missing name\n");
    return false;
  }
  StringHandle nameHndl = circ_str_c(circ, name);
  circ_set(circ, subnetID, Name, {nameHndl});

  yyjson_val *endpointsVal = yyjson_obj_get(subnetVal, "endpoints");
  if (endpointsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Subnet missing endpoints\n");
    return false;
  }

  log_debug(" Subnet %x", subnetID);

  for (size_t endpointIndex = 0; endpointIndex < yyjson_arr_size(endpointsVal);
       endpointIndex++) {
    yyjson_val *endpointVal = yyjson_arr_get(endpointsVal, endpointIndex);
    if (endpointVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Subnet missing endpoint\n");
      return false;
    }

    if (!load_endpoint(ctx, endpointVal, subnetID)) {
      return false;
    }
  }

  return true;
}

static bool load_net(LoadContext *ctx, yyjson_val *netVal, ID moduleID) {
  Circuit *circ = ctx->circ;

  ID netID = circ_add_net(circ, moduleID);
  if (!load_id(ctx, yyjson_obj_get(netVal, "id"), netID)) {
    fprintf(stderr, "Failed to read circuit: Net missing id\n");
    return false;
  }

  const char *name = load_string(yyjson_obj_get(netVal, "name"));
  if (name == NULL) {
    fprintf(stderr, "Failed to read circuit: Net missing name\n");
    return false;
  }
  StringHandle nameHndl = circ_str_c(circ, name);
  circ_set(circ, netID, Name, {nameHndl});

  yyjson_val *subnetsVal = yyjson_obj_get(netVal, "subnets");
  if (subnetsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Net missing subnets\n");
    return false;
  }

  log_debug("Net %x", netID);

  for (size_t i = 0; i < yyjson_arr_size(subnetsVal); i++) {
    yyjson_val *subnetVal = yyjson_arr_get(subnetsVal, i);

    if (!load_subnet(ctx, subnetVal, netID)) {
      return false;
    }
  }

  return true;
}

static bool load_module(LoadContext *ctx, yyjson_val *moduleVal) {
  ID moduleID = shget(ctx->ids, load_string(yyjson_obj_get(moduleVal, "id")));

  yyjson_val *symbolsVal = yyjson_obj_get(moduleVal, "symbols");
  if (symbolsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Missing symbols\n");
    return false;
  }

  log_debug("Symbols...");

  for (size_t i = 0; i < yyjson_arr_size(symbolsVal); i++) {
    yyjson_val *symbolVal = yyjson_arr_get(symbolsVal, i);

    log_debug("Symbol %zu", i);

    if (!load_symbol(ctx, symbolVal, moduleID)) {
      return false;
    }
  }

  yyjson_val *netsVal = yyjson_obj_get(moduleVal, "nets");
  if (netsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Missing nets\n");
    return false;
  }

  log_debug("Nets...");

  for (size_t i = 0; i < yyjson_arr_size(netsVal); i++) {
    yyjson_val *netVal = yyjson_arr_get(netsVal, i);

    if (!load_net(ctx, netVal, moduleID)) {
      return false;
    }
  }

  return true;
}

static bool circ_deserialize(LoadContext *ctx) {
  yyjson_val *root = ctx->root;
  Circuit *circ = ctx->circ;

  log_debug("Deserializing circuit");

  yyjson_val *modulesVal = yyjson_obj_get(root, "modules");
  if (modulesVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Missing modules\n");
    return false;
  }

  // first load all module SymbolKinds so they can be referenced
  for (size_t i = 0; i < yyjson_arr_size(modulesVal); i++) {
    yyjson_val *moduleVal = yyjson_arr_get(modulesVal, i);
    if (moduleVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Missing module value\n");
      return false;
    }

    ID moduleID = circ->top;
    if (i != 0) {
      moduleID = circ_add_module(circ);
    }
    load_module_symbol_kind(ctx, moduleVal, moduleID);
  }

  // next load the contents of each module
  for (size_t i = 0; i < yyjson_arr_size(modulesVal); i++) {
    yyjson_val *moduleVal = yyjson_arr_get(modulesVal, i);
    if (moduleVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Missing module value\n");
      return false;
    }
    load_module(ctx, moduleVal);
  }

  return true;
}

bool circ_load_file(Circuit *circ, const char *filename) {
  yyjson_read_err err;

  yyjson_read_flag flags =
    YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;

  yyjson_doc *doc = yyjson_read_file(filename, flags, NULL, &err);
  if (doc == NULL) {
    fprintf(stderr, "Failed to read circuit file: %s\n", err.msg);
    return false;
  }

  yyjson_val *root = yyjson_doc_get_root(doc);

  int version = yyjson_get_int(yyjson_obj_get(root, "version"));

  LoadContext ctx = {
    .circ = circ,
    .doc = doc,
    .root = root,
    .ids = NULL,
    .version = version,
  };

  bool result = false;

  switch (version) {
  case 0:
    fprintf(stderr, "Failed to read circuit: File missing version\n");
    result = false;
    break;
  case SAVE_VERSION:
    result = circ_deserialize(&ctx);
    break;
  default:
    fprintf(stderr, "Failed to read circuit: Unknown version %d\n", version);
    result = false;
    break;
  }

  yyjson_doc_free(doc);
  shfree(ctx.ids);

  return result;
}
