#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/material.h"
#include "rendering/render_resource_store.h"
#include "rendering/render_settings.h"
#include "rendering/renderers/forward_render_pipeline.h"
#include "rendering/rid_handle.h"
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
		Rendering::MaterialHandle material = Rendering::INVALID_MATERIAL;
	};

	struct PendingMeshDestroy {
		Rendering::MeshHandle mesh = Rendering::INVALID_MESH;
		uint32_t frames_left = 0;
	};

	void configure_wsi();
	void create_scene_resources();
	void regenerate_terrain_chunks();
	void rebuild_chunk_window(bool discard_existing);
	void update_streaming_chunks();
	void prune_chunk_cache();
	void queue_mesh_destroy(Rendering::MeshHandle mesh);
	void process_pending_mesh_destroys();
	void process_deferred_requests();
	TerrainChunk create_chunk(int32_t x, int32_t z);
	Rendering::MaterialHandle create_chunk_material(int32_t x, int32_t z);
	RID create_chunk_color_texture(int32_t x, int32_t z);
	void create_water_resources();
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
	int32_t chunk_cache_margin = 2;
	bool stream_chunks = false;
	bool water_enabled = true;
	float water_level = 0.0f;
	int32_t water_padding_chunks = 8;
	float water_forward_bias = 0.35f;
	float water_min_diameter = 480.0f;
	float water_depth_scale = 2.5f;
	bool regenerate_requested = false;
	bool pending_chunks_ready = false;
	uint32_t pending_chunk_frames_left = 0;
	glm::ivec2 chunk_center = glm::ivec2(0);

	std::vector<TerrainChunk> chunks;
	std::vector<TerrainChunk> pending_chunks;
	std::vector<PendingMeshDestroy> pending_mesh_destroys;
	std::vector<RIDHandle> terrain_color_textures;
	std::unordered_map<uint64_t, TerrainChunk> chunk_cache;
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle water_mesh = Rendering::INVALID_MESH;
	Rendering::MaterialHandle water_material = Rendering::INVALID_MATERIAL;
	uint32_t terrain_mesh_generation = 0;
	uint32_t terrain_texture_generation = 0;
};

} // namespace Terrain
