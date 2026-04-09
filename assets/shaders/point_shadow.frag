#version 450

layout(location = 0) in vec4 FragPos;

layout(set = 0, binding = 1) uniform PointShadowUBO {
    mat4  shadowMatrices[6];
    vec4  lightPos;
    float farPlane;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

void main() {
    float dist = length(FragPos.xyz - ubo.lightPos.xyz);
    gl_FragDepth = dist / ubo.farPlane;  // normalize to [0, 1]
}