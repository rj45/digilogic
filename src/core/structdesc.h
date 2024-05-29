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

// This file is used to build an X-Macro reflection system similar to:
// https://natecraun.net/articles/struct-iteration-through-abuse-of-the-c-preprocessor.html
// See src/core/structdescs.h for its usage.

#ifndef STRUCT_DESC_DEF
/* One time only definitons */
#define STRUCT_DESC_DEF
typedef struct StructDesc {
  const char *name;
  const char *desc;
  size_t size;
  size_t numFields;
  size_t *offsets;
  size_t *sizes;
  const char **names;
  const char **types;
  const char **descs;
} StructDesc;
#endif

/* Error Checking */

#ifndef STRUCT_NAME
#error "Did not define STRUCT_NAME before including structdesc.h"
#endif

#ifndef STRUCT_FIELDS
#error "Did not define STRUCT_FIELDS before including structdesc.h"
#endif

#ifndef STRUCT_DESC
#error "Did not define STRUCT_DESC before including structdesc.h"
#endif

#define STR_NOEXPAND(A) #A
#define STR(A) STR_NOEXPAND(A)

#define CAT_NOEXPAND(A, B) A##B
#define CAT(A, B) CAT_NOEXPAND(A, B)

typedef struct STRUCT_NAME {
#define X(fieldType, fieldName, fieldDesc) fieldType fieldName;
  STRUCT_FIELDS
#undef X
} STRUCT_NAME;

static const StructDesc CAT(STRUCT_NAME, Desc) = {
  .name = STR(STRUCT_NAME),

  .desc = STRUCT_DESC,

  .size = sizeof(STRUCT_NAME),

  .numFields = (
#define X(fieldType, fieldName, fieldDesc) 1 +
    STRUCT_FIELDS
#undef X
    0),

  .offsets =
    (size_t[]){
#define X(fieldType, fieldName, fieldDesc) offsetof(STRUCT_NAME, fieldName),
      STRUCT_FIELDS
#undef X
    },

  .sizes =
    (size_t[]){
#define X(fieldType, fieldName, fieldDesc) sizeof(fieldType),
      STRUCT_FIELDS
#undef X
    },

  .names =
    (char const *[]){
#define X(fieldType, fieldName, fieldDesc) #fieldName,
      STRUCT_FIELDS
#undef X
    },

  .types =
    (char const *[]){
#define X(fieldType, fieldName, fieldDesc) #fieldType,
      STRUCT_FIELDS
#undef X
    },

  .descs =
    (char const *[]){
#define X(fieldType, fieldName, fieldDesc) fieldDesc,
      STRUCT_FIELDS
#undef X
    },
};

#undef STRUCT_DESC
#undef STRUCT_FIELDS
#undef STRUCT_NAME
#undef STR_NOEXPAND
#undef STR
#undef CAT_NOEXPAND
#undef CAT