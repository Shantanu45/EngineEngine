#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "rendering/primitve_shapes.h"

namespace Terrain {

enum class NoiseType { Value, Perlin };

/**
 * Controls procedural terrain generation for both mesh geometry and matching color textures.
 *
 * Coordinates are expressed in world units. Chunks are square, centered around their
 * integer chunk coordinate, and sample the same continuous height field so neighboring
 * chunks line up without seams.
 */
struct TerrainSettings {
	/** Seed used by the deterministic value-noise hash. */
	uint32_t seed = 1337;

	/** Number of grid cells per side in each terrain chunk. Vertices per side are resolution + 1. */
	uint32_t chunk_resolution = 64;

	/** World-space width/depth of one square chunk. */
	float chunk_size = 40.0f;

	/** Multiplier applied to normalized procedural noise to produce world-space height. */
	float height_scale = 12.0f;

	/** Base frequency used by the first octave of fractal value noise. */
	float base_frequency = 0.035f;

	/** Number of fractal noise layers to accumulate. */
	uint32_t octaves = 5;

	/** Frequency multiplier applied after each octave. */
	float lacunarity = 2.0f;

	/** Amplitude multiplier applied after each octave. */
	float persistence = 0.5f;

	/** noise type used for terrain generation */
	NoiseType noise_type = NoiseType::Value;

	float warp_strength = 1;
	float warp_strength2 = 0.5f;

	/** Low-frequency multiplier used for broad landforms. */
	float macro_frequency_scale = 0.45f;

	/** High-frequency multiplier used for fine terrain detail. */
	float detail_frequency_scale = 3.0f;

	/** Amount of high-frequency detail blended into the height field. */
	float detail_strength = 0.16f;

	/** Amount of ridged mountain structure added to the height field. */
	float ridged_strength = 0.22f;

	/** Amount of subtle stepped plateau shaping. */
	float terrace_strength = 0.08f;

	/** Number of height bands used by terrace shaping. */
	float terrace_levels = 8.0f;

	/** Softens extreme highs/lows using detail noise as a cheap erosion proxy. */
	float erosion_strength = 0.10f;

	/** Low-frequency biome/moisture variation used by terrain color. */
	float moisture_frequency = 0.018f;

	/** Strength of moisture tinting in terrain color. */
	float moisture_strength = 0.35f;
};

/**
 * Samples the continuous terrain height field at a world-space X/Z position.
 *
 * This is the canonical height function used by mesh generation, normal estimation,
 * and terrain color generation. Keeping all of those paths on this function is what
 * lets streamed chunks match each other spatially.
 */
float sample_terrain_height(const TerrainSettings& settings, float x, float z);

/**
 * Samples terrain albedo from the same height, slope, and biome rules used by generated color textures.
 */
glm::vec3 sample_terrain_color(const TerrainSettings& settings, float x, float z);

/**
 * Generates the default terrain chunk at chunk coordinate (0, 0).
 */
Rendering::Shapes::ShapeData generate_terrain_mesh(const TerrainSettings& settings);

/**
 * Generates a grid mesh for one terrain chunk at integer chunk coordinates.
 *
 * The generated vertex positions are in world space, so chunks can be rendered with
 * an identity model matrix. Normals are estimated by central-difference height samples.
 */
Rendering::Shapes::ShapeData generate_terrain_chunk_mesh(const TerrainSettings& settings, int32_t chunk_x, int32_t chunk_z);

} // namespace Terrain
