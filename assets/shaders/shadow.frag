#version 450 core

#include "lib/lighting.glsl"

layout(location = 0) in vec2 TexCoords;

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
}
