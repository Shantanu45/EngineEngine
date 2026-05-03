#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#define MAX_LIGHTS 16


// Must match CPU enum LightType : uint32_t
#define LIGHT_DIRECTIONAL 0u
#define LIGHT_POINT       1u
#define LIGHT_SPOT        2u

struct Light {
    vec4     position;    // xyz = pos,   w = range
    vec4     direction;   // xyz = dir,   w = innerCutOff (cos)
    vec4     color;       // xyz = color, w = intensity
    uint     type;
    float    outerAngle;  // outerCutOff (cos)
    float _pad0; 
    float _pad1; 
};

// Must match Material_UBO in material.h (std140 layout).
struct Material {
    vec4  base_color_factor;    // offset  0
    float metallic_factor;      // offset 16
    float roughness_factor;     // offset 20
    float shininess;            // offset 24  (Blinn-Phong)
    float alpha_cutoff;         // offset 28
    vec4  emissive_and_normal;  // offset 32  xyz=emissive_factor, w=normal_scale
    float occlusion_strength;   // offset 48
    float _pad0;                // offset 52
    float _pad1;                // offset 56
    float _pad2;                // offset 60
};

// ---------------------------------------------
//  Attenuation
// ---------------------------------------------

float CalcAttenuation(float distance, float range) {
    float x = clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0);
    return (x * x) / max(distance * distance, 0.01);
}

// ---------------------------------------------
//  Per-type internals
// ---------------------------------------------

vec3 CalcDirLight(Light light, vec3 albedo, vec3 specMap,
    vec3 normal, vec3 viewDir, float shininess)
{
    vec3  lightDir    = normalize(-light.direction.xyz);
    vec3  halfwayDir  = normalize(lightDir + viewDir);
    float diff        = max(dot(normal, lightDir), 0.0);
    float spec        = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3  radiance    = light.color.xyz * light.color.w;
    return (diff * albedo + spec * specMap) * radiance;
}

vec3 CalcPointLight(Light light, vec3 albedo, vec3 specMap,
    vec3 normal, vec3 fragPos, vec3 viewDir, float shininess)
{
    vec3  delta       = light.position.xyz - fragPos;
    float distance    = length(delta);
    vec3  lightDir    = delta / distance;
    vec3  halfwayDir  = normalize(lightDir + viewDir);
    float diff        = max(dot(normal, lightDir), 0.0);
    float spec        = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    float attenuation = CalcAttenuation(distance, light.position.w);
    vec3  radiance    = light.color.xyz * light.color.w * attenuation;
    return (diff * albedo + spec * specMap) * radiance;
}

vec3 CalcSpotLight(Light light, vec3 albedo, vec3 specMap,
    vec3 normal, vec3 fragPos, vec3 viewDir, float shininess)
{
    vec3  delta       = light.position.xyz - fragPos;
    float distance    = length(delta);
    vec3  lightDir    = delta / distance;
    vec3  halfwayDir  = normalize(lightDir + viewDir);
    float diff        = max(dot(normal, lightDir), 0.0);
    float spec        = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    float attenuation = CalcAttenuation(distance, light.position.w);

    float theta   = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.direction.w - light.outerAngle; // inner - outer
    float cone    = clamp((theta - light.outerAngle) / epsilon, 0.0, 1.0);

    vec3 radiance = light.color.xyz * light.color.w * attenuation * cone;
    return (diff * albedo + spec * specMap) * radiance;
}

// ---------------------------------------------
//  Main dispatch — call this in your frag shader
// ---------------------------------------------

vec3 CalcLight(Light light, Material material,
    texture2D diffuse_tex, texture2D specular_tex, sampler texSampler,
    vec2 texCoords, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 albedo  = vec3(texture(sampler2D(diffuse_tex,  texSampler), texCoords));
    vec3 specMap = vec3(texture(sampler2D(specular_tex, texSampler), texCoords));

    if (light.type == LIGHT_DIRECTIONAL)
        return CalcDirLight(light, albedo, specMap, normal, viewDir, material.shininess);
    else if (light.type == LIGHT_POINT)
        return CalcPointLight(light, albedo, specMap, normal, fragPos, viewDir, material.shininess);
    else if (light.type == LIGHT_SPOT)
        return CalcSpotLight(light, albedo, specMap, normal, fragPos, viewDir, material.shininess);

    return vec3(0.0);
}

#endif