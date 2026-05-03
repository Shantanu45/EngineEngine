#version 450
#include "../lib/common.glsl"
#include "../lib/lighting.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
    float _pad1;
    float _pad2;
    float _pad3;
    mat4 lightSpaceMatrix;
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

void main()
{
    vec3 fragPos = texture(sampler2D(positionMap, texSampler), inUV).xyz;
    vec3 normal  = normalize(texture(sampler2D(normalMap,   texSampler), inUV).xyz);
    vec3 albedo  = texture(sampler2D(albedoMap,   texSampler), inUV).rgb;

    vec3 viewDir = normalize(frame.camera.cameraPos - fragPos);

    vec3 color = 0.05 * albedo; // ambient

    for (uint i = 0u; i < lightData.lightCount; i++) {
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL)
            color += CalcDirLight(lightData.lights[i], albedo, albedo, normal, viewDir, 32.0);
        else if (lightData.lights[i].type == LIGHT_POINT)
            color += CalcPointLight(lightData.lights[i], albedo, albedo, normal, fragPos, viewDir, 32.0);
        else if (lightData.lights[i].type == LIGHT_SPOT)
            color += CalcSpotLight(lightData.lights[i], albedo, albedo, normal, fragPos, viewDir, 32.0);
    }

    outColor = vec4(color, 1.0);
}
