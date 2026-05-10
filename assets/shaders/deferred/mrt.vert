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
layout (location = 5) out vec3 outBitangent;

void main()
{
    vec4 worldPos4 = object.model * vec4(inPosition, 1.0);

    gl_Position = frame.camera.proj * frame.camera.view * worldPos4;

    outUV = inTexcoord;

    // Vertex position in world space
    outWorldPos = worldPos4.xyz;

    // Normal in world space
    mat3 mNormal = mat3(object.normalMatrix);
    vec3 N = normalize(mNormal * inNormal);
    vec3 T = mNormal * inTangent.xyz;
    if (dot(T, T) > 0.000001) {
        T = normalize(T);
        T = T - dot(T, N) * N;
        if (dot(T, T) > 0.000001) {
            T = normalize(T);
            outTangent = T;
            outBitangent = normalize(cross(N, T)) * inTangent.w;
        } else {
            outTangent = vec3(0.0);
            outBitangent = vec3(0.0);
        }
    } else {
        outTangent = vec3(0.0);
        outBitangent = vec3(0.0);
    }
    outNormal = N;

}
