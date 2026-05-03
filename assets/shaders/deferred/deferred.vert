#version 450
#include "../lib/common.glsl"

layout(location = 0) out vec2 outUV;

void main()
{
    // Fullscreen triangle from vertex index — no vertex buffer needed.
    // Index 0: uv(0,0) pos(-1,-1)   Index 1: uv(2,0) pos(3,-1)   Index 2: uv(0,2) pos(-1,3)
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV.x * 2.0 - 1.0, 1.0 - outUV.y * 2.0, 0.0, 1.0);
}
