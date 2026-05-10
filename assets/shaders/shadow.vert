#version 450 core

#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform FrameData {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    uint materialDebugView;
    float _pad0;
    vec4 shadowBias;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0, _pad1, _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

layout(push_constant) uniform PC {
    mat4 model;
    mat4 normalMatrix;
} pc;

layout(location = 0) out vec2 TexCoords;

void main() {
    TexCoords = inTexcoord;
    gl_Position = shadowBuf.shadows[frame.dirShadowIdx].matrices[0] * pc.model * vec4(inPosition, 1.0);
}
