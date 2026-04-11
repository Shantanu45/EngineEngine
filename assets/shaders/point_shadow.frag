#version 450

layout(location = 0) in vec4 FragPos;

layout(set = 0, binding = 1) uniform PointShadowUBO {
    mat4  shadowMatrices[6];
    vec4  lightPos;
} ubo;

void main() {
    float dist = length(FragPos.xyz - ubo.lightPos.xyz);
    gl_FragDepth = dist / ubo.lightPos.w;;  // normalize to [0, 1]
}