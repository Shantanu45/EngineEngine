#version 450 core
#include "lib/common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoords;

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
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    FragPos       = worldPos.xyz;
    Normal        = mat3(object.normalMatrix) * inNormal;
    TexCoords     = inTexcoord;

    gl_Position = frame.camera.proj * frame.camera.view * worldPos;
}