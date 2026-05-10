#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

bool directional_shadow_is_cascaded(ShadowData shadow)
{
    return shadow.cascade_splits.w > 0.0;
}

uint directional_shadow_cascade_index(ShadowData shadow, mat4 view, vec3 fragPos)
{
    float viewDepth = abs((view * vec4(fragPos, 1.0)).z);
    uint cascadeIndex = 0u;
    if (viewDepth > shadow.cascade_splits.x) cascadeIndex = 1u;
    if (viewDepth > shadow.cascade_splits.y) cascadeIndex = 2u;
    if (viewDepth > shadow.cascade_splits.z) cascadeIndex = 3u;
    return cascadeIndex;
}

vec2 directional_shadow_atlas_uv(vec2 uv, uint cascadeIndex, bool cascaded)
{
    if (!cascaded)
        return uv;

    vec2 atlasOffset = vec2(
        (cascadeIndex & 1u) == 0u ? 0.0 : 0.5,
        cascadeIndex < 2u ? 0.0 : 0.5
    );
    return uv * 0.5 + atlasOffset;
}

float shadow_factor(vec4 fragPosLS,
                    uint cascadeIndex,
                    bool cascaded,
                    vec3 normal,
                    vec3 lightDir,
                    float biasScale,
                    float biasMin)
{
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;
    if (proj.z > 1.0) return 1.0;
    proj.xy = directional_shadow_atlas_uv(proj.xy, cascadeIndex, cascaded);

    float cosTheta = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float bias = max(biasScale * (1.0 - cosTheta), biasMin);

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

float sample_point_shadow(vec3 fragPos,
                          vec3 lightPos,
                          float farPlane,
                          vec3 normal,
                          float biasMax,
                          float biasMin)
{
    vec3 dir = fragPos - lightPos;
    float currentDepth = length(dir) / farPlane;

    vec3 lightDir = normalize(lightPos - fragPos);
    float cosTheta = max(dot(normal, lightDir), 0.0);
    float bias = mix(biasMax, biasMin, cosTheta);

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

#endif
