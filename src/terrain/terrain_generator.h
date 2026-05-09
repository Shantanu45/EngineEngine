#pragma once

#include <cstdint>

#include "rendering/primitve_shapes.h"

namespace Terrain {

struct TerrainSettings {
	uint32_t seed = 1337;
	uint32_t resolution = 128;
	float size = 80.0f;
	float height_scale = 12.0f;
	float base_frequency = 0.035f;
	uint32_t octaves = 5;
	float lacunarity = 2.0f;
	float persistence = 0.5f;
};

Rendering::Shapes::ShapeData generate_terrain_mesh(const TerrainSettings& settings);

} // namespace Terrain
