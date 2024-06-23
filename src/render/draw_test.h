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

#ifndef DRAW_TEST_H
#define DRAW_TEST_H

#include "draw.h"

DrawContext *draw_create();
void draw_reset(DrawContext *draw);
void draw_free(DrawContext *draw);
char *draw_get_build_string(DrawContext *draw);
void draw_do_nothing(DrawContext *draw);

#endif // DRAW_TEST_H