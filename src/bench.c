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
#include "ux/ux.h"
#include "view/view.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define SOKOL_IMPL
#include "sokol_time.h"

#include "ubench.h"

#ifndef _WIN32
#include "stacktrace.h"
#endif

#define STRPOOL_IMPLEMENTATION
#include "strpool.h"

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

#include "ubench.h"

#include "import/import.h"
#include "ux/ux.h"

#include "render/draw.h"
#include "render/draw_test.h"

struct Benchy {
  CircuitUX ux;
  DrawContext *draw;
};

UBENCH_F_SETUP(Benchy) {
  ubench_fixture->draw = draw_create();
  draw_do_nothing(ubench_fixture->draw);
  ux_init(
    &ubench_fixture->ux, circuit_component_descs(), ubench_fixture->draw, NULL);

  FILE *fp = fopen("res/assets/testdata/alu_1bit_2inpgate.dig", "rt");
  assert(fp);

  fseek(fp, 0, SEEK_END);
  int fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *buffer = malloc(fileSize + 1);
  fread(buffer, 1, fileSize, fp);
  buffer[fileSize] = 0;

  import_digital(&ubench_fixture->ux.view.circuit, buffer);
  ux_route(&ubench_fixture->ux);

  free(buffer);
}

UBENCH_F_TEARDOWN(Benchy) { ux_free(&ubench_fixture->ux); }

UBENCH_F(Benchy, draw_code) { view_draw(&ubench_fixture->ux.view); }

UBENCH_F(Benchy, routing) { ux_route(&ubench_fixture->ux); }

UBENCH_STATE();
int main(int argc, const char *const argv[]) {
#ifndef _WIN32
  init_exceptions((char *)argv[0]);
#endif
  ux_global_init();
  stm_setup();
  return ubench_main(argc, argv);
}