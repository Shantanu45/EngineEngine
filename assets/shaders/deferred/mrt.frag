#version 450
#include "../lib/common.glsl"
#include "../lib/lighting.glsl"

layout(set = 0, binding = 3) uniform sampler texSampler;

layout(set = 2, binding = 0) uniform MaterialUBO {
    Material material;
} mat;

layout(set = 2, binding = 1) uniform texture2D diffuse_tex;
layout(set = 2, binding = 2) uniform texture2D metallic_roughness;
layout(set = 2, binding = 3) uniform texture2D normal_tex;
layout(set = 2, binding = 4) uniform texture2D displacement_tex;
layout(set = 2, binding = 5) uniform texture2D emissive_tex;
layout(set = 2, binding = 6) uniform texture2D occlusion_tex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outMaterial;
layout (location = 4) out vec4 outEmissive;

#define ALPHA_MODE_MASK 1u

void main() 
{
	outPosition = vec4(inWorldPos, 1.0);

	// Calculate normal in tangent space
	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent);
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);
	vec3 tnorm = TBN * normalize(texture(sampler2D(normal_tex, texSampler), inUV).rgb * 2.0 - 1.0);
	outNormal = vec4(tnorm, 1.0);

	vec4 baseColor = texture(sampler2D(diffuse_tex, texSampler), inUV)
		* mat.material.base_color_factor;
	if (mat.material.alpha_mode == ALPHA_MODE_MASK &&
		baseColor.a < mat.material.alpha_cutoff)
		discard;

	vec3 metallicRoughness = texture(sampler2D(metallic_roughness, texSampler), inUV).rgb;
	float roughness = clamp(metallicRoughness.g * mat.material.roughness_factor, 0.04, 1.0);
	float metallic = clamp(metallicRoughness.b * mat.material.metallic_factor, 0.0, 1.0);
	float ao = texture(sampler2D(occlusion_tex, texSampler), inUV).r;
	float ambientOcclusion = mix(1.0, ao, mat.material.occlusion_strength);
	vec3 emissive = mat.material.emissive_and_normal.xyz
		* vec3(texture(sampler2D(emissive_tex, texSampler), inUV));

	outAlbedo = baseColor;
	outMaterial = vec4(roughness, metallic, ambientOcclusion, 1.0);
	outEmissive = vec4(emissive, 1.0);
}
