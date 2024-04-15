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

@ctype vec2 HMM_vec2
@ctype float float
@ctype vec4 HMM_Vec4

@vs vs
uniform msdfUniform {
    float scale;
    vec4 fgColor;
    vec4 bgColor;
};
layout(location=0) in vec4 coord;
layout(location=0) out vec2 texUV;
layout(location=1) out vec2 fragPos;
void main() {
    gl_Position = vec4(coord.xy, 0.0, 1.0);
    fragPos = vec2(coord.x * scale, coord.y*scale);
    texUV = coord.zw;
}
@end

@fs fs
uniform texture2D atlas;
uniform sampler samp;
uniform msdfUniform {
    float scale;
    vec4 fgColor;
    vec4 bgColor;
};
layout(location=0) in vec2 texUV;
layout(location=1) in vec2 fragPos;
layout(location=0) out vec4 fragColor;

#line 33

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    //vec2 flippedUV = vec2(texUV.x, 1.0 - texUV.y);
    vec3 msd = texture(sampler2D(atlas, samp), texUV).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float w = fwidth(sd);
    //float opacity = clamp(w + 0.5, 0.0, 1.0);
    float opacity = smoothstep(0.5 - w, 0.5 + w, sd);
    fragColor = mix(bgColor, fgColor, opacity);
}
@end

@program msdf vs fs
