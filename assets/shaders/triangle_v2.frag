#version 450

layout(binding = 1) uniform texture2D u_Texture;
layout(binding = 2) uniform sampler u_Sampler;

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outCol;

void main() {
    //outCol = vec4(fragCol, 1.0);
    outCol = texture(sampler2D(u_Texture, u_Sampler), fragUV);
}