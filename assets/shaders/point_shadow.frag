#version 450

#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(location = 0) in vec4 FragPos;
layout(location = 1) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform FrameData {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    uint materialDebugView;
    float _pad0;
    vec4 shadowBias;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0, _pad1, _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

layout(set = 0, binding = 3) uniform sampler texSampler;

layout(set = 2, binding = 0) uniform MaterialUBO {
    Material material;
} mat;

layout(set = 2, binding = 1) uniform texture2D diffuse_tex;
layout(set = 2, binding = 2) uniform texture2D metallic_roughness;
layout(set = 2, binding = 3) uniform texture2D normal_tex;
layout(set = 2, binding = 4) uniform texture2D displacement_tex;
layout(set = 2, binding = 5) uniform texture2D emissive_tex;
layout(set = 2, binding = 6) uniform texture2D occlusion_tex;

#define ALPHA_MODE_MASK 1u

void main() {
    vec4 baseColor = texture(sampler2D(diffuse_tex, texSampler), TexCoords)
                     * mat.material.base_color_factor;
    if (mat.material.alpha_mode == ALPHA_MODE_MASK &&
        baseColor.a < mat.material.alpha_cutoff)
        discard;

    vec3  lightPos = shadowBuf.shadows[frame.ptShadowIdx].light_pos.xyz;
    float farPlane = shadowBuf.shadows[frame.ptShadowIdx].light_pos.w;
    gl_FragDepth = length(FragPos.xyz - lightPos) / farPlane;
}
