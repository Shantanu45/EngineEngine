#version 450

//layout(binding = 0) uniform texture2D u_Texture;
//layout(binding = 1) uniform sampler u_Sampler;

layout(location = 0) in vec3 fragCol;

layout(location = 0) out vec4 outCol;

void main() {
    outCol = vec4(fragCol, 1.0);
}