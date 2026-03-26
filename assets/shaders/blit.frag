#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outCol;

layout(binding = 0) uniform sampler2D src_rt;

void main()
{
    outCol = vec4(1.0, 1.0, 0.0, 1.0);//texture(src_rt, uv);
}