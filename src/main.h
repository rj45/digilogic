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

#ifndef MAIN_H
#define MAIN_H

#include "sokol_gfx.h"
#include "ux/ux.h"

typedef struct {
  CircuitUX circuit;

  sg_shader msdf_shader;
  sg_pipeline msdf_pipeline;
  sg_image msdf_tex;
  sg_sampler msdf_sampler;
} my_app_t;

#endif // MAIN_H