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

typedef struct IDLookup {
  char *key;
  ID value;
} IDLookup;

static bool
circuit_deserialize(Circuit *circuit, yyjson_val *root, IDLookup **ids) {

  return true;
}

bool circuit_save_file(Circuit *circuit, const char *filename) {
  yyjson_read_err err;

  yyjson_read_flag flags =
    YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;

  yyjson_doc *doc = yyjson_read_file(filename, flags, NULL, &err);
  if (doc == NULL) {
    fprintf(stderr, "Failed to read circuit file: %s\n", err.msg);
    return false;
  }

  yyjson_val *root = yyjson_doc_get_root(doc);

  IDLookup *ids = NULL;

  int version = yyjson_get_int(yyjson_obj_get(root, "version"));

  bool result = false;

  switch (version) {
  case 0:
    fprintf(stderr, "Failed to read circuit: File missing version\n");
    result = false;
    break;
  case SAVE_VERSION:
    result = circuit_deserialize(circuit, root, &ids);
    break;
  default:
    fprintf(stderr, "Failed to read circuit: Unknown version %d\n", version);
    result = false;
    break;
  }

  yyjson_doc_free(doc);

  shfree(ids);

  return result;
}
