#version 450
#extension GL_ARB_shader_viewport_layer_array : require

#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;  // 6 faces × 3 vertices

layout(set = 0, binding = 0) uniform FrameData {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    float _pad;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0, _pad1, _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

layout(location = 0) in  vec4 InFragPos[];
layout(location = 1) in  vec2 InTexCoords[];
layout(location = 0) out vec4 FragPos;
layout(location = 1) out vec2 TexCoords;

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int v = 0; v < 3; ++v) {
            FragPos = InFragPos[v];
            TexCoords = InTexCoords[v];
            gl_Position = shadowBuf.shadows[frame.ptShadowIdx].matrices[face] * InFragPos[v];
            EmitVertex();
        }
        EndPrimitive();
    }
}
