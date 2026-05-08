#version 450 core
#include "../lib/common.glsl"
#include "lighting.glsl"

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

float shadow_factor(vec4 fragPosLS, vec3 normal, vec3 lightDir) {
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;

    float cosTheta = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float bias = max(0.002 * (1.0 - cosTheta), 0.0005);

    vec2 texelSize = 1.0 / textureSize(sampler2DShadow(shadowMap, pcfSampler), 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            shadow += texture(sampler2DShadow(shadowMap, pcfSampler),
                              vec3(proj.xy + vec2(x, y) * texelSize, proj.z - bias));
        }
    }
    return shadow / 9.0;
}

float samplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane, vec3 normal) {
    vec3 dir = fragPos - lightPos;
    float currentDepth = length(dir) / farPlane;

    vec3 lightDir = normalize(lightPos - fragPos);
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float bias = mix(0.005, 0.0001, cosTheta);

    vec3 sampleOffsets[4] = vec3[](
        vec3( 1.0,  0.0,  0.0),
        vec3(-1.0,  0.0,  0.0),
        vec3( 0.0,  1.0,  0.0),
        vec3( 0.0, -1.0,  0.0)
    );
    float diskRadius = 0.02;
    float shadow = 0.0;
    for (int i = 0; i < 4; i++) {
        float closestDepth = texture(
            samplerCube(PointShadowMap, pointShadowSampler),
            dir + sampleOffsets[i] * diskRadius
        ).r;
        shadow += (currentDepth - bias) < closestDepth ? 1.0 : 0.0;
    }
    return shadow / 4.0;
}

void main()
{
    mat3 TBN    = mat3(normalize(Tangent), normalize(Bitangent), normalize(Normal));
    vec3 viewDir = normalize(frame.camera.cameraPos - FragPos);

    vec3 tangentViewDir = normalize(transpose(TBN) * viewDir);
    vec2 uv = ParallaxMapping(TexCoords, tangentViewDir);

    vec3 tangentNormal = texture(sampler2D(normal_tex, texSampler), uv).rgb * 2.0 - 1.0;
    tangentNormal.xy  *= mat.material.emissive_and_normal.w;
    vec3 normal = normalize(TBN * tangentNormal);

    vec3 baseColor = vec3(texture(sampler2D(diffuse_tex, texSampler), uv))
                     * mat.material.base_color_factor.rgb;
    float ao = texture(sampler2D(occlusion_tex, texSampler), uv).r;
    float ambientOcclusion = mix(1.0, ao, mat.material.occlusion_strength);
    vec3 color = baseColor * 0.05 * ambientOcclusion;

    for (uint i = 0u; i < lightData.lightCount; i++) {
        float shadow = 1.0;
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL) {
            vec3 ld = normalize(-vec3(lightData.lights[i].direction));
            shadow = shadow_factor(fragPosLightSpace, normal, ld);
        } else if (lightData.lights[i].type == LIGHT_POINT) {
            shadow = samplePointShadow(
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
