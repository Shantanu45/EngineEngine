#include "terrain/terrain_generator.h"

#include "rendering/gltf_loader.h"

#include <cmath>

namespace Terrain {
namespace {

float fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

uint32_t hash(uint32_t x, uint32_t y, uint32_t seed)
{
	uint32_t h = seed ^ (x * 374761393u) ^ (y * 668265263u);
	h = (h ^ (h >> 13u)) * 1274126177u;
	return h ^ (h >> 16u);
}

float random_unit(uint32_t x, uint32_t y, uint32_t seed)
{
	return static_cast<float>(hash(x, y, seed) & 0xffffu) / 32767.5f - 1.0f;
}


float value_noise(float x, float y, uint32_t seed)
{
	const int32_t x0 = static_cast<int32_t>(std::floor(x));
	const int32_t y0 = static_cast<int32_t>(std::floor(y));
	const int32_t x1 = x0 + 1;
	const int32_t y1 = y0 + 1;

	const float tx = fade(x - static_cast<float>(x0));
	const float ty = fade(y - static_cast<float>(y0));

	// Hash the four integer lattice corners and smoothly interpolate between them.
	// This gives deterministic, continuous value noise without storing a noise table.
	const float a = random_unit(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), seed);
	const float b = random_unit(static_cast<uint32_t>(x1), static_cast<uint32_t>(y0), seed);
	const float c = random_unit(static_cast<uint32_t>(x0), static_cast<uint32_t>(y1), seed);
	const float d = random_unit(static_cast<uint32_t>(x1), static_cast<uint32_t>(y1), seed);

	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
}

glm::vec2 gradient(uint32_t x, uint32_t y, uint32_t seed)
{
	const uint32_t h = hash(x, y, seed) & 7u;
	constexpr float inv_sqrt2 = 0.70710678118f;
	constexpr glm::vec2 dirs[8] = {
		{ 1.0f, 0.0f },
		{ inv_sqrt2, inv_sqrt2 },
		{ 0.0f, 1.0f },
		{ -inv_sqrt2, inv_sqrt2 },
		{ -1.0f, 0.0f },
		{ -inv_sqrt2, -inv_sqrt2 },
		{ 0.0f, -1.0f },
		{ inv_sqrt2, -inv_sqrt2 },
	};
	return dirs[h];
}

float perlin_noise(float x, float y, uint32_t seed)
{
	const int32_t x0 = static_cast<int32_t>(std::floor(x));
	const int32_t y0 = static_cast<int32_t>(std::floor(y));

	const float xf = x - static_cast<float>(x0);
	const float yf = y - static_cast<float>(y0);

	const float tx = fade(xf);
	const float ty = fade(yf);

	const float a = glm::dot(gradient(x0, y0, seed), { xf,        yf });
	const float b = glm::dot(gradient(x0 + 1, y0, seed), { xf - 1.0f, yf });
	const float c = glm::dot(gradient(x0, y0 + 1, seed), { xf,        yf - 1.0f });
	const float d = glm::dot(gradient(x0 + 1, y0 + 1, seed), { xf - 1.0f, yf - 1.0f });

	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty) * 1.41421356237f;
}

float fractal_noise(float x, float y, const TerrainSettings& settings)
{
	float frequency = settings.base_frequency;
	float amplitude = 1.0f;
	float sample = 0.0f;
	float norm = 0.0f;

	for (uint32_t octave = 0; octave < settings.octaves; ++octave) {
		// Each octave samples the same continuous world-space point at a higher
		// frequency and lower amplitude. The normalized sum stays roughly [-1, 1].
		sample += ((settings.noise_type == NoiseType::Perlin)
			? perlin_noise(x * frequency, y * frequency, settings.seed + octave * 1013u)
			: value_noise(x * frequency, y * frequency, settings.seed + octave * 1013u)) * amplitude;
		//value += value_noise(x * frequency, y * frequency, settings.seed + octave * 1013u) * amplitude;
		norm += amplitude;
		frequency *= settings.lacunarity;
		amplitude *= settings.persistence;
	}

	return norm > 0.0f ? sample / norm : 0.0f;
}

} // namespace

