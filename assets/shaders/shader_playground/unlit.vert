#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 uv;
layout(location = 3) out vec4 tangent;

layout(set = 0, binding = 0) uniform PlaygroundFrame {
    vec2 iResolution;
    float iTime;
    float iDeltaTime;
    mat4 view;
    mat4 proj;
    vec4 iMouse;
} frame;

layout(push_constant) uniform Object {
    mat4 model;
    mat4 normalMatrix;
} object;

void main()
{
    vec4 wp = object.model * vec4(inPosition, 1.0);
    worldPos = wp.xyz;
    normal = normalize(mat3(object.normalMatrix) * inNormal);
    tangent = vec4(normalize(mat3(object.normalMatrix) * inTangent.xyz), inTangent.w);
    uv = inTexcoord;
    gl_Position = frame.proj * frame.view * wp;
}
