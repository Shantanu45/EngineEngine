#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D src_rt;
layout(binding = 1) uniform sampler2D ui_rt;

void main()
{
    vec4 ui = texture(ui_rt, uv);
    vec4 scene = texture(src_rt, uv);
    color = ui + scene * (1.0 - ui.a) ;
}