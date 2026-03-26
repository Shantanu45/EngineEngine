#version 450

//layout(location = 0) out vec2 uv;
vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

void main()
{
    //uv = (pos[gl_VertexIndex] + 1.0) * 0.5;
    //gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}