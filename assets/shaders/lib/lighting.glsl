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

struct Material {
    vec4  base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float shininess;        // <-- keeping this for Blinn-Phong
    float _pad;
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
    sampler2D diffuse_tex, sampler2D specular_tex,
    vec2 texCoords, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 albedo  = vec3(texture(diffuse_tex,  texCoords));
    vec3 specMap = vec3(texture(specular_tex, texCoords));

    if (light.type == LIGHT_DIRECTIONAL)
        return CalcDirLight(light, albedo, specMap, normal, viewDir, material.shininess);
    else if (light.type == LIGHT_POINT)
        return CalcPointLight(light, albedo, specMap, normal, fragPos, viewDir, material.shininess);
    else if (light.type == LIGHT_SPOT)
        return CalcSpotLight(light, albedo, specMap, normal, fragPos, viewDir, material.shininess);

    return vec3(0.0);
}

#endif