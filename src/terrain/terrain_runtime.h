#pragma once

#include <memory>
#include <vector>

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/material.h"
#include "rendering/render_resource_store.h"
#include "rendering/render_settings.h"
#include "rendering/renderers/forward_render_pipeline.h"
#include "rendering/wsi.h"
#include "terrain/terrain_generator.h"

namespace Terrain {

class TerrainRuntime {
public:
	bool initialize(
		Rendering::WSI* wsi,
		FileSystem::Filesystem& filesystem,
		std::shared_ptr<EE::InputSystemInterface> input_system);
	void render_frame(double frame_time, double elapsed_time);
	void shutdown();

private:
	struct TerrainChunk {
		int32_t x = 0;
		int32_t z = 0;
		Rendering::MeshHandle mesh = Rendering::INVALID_MESH;
	};

	void configure_wsi();
	void create_scene_resources();
	void create_terrain_material();
	void regenerate_terrain_chunks();
	void update_streaming_chunks();
	void draw_ui();

	Rendering::WSI* wsi = nullptr;
	Rendering::RenderingDevice* device = nullptr;
	std::shared_ptr<EE::InputSystemInterface> input_system;

	Camera camera;
	Rendering::RenderResourceStore resources;
	Rendering::ForwardRenderPipeline render_pipeline;
	RenderSettings render_settings;
	TerrainSettings terrain_settings;
	int32_t chunk_radius = 2;
	bool stream_chunks = false;
	glm::ivec2 chunk_center = glm::ivec2(0);

	std::vector<TerrainChunk> chunks;
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;
	Rendering::MaterialHandle terrain_material = Rendering::INVALID_MATERIAL;
	uint32_t terrain_mesh_generation = 0;
};

} // namespace Terrain
