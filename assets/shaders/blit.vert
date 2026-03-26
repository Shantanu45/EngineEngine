#version 450

layout(location = 0) out vec2 uv;

void main()
{
vec2 pos[4] = vec2[](
    vec2(0.0, 0.0),  // 0
    vec2(0.0, 1.0),  // 1
    vec2(1.0, 1.0),  // 2
    vec2(1.0, 0.0)   // 3
);
    uint fake_index = gl_VertexIndex % 4;
    uv = pos[fake_index];
    gl_Position = vec4(pos[gl_VertexIndex] * 2.0 - 1.0, 0.0, 1.0);
}