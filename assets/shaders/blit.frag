#version 450

//layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

//layout(binding = 0) uniform sampler2D src_rt;

void main()
{
    color = vec4(1.0, 1.0, 0.0, 1.0);//texture(src_rt, uv);
}
