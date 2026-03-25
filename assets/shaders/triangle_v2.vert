#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(binding = 0) uniform UBO {
    vec3 value;
} ubo;

layout(location = 0) out vec3 fragCol;
layout(location = 1) out vec2 fragUV;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragUV = inTexcoord;
    fragCol = inPosition;
}