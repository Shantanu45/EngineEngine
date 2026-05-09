#pragma once

#include <cstdint>

#include "rendering/primitve_shapes.h"

namespace Terrain {

struct TerrainSettings {
	uint32_t seed = 1337;
	uint32_t chunk_resolution = 64;
	float chunk_size = 40.0f;
	float height_scale = 12.0f;
	float base_frequency = 0.035f;
	uint32_t octaves = 5;
	float lacunarity = 2.0f;
	float persistence = 0.5f;
};

float sample_terrain_height(const TerrainSettings& settings, float x, float z);
Rendering::Shapes::ShapeData generate_terrain_mesh(const TerrainSettings& settings);
Rendering::Shapes::ShapeData generate_terrain_chunk_mesh(const TerrainSettings& settings, int32_t chunk_x, int32_t chunk_z);

} // namespace Terrain
