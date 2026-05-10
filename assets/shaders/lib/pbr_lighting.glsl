#ifndef PBR_LIGHTING_GLSL
#define PBR_LIGHTING_GLSL

#define MAX_LIGHTS 16

// Must match ShadowData in frame_data.h (std140 layout).
struct ShadowData {
    mat4  matrices[6];
    vec4  light_pos;
    vec4  cascade_splits;
    uint  light_index;
    float _pad0, _pad1, _pad2;
};

// Must match CPU enum LightType : uint32_t.
#define LIGHT_DIRECTIONAL 0u
#define LIGHT_POINT       1u
#define LIGHT_SPOT        2u

struct Light {
    vec4  position;    // xyz = pos,   w = range
    vec4  direction;   // xyz = dir,   w = innerCutOff (cos)
    vec4  color;       // xyz = color, w = intensity
    uint  type;
    float outerAngle;  // outerCutOff (cos)
    float _pad0;
    float _pad1;
};

// Must match Material_UBO in material.h (std140 layout).
struct Material {
    vec4  base_color_factor;    // offset  0
    float metallic_factor;      // offset 16
    float roughness_factor;     // offset 20
    float shininess;            // offset 24  unused here
    float alpha_cutoff;         // offset 28
    vec4  emissive_and_normal;  // offset 32  xyz=emissive_factor, w=normal_scale
    float occlusion_strength;   // offset 48
    uint  alpha_mode;           // offset 52
    float _pad1;                // offset 56
    float _pad2;                // offset 60
};

const float PI = 3.14159265359;

float CalcAttenuation(float distance, float range)
{
    float x = clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0);
    return (x * x) / max(distance * distance, 0.01);
}

float DistributionGGX(vec3 normal, vec3 halfwayDir, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(normal, halfwayDir), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / max(NdotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness)
{
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ggxV  = GeometrySchlickGGX(NdotV, roughness);
    float ggxL  = GeometrySchlickGGX(NdotL, roughness);

    return ggxV * ggxL;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalcBRDF(vec3 albedo,
              float metallic,
              float roughness,
              vec3 normal,
              vec3 lightDir,
              vec3 viewDir,
              vec3 radiance)
{
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    if (NdotL <= 0.0 || NdotV <= 0.0)
        return vec3(0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F  = FresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
    float D = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);

    vec3 numerator    = D * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 0.000001);
    vec3 specular     = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * radiance * NdotL;
}

vec3 CalcDirLight(Light light,
                  vec3 albedo,
                  float metallic,
                  float roughness,
                  vec3 normal,
                  vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction.xyz);
    vec3 radiance = light.color.xyz * light.color.w;

    return CalcBRDF(albedo, metallic, roughness,
                    normal, lightDir, viewDir, radiance);
}

vec3 CalcPointLight(Light light,
                    vec3 albedo,
                    float metallic,
                    float roughness,
                    vec3 normal,
                    vec3 fragPos,
                    vec3 viewDir)
{
    vec3 delta    = light.position.xyz - fragPos;
    float dist    = length(delta);
    vec3 lightDir = delta / max(dist, 0.0001);

    float attenuation = CalcAttenuation(dist, light.position.w);
    vec3 radiance = light.color.xyz * light.color.w * attenuation;

    return CalcBRDF(albedo, metallic, roughness,
                    normal, lightDir, viewDir, radiance);
}

vec3 CalcSpotLight(Light light,
                   vec3 albedo,
                   float metallic,
                   float roughness,
                   vec3 normal,
                   vec3 fragPos,
                   vec3 viewDir)
{
    vec3 delta    = light.position.xyz - fragPos;
    float dist    = length(delta);
    vec3 lightDir = delta / max(dist, 0.0001);

    float attenuation = CalcAttenuation(dist, light.position.w);

    float theta   = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = max(light.direction.w - light.outerAngle, 0.0001);
    float cone    = clamp((theta - light.outerAngle) / epsilon, 0.0, 1.0);

    vec3 radiance = light.color.xyz * light.color.w * attenuation * cone;

    return CalcBRDF(albedo, metallic, roughness,
                    normal, lightDir, viewDir, radiance);
}

vec3 CalcLight(Light light,
               Material material,
               texture2D diffuse_tex,
               texture2D metallic_roughness_tex,
               sampler texSampler,
               vec2 texCoords,
               vec3 normal,
               vec3 fragPos,
               vec3 viewDir)
{
    vec4 baseColorSample =
        texture(sampler2D(diffuse_tex, texSampler), texCoords);

    vec3 metallicRoughnessSample =
        texture(sampler2D(metallic_roughness_tex, texSampler), texCoords).rgb;

    vec3 albedo = baseColorSample.rgb * material.base_color_factor.rgb;

    // glTF metallicRoughnessTexture convention:
    // R = unused here, often available for occlusion in ORM textures
    // G = roughness
    // B = metallic
    float roughness = metallicRoughnessSample.g * material.roughness_factor;
    float metallic  = metallicRoughnessSample.b * material.metallic_factor;

    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic,  0.0, 1.0);

    normal  = normalize(normal);
    viewDir = normalize(viewDir);

    if (light.type == LIGHT_DIRECTIONAL)
        return CalcDirLight(light, albedo, metallic, roughness,
                            normal, viewDir);

    else if (light.type == LIGHT_POINT)
        return CalcPointLight(light, albedo, metallic, roughness,
                              normal, fragPos, viewDir);

    else if (light.type == LIGHT_SPOT)
        return CalcSpotLight(light, albedo, metallic, roughness,
                             normal, fragPos, viewDir);

    return vec3(0.0);
}

#endif
