#version 450
#include "../lib/common.glsl"
#include "../lib/pbr_lighting.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    uint materialDebugView;
    vec4 shadowBias;
} frame;

layout(set = 0, binding = 1) uniform ShadowBuffer {
    uint count;
    float _pad0;
    float _pad1;
    float _pad2;
    ShadowData shadows[MAX_LIGHTS];
} shadowBuf;

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

// GBuffer inputs (written by mrt.frag: location 0=position, 1=normal, 2=albedo)
layout(set = 1, binding = 0) uniform texture2D albedoMap;
layout(set = 1, binding = 1) uniform texture2D positionMap;
layout(set = 1, binding = 2) uniform texture2D normalMap;
layout(set = 1, binding = 3) uniform texture2D materialMap;
layout(set = 1, binding = 4) uniform texture2D emissiveMap;

layout(set = 2, binding = 0) uniform texture2DArray shadowMap;
layout(set = 2, binding = 1) uniform textureCube PointShadowMap;

#include "../lib/shadows.glsl"

#define DEBUG_VIEW_LIT 0u
#define DEBUG_VIEW_ALBEDO 1u
#define DEBUG_VIEW_NORMAL 2u
#define DEBUG_VIEW_ROUGHNESS 3u
#define DEBUG_VIEW_METALLIC 4u
#define DEBUG_VIEW_AO 5u
#define DEBUG_VIEW_EMISSIVE 6u
#define DEBUG_VIEW_SHADOW 7u
#define DEBUG_VIEW_LIGHT_COUNT 8u
#define DEBUG_VIEW_DEPTH 9u
#define DEBUG_VIEW_DIR_SHADOW_MAP 10u
#define DEBUG_VIEW_LIGHT_SPACE_COORDS 11u
#define DEBUG_VIEW_CASCADE_BOUNDARIES 12u

void main()
{
    vec3 fragPos = texture(sampler2D(positionMap, texSampler), inUV).xyz;
    vec3 normal  = normalize(texture(sampler2D(normalMap,   texSampler), inUV).xyz);
    vec3 albedo  = texture(sampler2D(albedoMap,   texSampler), inUV).rgb;
    vec3 material = texture(sampler2D(materialMap, texSampler), inUV).rgb;
    vec3 emissive = texture(sampler2D(emissiveMap, texSampler), inUV).rgb;
    float roughness = material.r;
    float metallic = material.g;
    float ambientOcclusion = material.b;

    vec3 viewDir = normalize(frame.camera.cameraPos - fragPos);

    if (frame.materialDebugView == DEBUG_VIEW_ALBEDO) {
        outColor = vec4(albedo, 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_NORMAL) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_ROUGHNESS) {
        outColor = vec4(vec3(roughness), 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_METALLIC) {
        outColor = vec4(vec3(metallic), 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_AO) {
        outColor = vec4(vec3(ambientOcclusion), 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_EMISSIVE) {
        outColor = vec4(emissive, 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_LIGHT_COUNT) {
        outColor = vec4(vec3(min(float(lightData.lightCount) / float(MAX_LIGHTS), 1.0)), 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_DEPTH) {
        vec4 clip = frame.camera.proj * frame.camera.view * vec4(fragPos, 1.0);
        float depth = clip.z / clip.w;
        outColor = vec4(vec3(clamp(depth, 0.0, 1.0)), 1.0);
        return;
    }

    vec3 color = 0.05 * albedo * ambientOcclusion; // ambient
    float minShadow = 1.0;
    ShadowData directionalShadow = shadowBuf.shadows[frame.dirShadowIdx];
    bool cascadedShadow = directional_shadow_is_cascaded(directionalShadow);
    uint cascadeIndex = cascadedShadow
        ? directional_shadow_cascade_index(directionalShadow, frame.camera.view, fragPos)
        : 0u;
    vec4 fragPosLightSpace = directionalShadow.matrices[cascadeIndex] * vec4(fragPos, 1.0);
    vec3 shadowProj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    shadowProj.xy = shadowProj.xy * 0.5 + 0.5;
    if (frame.materialDebugView == DEBUG_VIEW_DIR_SHADOW_MAP) {
        if (any(lessThan(shadowProj.xy, vec2(0.0))) || any(greaterThan(shadowProj.xy, vec2(1.0)))) {
            outColor = vec4(1.0, 0.0, 1.0, 1.0);
        } else {
            outColor = vec4(vec3(texture(sampler2DArray(shadowMap, texSampler), vec3(shadowProj.xy, float(cascadeIndex))).r), 1.0);
        }
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_LIGHT_SPACE_COORDS) {
        outColor = vec4(clamp(shadowProj, 0.0, 1.0), 1.0);
        return;
    }
    if (frame.materialDebugView == DEBUG_VIEW_CASCADE_BOUNDARIES) {
        outColor = vec4(directional_shadow_cascade_debug_color(directionalShadow, frame.camera.view, fragPos, cascadeIndex, cascadedShadow), 1.0);
        return;
    }

    for (uint i = 0u; i < lightData.lightCount; i++) {
        float shadow = 1.0;
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL) {
            vec3 ld = normalize(-vec3(lightData.lights[i].direction));
            shadow = shadow_factor(fragPosLightSpace, cascadeIndex, normal, ld, frame.shadowBias.x, frame.shadowBias.y);
        } else if (lightData.lights[i].type == LIGHT_POINT) {
            shadow = sample_point_shadow(
                fragPos,
                lightData.lights[i].position.xyz,
                lightData.lights[i].position.w,
                normal,
                frame.shadowBias.z,
                frame.shadowBias.w
            );
        }
        // Spot shadows are not generated by ShadowSystem yet, so spots are lit unshadowed.
        minShadow = min(minShadow, shadow);

        if (lightData.lights[i].type == LIGHT_DIRECTIONAL)
            color += shadow * CalcDirLight(lightData.lights[i], albedo, metallic, roughness, normal, viewDir);
        else if (lightData.lights[i].type == LIGHT_POINT)
            color += shadow * CalcPointLight(lightData.lights[i], albedo, metallic, roughness, normal, fragPos, viewDir);
        else if (lightData.lights[i].type == LIGHT_SPOT)
            color += shadow * CalcSpotLight(lightData.lights[i], albedo, metallic, roughness, normal, fragPos, viewDir);
    }

    if (frame.materialDebugView == DEBUG_VIEW_SHADOW) {
        outColor = vec4(vec3(minShadow), 1.0);
        return;
    }

    outColor = vec4(color + emissive, 1.0);
}
