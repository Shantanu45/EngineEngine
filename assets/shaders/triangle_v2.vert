#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UBO {
    vec3 value;
} ubo;

layout(location = 0) out vec3 fragCol;

void main() {
    gl_Position = vec4(inPos, 1.0);
    fragCol = inColor;
}