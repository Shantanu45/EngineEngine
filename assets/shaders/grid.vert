#version 450
#include "lib/common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
} frame;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normalMatrix;
} object;

layout(location = 0) out vec3 out_cameraPos;
layout(location = 1) out vec3 out_worldPos;

void main()
{
    vec4 worldPos4 = object.model * vec4(inPosition, 1.0);
    out_worldPos   = worldPos4.xyz;
    out_cameraPos  = frame.camera.cameraPos;

    gl_Position = frame.camera.proj * frame.camera.view * worldPos4;
}