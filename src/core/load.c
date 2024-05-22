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
  Circuit *circuit;
  yyjson_doc *doc;
  yyjson_val *root;
  IDLookup *ids;
  int version;
} LoadContext;

static bool circuit_deserialize(LoadContext *ctx) {
  Circuit *circuit = ctx->circuit;
  yyjson_val *root = ctx->root;

  log_debug("Deserializing circuit");

  yyjson_val *componentsVal = yyjson_obj_get(root, "components");
  if (componentsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Missing components\n");
    return false;
  }

  log_debug("Components...");

  for (size_t i = 0; i < yyjson_arr_size(componentsVal); i++) {
    yyjson_val *componentVal = yyjson_arr_get(componentsVal, i);

    log_debug("Component %zu", i);

    yyjson_val *idVal = yyjson_obj_get(componentVal, "id");
    if (idVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Component missing id\n");
      return false;
    }

    log_debug("ID: %s", yyjson_get_str(idVal));

    yyjson_val *typeVal = yyjson_obj_get(componentVal, "type");
    if (typeVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Component missing type\n");
      return false;
    }

    log_debug("type: %s", yyjson_get_str(typeVal));

    const char *idStr = yyjson_get_str(idVal);
    const char *type = yyjson_get_str(typeVal);

    if (idStr == NULL) {
      fprintf(stderr, "Failed to read circuit: Component missing id\n");
      return false;
    }

    if (type == NULL) {
      fprintf(stderr, "Failed to read circuit: Component missing type\n");
      return false;
    }

    HMM_Vec2 position;
    yyjson_val *positionVal = yyjson_obj_get(componentVal, "position");
    if (positionVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Component missing position\n");
      return false;
    }
    if (
      yyjson_arr_get(positionVal, 0) == NULL ||
      yyjson_arr_get(positionVal, 1) == NULL) {
      fprintf(
        stderr,
        "Failed to read circuit: Component missing position coordinate\n");
      return false;
    }
    position.X = yyjson_get_real(yyjson_arr_get(positionVal, 0));
    position.Y = yyjson_get_real(yyjson_arr_get(positionVal, 1));
    const ComponentDesc *desc;
    ComponentDescID descID = COMP_COUNT;

    log_debug("position: %f %f", position.X, position.Y);

    for (ComponentDescID j = 0; j < COMP_COUNT; j++) {
      desc = &circuit->componentDescs[j];
      if (!desc->typeName) {
        continue;
      }
      log_debug("Comparing %s to %s", type, desc->typeName);
      if (strncmp(type, desc->typeName, strlen(type)) == 0) {
        descID = j;
        break;
      }
    }
    if (descID >= COMP_COUNT) {
      fprintf(
        stderr, "Failed to read circuit: Unknown component type %s\n", type);
      return false;
    }

    log_debug("Adding component %s at %f %f", type, position.X, position.Y);

    ComponentID componentID = circuit_add_component(circuit, descID, position);
    shput(ctx->ids, idStr, componentID);

    Component *component = circuit_component_ptr(circuit, componentID);

    yyjson_val *portsVal = yyjson_obj_get(componentVal, "ports");

    int portIndex = 0;
    PortID portID = component->portFirst;
    while (circuit_has(circuit, portID)) {
      Port *port = circuit_port_ptr(circuit, portID);
      yyjson_val *portIDVal = yyjson_arr_get(portsVal, portIndex);
      if (portIDVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Component missing port ID\n");
        return false;
      }

      const char *portIdstr = yyjson_get_str(portIDVal);
      if (portIdstr == NULL) {
        fprintf(stderr, "Failed to read circuit: Port missing id\n");
        return false;
      }

      shput(ctx->ids, portIdstr, portID);

      portID = port->next;
      portIndex++;
    }
  }

  yyjson_val *netsVal = yyjson_obj_get(root, "nets");
  if (netsVal == NULL) {
    fprintf(stderr, "Failed to read circuit: Missing nets\n");
    return false;
  }

  log_debug("Nets...");

  for (size_t i = 0; i < yyjson_arr_size(netsVal); i++) {
    yyjson_val *netVal = yyjson_arr_get(netsVal, i);

    yyjson_val *idVal = yyjson_obj_get(netVal, "id");
    if (idVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Net missing id\n");
      return false;
    }

    log_debug("ID: %s", yyjson_get_str(idVal));

    const char *idStr = yyjson_get_str(idVal);
    if (idStr == NULL) {
      fprintf(stderr, "Failed to read circuit: Net missing id\n");
      return false;
    }

    NetID netID = circuit_add_net(circuit);
    shput(ctx->ids, idStr, netID);

    yyjson_val *endpointsVal = yyjson_obj_get(netVal, "endpoints");
    if (endpointsVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Net missing endpoints\n");
      return false;
    }

    for (size_t endpointIndex = 0;
         endpointIndex < yyjson_arr_size(endpointsVal); endpointIndex++) {
      yyjson_val *endpointVal = yyjson_arr_get(endpointsVal, endpointIndex);
      if (endpointVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Net missing endpoint\n");
        return false;
      }

      yyjson_val *endpointIDVal = yyjson_obj_get(endpointVal, "id");
      if (endpointIDVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing id\n");
        return false;
      }

      const char *endpointIDStr = yyjson_get_str(endpointIDVal);
      if (endpointIDStr == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing id\n");
        return false;
      }

      PortID portID = NO_PORT;

      yyjson_val *portIDVal = yyjson_obj_get(endpointVal, "port");
      if (portIDVal != NULL) {
        // port is optional
        const char *portIDStr = yyjson_get_str(portIDVal);
        if (portIDStr != NULL) {
          portID = shget(ctx->ids, portIDStr);
        }
      }

      HMM_Vec2 position;
      yyjson_val *positionVal = yyjson_obj_get(endpointVal, "position");
      if (positionVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing position\n");
        return false;
      }

      if (
        yyjson_arr_get(positionVal, 0) == NULL ||
        yyjson_arr_get(positionVal, 1) == NULL) {
        fprintf(
          stderr,
          "Failed to read circuit: Endpoint missing position coordinate\n");
        return false;
      }

      position.X = yyjson_get_real(yyjson_arr_get(positionVal, 0));
      position.Y = yyjson_get_real(yyjson_arr_get(positionVal, 1));

      log_debug(
        "Adding endpoint %s at %f %f to net %d", endpointIDStr, position.X,
        position.Y, netID);

      EndpointID endpointID =
        circuit_add_endpoint(circuit, netID, portID, position);
      shput(ctx->ids, endpointIDStr, endpointID);
    }

    log_debug("Waypoints...");

    yyjson_val *waypointsVal = yyjson_obj_get(netVal, "waypoints");
    if (waypointsVal == NULL) {
      fprintf(stderr, "Failed to read circuit: Net missing waypoints\n");
      return false;
    }

    for (size_t waypointIndex = 0;
         waypointIndex < yyjson_arr_size(waypointsVal); waypointIndex++) {
      yyjson_val *waypointVal = yyjson_arr_get(waypointsVal, waypointIndex);
      if (waypointVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Net missing waypoint\n");
        return false;
      }

      yyjson_val *waypointIDVal = yyjson_obj_get(waypointVal, "id");
      if (waypointIDVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing id\n");
        return false;
      }

      const char *waypointIDStr = yyjson_get_str(waypointIDVal);
      if (waypointIDStr == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing id\n");
        return false;
      }

      HMM_Vec2 position;
      yyjson_val *positionVal = yyjson_obj_get(waypointVal, "position");
      if (positionVal == NULL) {
        fprintf(stderr, "Failed to read circuit: Endpoint missing position\n");
        return false;
      }

      if (
        yyjson_arr_get(positionVal, 0) == NULL ||
        yyjson_arr_get(positionVal, 1) == NULL) {
        fprintf(
          stderr,
          "Failed to read circuit: Endpoint missing position coordinate\n");
        return false;
      }

      position.X = yyjson_get_real(yyjson_arr_get(positionVal, 0));
      position.Y = yyjson_get_real(yyjson_arr_get(positionVal, 1));

      log_debug(
        "Adding waypoint %s at %f %f to net %d", waypointIDStr, position.X,
        position.Y, netID);

      EndpointID waypointID = circuit_add_waypoint(circuit, netID, position);
      shput(ctx->ids, waypointIDStr, waypointID);
    }
  }

  return true;
}

bool circuit_load_file(Circuit *circuit, const char *filename) {
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
    .circuit = circuit,
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
    result = circuit_deserialize(&ctx);
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
