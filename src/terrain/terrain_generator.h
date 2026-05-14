#pragma once

#include <cstdint>

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
