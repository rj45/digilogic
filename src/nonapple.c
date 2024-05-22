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

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_SOFTWARE_FONT
#define NK_IMPLEMENTATION
#define MSDF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STRPOOL_IMPLEMENTATION
#define ASSETSYS_IMPLEMENTATION

#define SOKOL_IMPL

#ifdef SOKOL_LINUX_CUSTOM
#include "sokol_app_wayland.h"
#else
#include "sokol_app.h"
#endif

#include "assetsys.h"
#include "nuklear.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gp.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "stb_image.h"
#include "strpool.h"

#include "render/sokol_nuklear.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "file_compat.h"

static char locale[64];
static char resourcePath[PATH_MAX];
static char dataPath[PATH_MAX];
static char cachePath[PATH_MAX];
static char autosavePath[PATH_MAX];

void platform_init() {
  fc_locale(locale, 64);
  fc_resdir(resourcePath, PATH_MAX);
  if (resourcePath[strlen(resourcePath) - 1] != FC_DIRECTORY_SEPARATOR) {
    int len = strlen(resourcePath);
    resourcePath[len] = FC_DIRECTORY_SEPARATOR;
    resourcePath[len + 1] = '\0';
  }
  fc_datadir("digilogic", dataPath, PATH_MAX);
  if (dataPath[strlen(dataPath) - 1] != FC_DIRECTORY_SEPARATOR) {
    int len = strlen(dataPath);
    dataPath[len] = FC_DIRECTORY_SEPARATOR;
    dataPath[len + 1] = '\0';
  }
  fc_cachedir("digilogic", cachePath, PATH_MAX);
  if (cachePath[strlen(cachePath) - 1] != FC_DIRECTORY_SEPARATOR) {
    int len = strlen(cachePath);
    cachePath[len] = FC_DIRECTORY_SEPARATOR;
    cachePath[len + 1] = '\0';
  }
  snprintf(autosavePath, PATH_MAX, "%sautosave.dlc", dataPath);
}

const char *platform_locale() { return locale; }
const char *platform_resource_path() { return resourcePath; }
const char *platform_data_path() { return dataPath; }
const char *platform_cache_path() { return cachePath; }
const char *platform_autosave_path() { return autosavePath; }