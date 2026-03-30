#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragCol;
layout(location = 1) out vec2 fragUV;

layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view_projectoin;
} ubo;

void main() {
    gl_Position = ubo.view_projectoin * ubo.model * vec4(inPosition, 1.0);
    fragUV = inTexcoord;
    fragCol = inNormal; // use normal for debug color
}
