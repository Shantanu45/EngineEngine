#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexcoord;

layout(location = 0) out vec2 fragUV;

void main()
{
    fragUV = inTexcoord;
    // make_quad() verts are in [-0.5, 0.5]; scale to NDC [-1, 1]
    gl_Position = vec4(inPosition.xy * 2.0, 0.0, 1.0);
}
