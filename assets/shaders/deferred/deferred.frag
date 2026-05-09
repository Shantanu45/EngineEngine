#version 450
#include "../lib/common.glsl"
#include "../lib/lighting.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
    uint dirShadowIdx;
    uint ptShadowIdx;
    uint materialDebugView;
} frame;

layout(set = 0, binding = 2) uniform LightBuffer {
    uint  lightCount;
    float _pad;
    float _pad1;
    float _pad2;
    Light lights[MAX_LIGHTS];
} lightData;

layout(set = 0, binding = 3) uniform sampler texSampler;

// GBuffer inputs (written by mrt.frag: location 0=position, 1=normal, 2=albedo)
layout(set = 1, binding = 0) uniform texture2D albedoMap;
layout(set = 1, binding = 1) uniform texture2D positionMap;
layout(set = 1, binding = 2) uniform texture2D normalMap;
layout(set = 1, binding = 3) uniform texture2D materialMap;
layout(set = 1, binding = 4) uniform texture2D emissiveMap;

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
    if (frame.materialDebugView == DEBUG_VIEW_SHADOW) {
        outColor = vec4(vec3(1.0), 1.0);
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

    for (uint i = 0u; i < lightData.lightCount; i++) {
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL)
            color += CalcDirLight(lightData.lights[i], albedo, albedo, normal, viewDir, 32.0);
        else if (lightData.lights[i].type == LIGHT_POINT)
            color += CalcPointLight(lightData.lights[i], albedo, albedo, normal, fragPos, viewDir, 32.0);
        else if (lightData.lights[i].type == LIGHT_SPOT)
            color += CalcSpotLight(lightData.lights[i], albedo, albedo, normal, fragPos, viewDir, 32.0);
    }

    outColor = vec4(color + emissive, 1.0);
}
