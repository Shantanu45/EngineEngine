#version 450

#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(location = 0) in vec4 FragPos;

layout(set = 0, binding = 0) uniform FrameData {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    float _pad;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0, _pad1, _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

void main() {
    vec3  lightPos = shadowBuf.shadows[frame.ptShadowIdx].light_pos.xyz;
    float farPlane = shadowBuf.shadows[frame.ptShadowIdx].light_pos.w;
    gl_FragDepth = length(FragPos.xyz - lightPos) / farPlane;
}
