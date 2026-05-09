#version 450 core
#include "../lib/common.glsl"
#include "../lib/lighting.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoords;
layout(location = 3) out vec4 fragPosLightSpace;
layout(location = 4) out vec3 Tangent;
layout(location = 5) out vec3 Bitangent;

layout(set = 0, binding = 0) uniform FrameData {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    uint materialDebugView;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0, _pad1, _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normalMatrix;
} object;

void main()
{
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    FragPos       = worldPos.xyz;
    TexCoords     = inTexcoord;
    fragPosLightSpace = shadowBuf.shadows[frame.dirShadowIdx].matrices[0] * worldPos;

    mat3 nm  = mat3(object.normalMatrix);
    vec3 N   = normalize(nm * inNormal);
    vec3 T   = nm * inTangent.xyz;
    if (dot(T, T) > 0.000001) {
        T = normalize(T);
        T = T - dot(T, N) * N;
        if (dot(T, T) > 0.000001) {
            T = normalize(T);
            vec3 B = normalize(cross(N, T)) * inTangent.w;
            Tangent = T;
            Bitangent = B;
        } else {
            Tangent = vec3(0.0);
            Bitangent = vec3(0.0);
        }
    } else {
        Tangent = vec3(0.0);
        Bitangent = vec3(0.0);
    }

    Normal    = N;

    gl_Position = frame.camera.proj * frame.camera.view * worldPos;
}
