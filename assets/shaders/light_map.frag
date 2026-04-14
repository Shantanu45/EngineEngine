#version 450 core
#include "lib/common.glsl"
#include "lib/lighting.glsl"

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
    float _pad1;
    float _pad2;
    float _pad3;
    mat4 lightSpaceMatrix; 
} frame;

layout(set = 1, binding = 0) uniform texture2D shadowMap;
//layout(set = 1, binding = 1) uniform textureCubeShadow PointShadowMap;
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

    vec2 texelSize = 1.0 / textureSize(sampler2DShadow(shadowMap, pcfSampler), 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            shadow += texture(sampler2DShadow(shadowMap, pcfSampler), vec3(proj.xy + vec2(x, y) * texelSize, proj.z - bias));
        }
    }
    return shadow / 9.0;
}

float samplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane, vec3 normal) {
    vec3  dir          = fragPos - lightPos;
    float currentDepth = length(dir) / farPlane;

    // slope-scaled bias: surfaces facing the light get minimal bias,
    // grazing surfaces get more to avoid acne
    vec3  lightDir = normalize(-dir);
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float bias     = max(0.005 * (1.0 - cosTheta), 0.0005);

    float closestDepth = texture(samplerCube(PointShadowMap, pointShadowSampler), dir).r;
    return currentDepth - bias < closestDepth ? 1.0 : 0.0;
}

//float samplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane) {
//    vec3  dir          = fragPos - lightPos;
//    float currentDepth = length(dir) / farPlane; // normalize same way as frag shader
//
//    // samplerCubeShadow does the compare for you (bias baked in)
//    float bias = 0.005;
//    
//    return texture(PointShadowMap, vec4(dir, currentDepth - bias));
//}

// TODO: impl for multiple lights
void main()
{
    // Sample normal map and unpack from [0,1] to [-1,1] (tangent space)
    vec3 tangentNormal = texture(sampler2D(normal_tex, texSampler), TexCoords).rgb * 2.0 - 1.0;

    // Build TBN to rotate from tangent space -> world space
    mat3 TBN   = mat3(normalize(Tangent), normalize(Bitangent), normalize(Normal));
    vec3 normal = normalize(TBN * tangentNormal);
    vec3 viewDir = normalize(frame.camera.cameraPos - FragPos);
    // Global ambient — one cheap sample, not multiplied per light
    vec3 color = vec3(0.05) * vec3(texture(sampler2D(diffuse_tex, texSampler), TexCoords));

    for (uint i = 0u; i < lightData.lightCount; i++) {
        color += CalcLight(
            lightData.lights[i], mat.material,
            diffuse_tex, metallic_roughness, texSampler,
            TexCoords, normal, FragPos, viewDir);
    }
    
    float shadow = 1.0;
    for (uint i = 0u; i < lightData.lightCount; i++) {
        if (lightData.lights[i].type == LIGHT_DIRECTIONAL) {
            vec3 ld = normalize(-vec3(lightData.lights[i].direction));
            shadow = min(shadow, shadow_factor(fragPosLightSpace, normal, ld));
        } else if (lightData.lights[i].type == LIGHT_POINT) {
            shadow = min(shadow, samplePointShadow(FragPos,
                lightData.lights[i].position.xyz,
                lightData.lights[i].position.w,
                normal));
        }
    }

    FragColor = vec4(color * shadow, 1.0);
    //FragColor = vec4(color, 1.0);
}