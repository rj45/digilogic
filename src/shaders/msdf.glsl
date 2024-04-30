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
layout(location=0) in vec4 coord;
layout(location=1) in vec4 color;
layout(location=0) out vec2 texUV;
layout(location=1) out vec4 iColor;
void main() {
    gl_Position = vec4(coord.xy, 0.0, 1.0);
    texUV = coord.zw;
    iColor = color;
}
@end

@fs fs
uniform texture2D iTexChannel0;
uniform sampler iSmpChannel0;
layout(location=0) in vec2 texUV;
layout(location=1) in vec4 iColor;
layout(location=0) out vec4 fragColor;

#line 31

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
    // 2.0 is the pixel range value
    vec2 unitRange = 2.0 / vec2(textureSize(sampler2D(iTexChannel0, iSmpChannel0), 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(texUV);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(sampler2D(iTexChannel0, iSmpChannel0), texUV).rgb;
    float dist = median(msd.r, msd.g, msd.b);

    float pxDist = screenPxRange() * (dist - 0.5);
    float opacity = clamp(pxDist + 0.5, 0.0, 1.0);

    fragColor = vec4(iColor.rgb, iColor.a * opacity);
}

@end

@program msdf vs fs
