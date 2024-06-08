// This file was generated by gen.c from structdescs.h -- DO NOT EDIT

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

#include "core/structs.h"

const StructDesc helperDescs[3] = {
  (StructDesc){
    .name = "HMM_Vec2",
    .size = 8,
    .numFields = 2,
    .offsets = (size_t[]){
      0, 4,
    },
    .sizes = (size_t[]){
      4, 4,
    },
    .names = (char const *[]){
      "X",
      "Y",
    },
    .types = (char const *[]){
      "float",
      "float",
    },
  },
  (StructDesc){
    .name = "Box",
    .size = 16,
    .numFields = 2,
    .offsets = (size_t[]){
      0, 8,
    },
    .sizes = (size_t[]){
      8, 8,
    },
    .names = (char const *[]){
      "center",
      "halfSize",
    },
    .types = (char const *[]){
      "HMM_Vec2",
      "HMM_Vec2",
    },
  },
  (StructDesc){
    .name = "Wire",
    .size = 2,
    .numFields = 1,
    .offsets = (size_t[]){
      0,
    },
    .sizes = (size_t[]){
      2,
    },
    .names = (char const *[]){
      "vertexCount",
    },
    .types = (char const *[]){
      "uint16_t",
    },
  },
};

const StructDesc structDescs[7] = {
  (StructDesc){
    .name = "None",
    .size = 4,
    .numFields = 1,
    .offsets = (size_t[]){
      0,
    },
    .sizes = (size_t[]){
      4,
    },
    .names = (char const *[]){
      "nothing",
    },
    .types = (char const *[]){
      "uint32_t",
    },
  },
  (StructDesc){
    .name = "Component",
    .size = 36,
    .numFields = 6,
    .offsets = (size_t[]){
      0, 16, 20, 24, 28, 32,
    },
    .sizes = (size_t[]){
      16, 4, 4, 4, 4, 4,
    },
    .names = (char const *[]){
      "box",
      "desc",
      "portFirst",
      "portLast",
      "typeLabel",
      "nameLabel",
    },
    .types = (char const *[]){
      "Box",
      "ComponentDescID",
      "PortID",
      "PortID",
      "LabelID",
      "LabelID",
    },
  },
  (StructDesc){
    .name = "Port",
    .size = 36,
    .numFields = 8,
    .offsets = (size_t[]){
      0, 8, 12, 16, 20, 24, 28, 32,
    },
    .sizes = (size_t[]){
      8, 4, 4, 4, 4, 4, 4, 4,
    },
    .names = (char const *[]){
      "position",
      "component",
      "desc",
      "label",
      "next",
      "prev",
      "net",
      "endpoint",
    },
    .types = (char const *[]){
      "HMM_Vec2",
      "ComponentID",
      "PortDescID",
      "LabelID",
      "PortID",
      "PortID",
      "NetID",
      "EndpointID",
    },
  },
  (StructDesc){
    .name = "Net",
    .size = 32,
    .numFields = 8,
    .offsets = (size_t[]){
      0, 4, 8, 12, 16, 20, 24, 28,
    },
    .sizes = (size_t[]){
      4, 4, 4, 4, 4, 4, 4, 4,
    },
    .names = (char const *[]){
      "endpointFirst",
      "endpointLast",
      "waypointFirst",
      "waypointLast",
      "label",
      "wireOffset",
      "wireCount",
      "vertexOffset",
    },
    .types = (char const *[]){
      "EndpointID",
      "EndpointID",
      "WaypointID",
      "WaypointID",
      "LabelID",
      "WireIndex",
      "uint32_t",
      "VertexIndex",
    },
  },
  (StructDesc){
    .name = "Endpoint",
    .size = 24,
    .numFields = 5,
    .offsets = (size_t[]){
      0, 8, 12, 16, 20,
    },
    .sizes = (size_t[]){
      8, 4, 4, 4, 4,
    },
    .names = (char const *[]){
      "position",
      "net",
      "port",
      "next",
      "prev",
    },
    .types = (char const *[]){
      "HMM_Vec2",
      "NetID",
      "PortID",
      "EndpointID",
      "EndpointID",
    },
  },
  (StructDesc){
    .name = "Waypoint",
    .size = 20,
    .numFields = 4,
    .offsets = (size_t[]){
      0, 8, 12, 16,
    },
    .sizes = (size_t[]){
      8, 4, 4, 4,
    },
    .names = (char const *[]){
      "position",
      "net",
      "next",
      "prev",
    },
    .types = (char const *[]){
      "HMM_Vec2",
      "NetID",
      "WaypointID",
      "WaypointID",
    },
  },
  (StructDesc){
    .name = "Label",
    .size = 20,
    .numFields = 2,
    .offsets = (size_t[]){
      0, 16,
    },
    .sizes = (size_t[]){
      16, 4,
    },
    .names = (char const *[]){
      "box",
      "textOffset",
    },
    .types = (char const *[]){
      "Box",
      "uint32_t",
    },
  },
};
