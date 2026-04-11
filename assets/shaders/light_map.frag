#version 450 core
#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;
layout(location = 3) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
    float _pad1;
    float _pad2;
    float _pad3;
    mat4 lightSpaceMatrix; 
} frame;

layout(set = 1, binding = 0) uniform sampler2DShadow shadowMap;
layout(set = 1, binding = 1) uniform samplerCubeShadow PointShadowMap;

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

layout(set = 2, binding = 1) uniform sampler2D diffuse_tex;
layout(set = 2, binding = 2) uniform sampler2D metallic_roughness;
layout(set = 2, binding = 3) uniform sampler2D normal;


//float shadow_factor(vec4 fragPosLS, vec3 normal, vec3 lightDir) {
//    vec3 proj = fragPosLS.xyz / fragPosLS.w;
//    proj.xy = proj.xy * 0.5 + 0.5;
//
//    if (proj.z > 1.0) return 1.0;
//
//    // slope-scaled bias — more bias on surfaces nearly perpendicular to light
//    float cosTheta = max(dot(normalize(normal), normalize(lightDir)), 0.0);
//    float bias = max(0.002 * (1.0 - cosTheta), 0.0005);
//
//    float shadow = 0.0;
//    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);           // returns ivec2(width, height) of mip level 0
//    for (int x = -1; x <= 1; x++)
//    {
//        for (int y = -1; y <= 1; y++) {
//            float depth = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
//            shadow += (proj.z - bias) > depth ? 0.0 : 1.0;
//        }
//    }
//    return shadow / 9.0;
//}

float shadow_factor(vec4 fragPosLS, vec3 normal, vec3 lightDir) {
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;

    float cosTheta = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float bias = max(0.002 * (1.0 - cosTheta), 0.0005);

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            shadow += texture(shadowMap, vec3(proj.xy + vec2(x, y) * texelSize, proj.z - bias));
        }
    }
    return shadow / 9.0;
}

float samplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane) {
    vec3  dir          = fragPos - lightPos;
    float currentDepth = length(dir) / farPlane; // normalize same way as frag shader

    // samplerCubeShadow does the compare for you (bias baked in)
    float bias = 0.005;
    return texture(PointShadowMap, vec4(dir, currentDepth - bias));
}

void main()
{
    vec3 normal  = normalize(Normal);
    vec3 viewDir = normalize(frame.camera.cameraPos - FragPos);
    vec3 lightDir = normalize(-vec3(lightData.lights[0].direction)); // toward light
    // Global ambient — one cheap sample, not multiplied per light
    vec3 color = vec3(0.05) * vec3(texture(diffuse_tex, TexCoords));

    for (uint i = 0u; i < lightData.lightCount; i++) {
        color += CalcLight(
            lightData.lights[i], mat.material,
            diffuse_tex, metallic_roughness,
            TexCoords, normal, FragPos, viewDir);
    }
    float shadow = shadow_factor(fragPosLightSpace, normal, lightDir);
    FragColor = vec4(color * shadow, 1.0);
    //FragColor = vec4(color, 1.0);
}