#version 450
#include "../lib/common.glsl"

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

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;

void main()
{
    vec4 worldPos4 = object.model * vec4(inPosition, 1.0);

    gl_Position = frame.camera.proj * frame.camera.view * worldPos4;

    outUV = inTexcoord;

    // Vertex position in world space
    outWorldPos = vec3(object.model * inPosition);

    // Normal in world space
    mat3 mNormal = transpose(inverse(mat3(object.model)));
    outNormal = mNormal * normalize(inNormal);
    outTangent = mNormal * normalize(inTangent);

}