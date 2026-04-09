#version 450
#extension GL_ARB_shader_viewport_layer_array : require

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;  // 6 faces × 3 vertices

layout(set = 0, binding = 1) uniform PointShadowUBO {
    mat4  shadowMatrices[6];
    vec4  lightPos;
    float farPlane;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(location = 0) in  vec4 InFragPos[];
layout(location = 0) out vec4 FragPos;

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;  // route to this cubemap face
        for (int v = 0; v < 3; ++v) {
            FragPos = InFragPos[v];
            gl_Position = ubo.shadowMatrices[face] * InFragPos[v];
            EmitVertex();
        }
        EndPrimitive();
    }
}