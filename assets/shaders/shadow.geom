#version 450
#extension GL_ARB_shader_viewport_layer_array : require

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(push_constant) uniform PC {
    mat4 model;
    uint cascadeIndex;
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout(location = 0) in vec2 InTexCoords[];
layout(location = 0) out vec2 TexCoords;

void main() {
    gl_Layer = int(pc.cascadeIndex);

    for (int i = 0; i < 3; ++i) {
        TexCoords = InTexCoords[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }

    EndPrimitive();
}
