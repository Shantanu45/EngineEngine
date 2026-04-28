#version 450 core
#include "../lib/common.glsl"

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
    float _pad1;
    float _pad2;
    float _pad3;
    mat4 lightSpaceMatrix; 
} frame;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normalMatrix;
} object;

void main()
{
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    FragPos       = worldPos.xyz;
    TexCoords     = inTexcoord;
    fragPosLightSpace = frame.lightSpaceMatrix * worldPos;

    mat3 nm  = mat3(object.normalMatrix);
    vec3 N   = normalize(nm * inNormal);
    vec3 T   = normalize(nm * inTangent.xyz);
    T        = normalize(T - dot(T, N) * N);          // re-orthogonalize
    vec3 B = normalize(cross(N, T)) * inTangent.w;            // w encodes handedness

    Normal    = N;
    Tangent   = T;
    Bitangent = B;

    gl_Position = frame.camera.proj * frame.camera.view * worldPos;
}