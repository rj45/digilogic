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

#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"

int main(int argc, char **argv) {
  char *buffer = 0;
  FILE *fp = fopen("res/noto_sans_regular.json", "rt");
  if (fp == NULL) {
    fprintf(stderr, "File not found\n");
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  long length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  buffer = malloc(length + 1);
  if (fread(buffer, 1, length, fp) != length) {
    fclose(fp);
    free(buffer);
    fprintf(stderr, "Error reading file\n");
    return 1;
  }
  fclose(fp);

  cJSON *json = cJSON_ParseWithLength(buffer, length);

  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    free(buffer);
    return 1;
  }
  free(buffer);

  if (!cJSON_IsObject(json)) {
    fprintf(stderr, "Expected root to be an object\n");
    cJSON_free(json);
    return 1;
  }

  cJSON *atlas = cJSON_GetObjectItem(json, "atlas");
  if (!cJSON_IsObject(atlas)) {
    fprintf(stderr, "Expected atlas to be an object\n");
    cJSON_free(json);
    return 1;
  }

  printf("// clang-format off\n");
  printf("#include \"font.h\"\n\n");

  printf(
    "// Auto-generated from res/noto_sans_regular.json -- DO NOT EDIT\n\n");
  printf("const Font notoSansRegular = {\n");
  printf(
    "  .distanceRange = %ff,\n",
    cJSON_GetObjectItem(atlas, "distanceRange")->valuedouble);
  printf("  .size = %ff,\n", cJSON_GetObjectItem(atlas, "size")->valuedouble);
  printf("  .width = %d,\n", cJSON_GetObjectItem(atlas, "width")->valueint);
  printf("  .height = %d,\n", cJSON_GetObjectItem(atlas, "height")->valueint);

  cJSON *metrics = cJSON_GetObjectItem(json, "metrics");
  if (!cJSON_IsObject(metrics)) {
    fprintf(stderr, "Expected metrics to be an object\n");
    cJSON_free(json);
    return 1;
  }

  printf(
    "  .emSize = %ff,\n", cJSON_GetObjectItem(metrics, "emSize")->valuedouble);
  printf(
    "  .lineHeight = %ff,\n",
    cJSON_GetObjectItem(metrics, "lineHeight")->valuedouble);
  printf(
    "  .ascender = %ff,\n",
    cJSON_GetObjectItem(metrics, "ascender")->valuedouble);
  printf(
    "  .descender = %ff,\n",
    cJSON_GetObjectItem(metrics, "descender")->valuedouble);
  printf(
    "  .underlineY = %ff,\n",
    cJSON_GetObjectItem(metrics, "underlineY")->valuedouble);
  printf(
    "  .underlineThickness = %ff,\n",
    cJSON_GetObjectItem(metrics, "underlineThickness")->valuedouble);

  cJSON *glyphs = cJSON_GetObjectItem(json, "glyphs");
  if (!cJSON_IsArray(glyphs)) {
    fprintf(stderr, "Expected glyphs to be an array\n");
    cJSON_free(json);
    return 1;
  }

  int numGlyphs = cJSON_GetArraySize(glyphs);
  printf("  .numGlyphs = %d,\n", numGlyphs);

  printf("  .glyphs = (const FontGlyph[]){\n");
  for (int i = 0; i < numGlyphs; i++) {
    cJSON *glyph = cJSON_GetArrayItem(glyphs, i);
    printf("    {\n");
    printf(
      "      .unicode = %d,\n",
      cJSON_GetObjectItem(glyph, "unicode")->valueint);
    printf(
      "      .advance = %ff,\n",
      cJSON_GetObjectItem(glyph, "advance")->valuedouble);
    cJSON *planeBounds = cJSON_GetObjectItem(glyph, "planeBounds");
    if (cJSON_IsObject(planeBounds)) {
      float left = cJSON_GetObjectItem(planeBounds, "left")->valuedouble;
      float top = cJSON_GetObjectItem(planeBounds, "top")->valuedouble;
      printf(
        "      .planeBounds = { .x = %ff, .y = %ff, .width = %ff, .height = "
        "%ff },\n",
        left, top,
        cJSON_GetObjectItem(planeBounds, "right")->valuedouble - left,
        cJSON_GetObjectItem(planeBounds, "bottom")->valuedouble - top);
    }
    cJSON *atlasBounds = cJSON_GetObjectItem(glyph, "atlasBounds");
    if (cJSON_IsObject(atlasBounds)) {
      float left = cJSON_GetObjectItem(atlasBounds, "left")->valuedouble;
      float top = cJSON_GetObjectItem(atlasBounds, "top")->valuedouble;
      printf(
        "      .atlasBounds = { .x = %ff, .y = %ff, .width = %ff, .height = "
        "%ff },\n",
        left, top,
        cJSON_GetObjectItem(atlasBounds, "right")->valuedouble - left,
        cJSON_GetObjectItem(atlasBounds, "bottom")->valuedouble - top);
    }
    printf("    },\n");
  }
  printf("  },\n");

  fp = fopen("res/noto_sans_regular.png", "rb");
  if (fp == NULL) {
    fprintf(stderr, "File not found\n");
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  buffer = malloc(length + 1);
  if (fread(buffer, 1, length, fp) != length) {
    fclose(fp);
    free(buffer);
    fprintf(stderr, "Error reading file\n");
    return 1;
  }
  fclose(fp);

  printf("  .pngSize = %ld,\n", length);
  printf("  .png = {\n    ");
  for (int i = 0; i < length; i++) {
    printf("0x%02x, ", buffer[i] & 0xff);
    if (i % 16 == 15) {
      printf("\n    ");
    }
  }
  printf("  },\n");
  printf("};\n");
  free(buffer);

  cJSON_free(json);
  return 0;
}