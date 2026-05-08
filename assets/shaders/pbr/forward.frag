#version 450 core
#include "../lib/common.glsl"
#include "../lib/pbr_lighting.glsl"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;
layout(location = 3) in vec4 fragPosLightSpace;
layout(location = 4) in vec3 Tangent;
layout(location = 5) in vec3 Bitangent;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    float _pad;
} frame;

layout(set = 1, binding = 0) uniform texture2D shadowMap;
layout(set = 1, binding = 1) uniform textureCube PointShadowMap;

layout(set = 2, binding = 0) uniform MaterialUBO {
    Material material;
} mat;

layout(set = 0, binding = 2) uniform LightBuffer {
    uint  lightCount;
    float _pad;
    float _pad1;
    float _pad2;
    Light lights[MAX_LIGHTS];
} lightData;

layout(set = 0, binding = 3) uniform sampler texSampler;
layout(set = 0, binding = 4) uniform sampler pcfSampler;
layout(set = 0, binding = 5) uniform sampler pointShadowSampler;

layout(set = 2, binding = 1) uniform texture2D diffuse_tex;
layout(set = 2, binding = 2) uniform texture2D metallic_roughness;
layout(set = 2, binding = 3) uniform texture2D normal_tex;
layout(set = 2, binding = 4) uniform texture2D displacement_tex;
layout(set = 2, binding = 5) uniform texture2D emissive_tex;
layout(set = 2, binding = 6) uniform texture2D occlusion_tex;

#include "../lib/shadows.glsl"

#define ALPHA_MODE_MASK 1u

vec2 ParallaxMapping(vec2 texCoords, vec3 tangentViewDir) {
    const float heightScale = 0.05;
    const int   maxLayers   = 32;

    float numLayers  = mix(float(maxLayers), 8.0, abs(tangentViewDir.z));
    float layerDepth = 1.0 / numLayers;

    vec2 deltaUV = (tangentViewDir.xy / max(tangentViewDir.z, 0.01)) * heightScale / numLayers;

    vec2  uv           = texCoords;
    float currentDepth = 0.0;
    float surfaceDepth = 1.0 - texture(sampler2D(displacement_tex, texSampler), uv).r;

    for (int i = 0; i < maxLayers; i++) {
        if (currentDepth >= surfaceDepth) break;
        uv           -= deltaUV;
        surfaceDepth  = 1.0 - texture(sampler2D(displacement_tex, texSampler), uv).r;
        currentDepth += layerDepth;
    }

    vec2  prevUV      = uv + deltaUV;
    float afterDepth  = surfaceDepth - currentDepth;
    float beforeDepth = (1.0 - texture(sampler2D(displacement_tex, texSampler), prevUV).r)
                        - currentDepth + layerDepth;
    float weight      = afterDepth / (afterDepth - beforeDepth);
    return mix(uv, prevUV, weight);
}

void main()
{
    vec3 viewDir = normalize(frame.camera.cameraPos - FragPos);
    vec3 normal = normalize(Normal);
    bool hasTangentBasis = dot(Tangent, Tangent) > 0.000001 &&
                           dot(Bitangent, Bitangent) > 0.000001;

    vec2 uv = TexCoords;
    if (hasTangentBasis) {
        mat3 TBN = mat3(normalize(Tangent), normalize(Bitangent), normal);

        vec3 tangentViewDir = normalize(transpose(TBN) * viewDir);
        uv = ParallaxMapping(TexCoords, tangentViewDir);

        vec3 tangentNormal = texture(sampler2D(normal_tex, texSampler), uv).rgb * 2.0 - 1.0;
        tangentNormal.xy  *= mat.material.emissive_and_normal.w;
        normal = normalize(TBN * tangentNormal);
    }

    vec4 baseColorSample = texture(sampler2D(diffuse_tex, texSampler), uv);
    vec4 baseColor = baseColorSample * mat.material.base_color_factor;
    if (mat.material.alpha_mode == ALPHA_MODE_MASK &&
        baseColor.a < mat.material.alpha_cutoff)
        discard;

    float ao = texture(sampler2D(occlusion_tex, texSampler), uv).r;
    float ambientOcclusion = mix(1.0, ao, mat.material.occlusion_strength);
    vec3 color = baseColor.rgb * 0.05 * ambientOcclusion;

    for (uint i = 0u; i < lightData.lightCount; i++) {
        float shadow = 1.0;
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL) {
            vec3 ld = normalize(-vec3(lightData.lights[i].direction));
            shadow = shadow_factor(fragPosLightSpace, normal, ld);
        } else if (lightData.lights[i].type == LIGHT_POINT) {
            shadow = sample_point_shadow(
                FragPos,
                lightData.lights[i].position.xyz,
                lightData.lights[i].position.w,
                normal
            );
        }
        color += shadow * CalcLight(
            lightData.lights[i], mat.material,
            diffuse_tex, metallic_roughness, texSampler,
            uv, normal, FragPos, viewDir);
    }

    vec3 emissive = mat.material.emissive_and_normal.xyz
                    * vec3(texture(sampler2D(emissive_tex, texSampler), uv));
    FragColor = vec4(color + emissive, 1.0);
}
