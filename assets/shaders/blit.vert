#version 450

layout(location = 0) out vec2 uv;

void main()
{
	vec2 base_arr[4] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0));
    uv = base_arr[gl_VertexIndex];
    gl_Position =  vec4(base_arr[gl_VertexIndex] * 2.0 - 1.0, 0.0, 1.0);
}