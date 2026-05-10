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

float shadow_factor(vec4 fragPosLS,
                    uint cascadeIndex,
                    vec3 normal,
                    vec3 lightDir,
                    float biasScale,
                    float biasMin)
{
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;
    if (proj.z > 1.0) return 1.0;

    float cosTheta = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float bias = max(biasScale * (1.0 - cosTheta), biasMin);

    vec2 texelSize = 1.0 / vec2(textureSize(sampler2DArrayShadow(shadowMap, pcfSampler), 0).xy);
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            shadow += texture(sampler2DArrayShadow(shadowMap, pcfSampler),
                              vec4(proj.xy + vec2(x, y) * texelSize,
                                   float(cascadeIndex),
                                   proj.z - bias));
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
