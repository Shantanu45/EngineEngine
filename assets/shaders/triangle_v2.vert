#version 450

layout(location = 0) in vec2 inPos;

layout(location = 0) out vec3 fragCol;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
    fragCol = vec3(1.0, 1.0, 0.0);
}