// Domain Warping
float sample_terrain_height(const TerrainSettings& settings, float x, float z)
{
	// First warp pass: displace sample coordinates by noise.
	float wx1 = fractal_noise(x, z, settings) * settings.warp_strength;
	float wz1 = fractal_noise(x + 5.2f, z + 1.3f, settings) * settings.warp_strength;

	// Second warp pass: warp the warp itself for geological folding.
	float wx2 = fractal_noise(x + wx1, z + wz1, settings) * settings.warp_strength2;
	float wz2 = fractal_noise(x + wx1 + 1.7f, z + wz1 + 9.2f, settings) * settings.warp_strength2;

	const float h = fractal_noise(x + wx2, z + wz2, settings);
	const float shaped = h * 0.75f + h * h * h * 0.25f;
	return shaped * settings.height_scale;
}

//float sample_terrain_height(const TerrainSettings& settings, float x, float z)
//{
//	const float h = fractal_noise(x, z, settings);
//	// Blend linear and cubic noise to keep broad hills while making peaks/valleys less uniform.
//	const float shaped = h * 0.75f + h * h * h * 0.25f;
//	return shaped * settings.height_scale;
//}

Rendering::Shapes::ShapeData generate_terrain_mesh(const TerrainSettings& settings)
{
	return generate_terrain_chunk_mesh(settings, 0, 0);
}

Rendering::Shapes::ShapeData generate_terrain_chunk_mesh(const TerrainSettings& settings, int32_t chunk_x, int32_t chunk_z)
{
	Rendering::Shapes::ShapeData mesh;
	const uint32_t resolution = glm::max(settings.chunk_resolution, 1u);
	const uint32_t verts_per_side = resolution + 1u;
	const float step = settings.chunk_size / static_cast<float>(resolution);
	const float half_chunk = settings.chunk_size * 0.5f;
	const float origin_x = static_cast<float>(chunk_x) * settings.chunk_size - half_chunk;
	const float origin_z = static_cast<float>(chunk_z) * settings.chunk_size - half_chunk;

	mesh.vertices.reserve(static_cast<size_t>(verts_per_side) * static_cast<size_t>(verts_per_side));
	mesh.indices.reserve(static_cast<size_t>(resolution) * static_cast<size_t>(resolution) * 6u);

	for (uint32_t z = 0; z < verts_per_side; ++z) {
		for (uint32_t x = 0; x < verts_per_side; ++x) {
			const float world_x = origin_x + static_cast<float>(x) * step;
			const float world_z = origin_z + static_cast<float>(z) * step;
			const float y = sample_terrain_height(settings, world_x, world_z);

			const float h_l = sample_terrain_height(settings, world_x - step, world_z);
			const float h_r = sample_terrain_height(settings, world_x + step, world_z);
			const float h_d = sample_terrain_height(settings, world_x, world_z - step);
			const float h_u = sample_terrain_height(settings, world_x, world_z + step);
			// Estimate the normal from neighboring height samples so lighting follows the same height field.
			const glm::vec3 normal = glm::normalize(glm::vec3(h_l - h_r, step * 2.0f, h_d - h_u));

			mesh.vertices.push_back(Rendering::Vertex{
				.position = glm::vec3(world_x, y, world_z),
				.normal = normal,
				.texcoord = glm::vec2(
					static_cast<float>(x) / static_cast<float>(resolution),
					static_cast<float>(z) / static_cast<float>(resolution)),
				.tangent = glm::vec4(0.0f),
			});
		}
	}

	for (uint32_t z = 0; z < resolution; ++z) {
		for (uint32_t x = 0; x < resolution; ++x) {
			const uint32_t i0 = z * verts_per_side + x;
			const uint32_t i1 = i0 + 1u;
			const uint32_t i2 = i0 + verts_per_side;
			const uint32_t i3 = i2 + 1u;
			// Two triangles per grid cell, sharing vertices with neighboring cells/chunks.
			mesh.indices.insert(mesh.indices.end(), { i0, i2, i1, i1, i2, i3 });
		}
	}

	return mesh;
}

} // namespace Terrain
