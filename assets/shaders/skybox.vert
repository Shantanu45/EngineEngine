// skybox.vert
#version 450 core
#include "lib/common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 TexCoords;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
} frame;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normalMatrix;
} object;

void main()
{
    TexCoords = inPosition;

    // strip translation — skybox stays centered on camera
    mat4 view_no_translation = mat4(mat3(frame.camera.view));
    vec4 pos = frame.camera.proj * view_no_translation * vec4(inPosition, 1.0);

    // z = w forces depth to 1.0 — always behind everything
    gl_Position = pos.xyww;
}