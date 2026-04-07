#version 450 core
#include "lib/common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
} frame;

layout(set = 3, binding = 0) uniform ObjectUBO {  // block name changed
    mat4 model;
    mat4 normalMatrix;
} object;

void main()
{
    mat4 mvp = frame.camera.proj * frame.camera.view * object.model;
    gl_Position = mvp * vec4(inPosition, 1.0);
}