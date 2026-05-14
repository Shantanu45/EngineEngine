#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/material.h"
#include "rendering/pipeline_builder.h"
#include "rendering/render_resource_store.h"
#include "rendering/render_settings.h"
#include "rendering/renderers/forward_render_pipeline.h"
#include "rendering/rid_handle.h"
#include "rendering/wsi.h"
#include "terrain/terrain_generator.h"

namespace Terrain {

/**
 * Owns the terrain sample application's scene resources and frame loop integration.
 *
 * The runtime builds a small forward-rendered scene containing streamed terrain chunks,
 * a skybox, optional grid, and a large camera-relative water plane. Terrain chunks are
 * generated procedurally on the CPU, uploaded as meshes, cached by chunk coordinate, and
 * swapped into the visible chunk list after a short delay so newly uploaded meshes are
 * not used before transfer work has had time to complete.
 */
class TerrainRuntime {
public:
	/**
	 * Initializes WSI state, resource stores, render pipeline, camera, terrain chunks, and water resources.
	 */
	bool initialize(
		Rendering::WSI* wsi,
		FileSystem::Filesystem& filesystem,
		std::shared_ptr<EE::InputSystemInterface> input_system);

	/**
	 * Advances input/streaming state, builds a SceneView, draws ImGui controls, and renders one frame.
	 */
	void render_frame(double frame_time, double elapsed_time);

	/**
	 * Destroys pending meshes, render pipeline state, and loaded render resources.
	 */
	void shutdown();

private:
	/**
	 * Runtime representation of one terrain chunk in the active/cached chunk set.
	 */
	struct TerrainChunk {
		/** Integer chunk coordinate on the world X axis. */
		int32_t x = 0;

		/** Integer chunk coordinate on the world Z axis. */
		int32_t z = 0;

		/** Uploaded terrain mesh for this chunk. */
		Rendering::MeshHandle mesh = Rendering::INVALID_MESH;

		/** Material containing the generated per-chunk color texture. */
		Rendering::MaterialHandle material = Rendering::INVALID_MATERIAL;
	};

	/**
	 * Delayed mesh-destruction record used to avoid freeing GPU resources too soon after removal.
	 */
	struct PendingMeshDestroy {
		Rendering::MeshHandle mesh = Rendering::INVALID_MESH;
		uint32_t frames_left = 0;
	};

	/** Configures vertex/index formats required by the terrain sample. */
	void configure_wsi();

	/** Creates initial terrain scene content. */
	void create_scene_resources();

	/** Drops cached terrain chunks and rebuilds the visible chunk window from scratch. */
	void regenerate_terrain_chunks();

	/**
	 * Builds the square chunk window around chunk_center.
	 * When discard_existing is false, chunks already present in chunk_cache are reused.
	 */
	void rebuild_chunk_window(bool discard_existing);

	/** Moves the chunk window when camera-based streaming is enabled and the camera enters a new chunk. */
	void update_streaming_chunks();

	/** Removes cached chunks outside the keep radius and queues their meshes for delayed destruction. */
	void prune_chunk_cache();

	/** Queues a mesh for delayed destruction after several frames. */
	void queue_mesh_destroy(Rendering::MeshHandle mesh);

	/** Retires queued mesh destructions once their grace period has elapsed. */
	void process_pending_mesh_destroys();

	/** Applies delayed chunk-window swaps and user-requested terrain regeneration. */
	void process_deferred_requests();

	/** Generates, uploads, and materializes one terrain chunk. */
	TerrainChunk create_chunk(int32_t x, int32_t z);

	/** Creates a material for a chunk, including its generated color texture. */
	Rendering::MaterialHandle create_chunk_material(int32_t x, int32_t z);

	/** Generates a small height/slope-based albedo texture for a terrain chunk. */
	RID create_chunk_color_texture(int32_t x, int32_t z);

	/** Creates the compute pipeline and buffers used for GPU terrain height generation experiments. */
	void create_compute_resources();

	/** Dispatches a compute pass that writes a height field for the current center chunk. */
	void dispatch_height_compute();

	/** Reads back the generated height field and compares it against the CPU height function. */
	void validate_height_compute();

	/** Creates the reusable water plane mesh and water material. */
	void create_water_resources();

	/** Draws ImGui controls that edit terrain, streaming, render, and water settings. */
	void draw_ui();

	/** Window/rendering integration used by the app. Not owned. */
	Rendering::WSI* wsi = nullptr;

	/** Rendering device retrieved from WSI. Not owned. */
	Rendering::RenderingDevice* device = nullptr;

	/** Input service used by the fly camera. */
	std::shared_ptr<EE::InputSystemInterface> input_system;

	/** Fly camera used to view and stream terrain. */
	Camera camera;

	/** Owns meshes, materials, textures, and skybox resources used by the scene. */
	Rendering::RenderResourceStore resources;

	/** Forward renderer used by this sample. */
	Rendering::ForwardRenderPipeline render_pipeline;

	// render doc count of frames captured.
	uint32_t rdoc_frame_captured = 0;

	/** Per-frame renderer options mirrored into the UI. */
	RenderSettings render_settings;

	/** Procedural terrain generation parameters. */
	TerrainSettings terrain_settings;

	/** Radius, in chunks, of the visible square chunk window around chunk_center. */
	int32_t chunk_radius = 2;

	/** Extra cached chunk margin kept around the visible chunk window when streaming. */
	int32_t chunk_cache_margin = 2;

	/** Enables camera-driven chunk window movement. */
	bool stream_chunks = false;

	/** Toggles drawing the water plane. */
	bool water_enabled = true;

	/** Base world-space Y level of the water plane. */
	float water_level = 0.0f;

	/** Additional chunks used to size water beyond the visible terrain window. */
	int32_t water_padding_chunks = 0;

	/** Moves the water plane forward from the camera to keep it under the view. */
	float water_forward_bias = 0.f;

	/** Minimum world-space width of the water plane. */
	float water_min_diameter = 80.0f;

	/** Multiplier applied to water width to make the plane longer in the camera-forward direction. */
	float water_depth_scale = 1.f;

	/** Set by the UI to rebuild terrain on a deferred frame boundary. */
	bool regenerate_requested = false;

	/** True when pending_chunks contains a freshly rebuilt visible chunk window. */
	bool pending_chunks_ready = false;

	/** Frame countdown before pending_chunks replaces chunks. */
	uint32_t pending_chunk_frames_left = 0;

	/** Current center chunk for the visible terrain window. */
	glm::ivec2 chunk_center = glm::ivec2(0);

	/** Chunks currently submitted for rendering. */
	std::vector<TerrainChunk> chunks;

	/** Newly rebuilt chunk set waiting to become visible. */
	std::vector<TerrainChunk> pending_chunks;

	/** Meshes waiting for delayed destruction. */
	std::vector<PendingMeshDestroy> pending_mesh_destroys;

	/** Handles that keep generated terrain color textures alive. */
	std::vector<RIDHandle> terrain_color_textures;

	/** Compute pipeline that generates terrain heights into a storage buffer. */
	Rendering::Pipeline height_compute_pipeline;

	/** Storage buffer containing terrain generation parameters consumed by the compute shader. */
	RIDHandle height_compute_params_buffer;

	/** Storage buffer receiving generated terrain heights from the compute shader. */
	RIDHandle height_compute_output_buffer;

	/** Descriptor set binding the terrain compute parameter and output buffers. */
	RIDHandle height_compute_uniform_set;

	/** Width/height of the generated GPU height field. */
	uint32_t height_compute_texture_size = 128;

	/** Enables the GPU height-field compute pass. CPU terrain generation remains the rendering source for now. */
	bool height_compute_enabled = true;

	/** Number of height-field compute dispatches recorded this run. */
	uint64_t height_compute_dispatches = 0;

	/** Set by the UI to run one GPU-vs-CPU height validation pass on the next frame. */
	bool height_compute_validation_requested = false;

	/** Whether a GPU-vs-CPU validation result is available for display. */
	bool height_compute_validation_valid = false;

	/** Maximum absolute height error from the last GPU-vs-CPU validation pass. */
	float height_compute_max_error = 0.0f;

	/** Average absolute height error from the last GPU-vs-CPU validation pass. */
	float height_compute_avg_error = 0.0f;

	/** Cache of generated chunks keyed by packed integer chunk coordinates. */
	std::unordered_map<uint64_t, TerrainChunk> chunk_cache;

	/** Uploaded helper grid mesh. */
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;

	/** Uploaded cube mesh used for the skybox pass. */
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;

	/** Uploaded unit plane scaled each frame to draw water. */
	Rendering::MeshHandle water_mesh = Rendering::INVALID_MESH;

	/** Material used by the water plane. */
	Rendering::MaterialHandle water_material = Rendering::INVALID_MATERIAL;

	/** Monotonic counter used to create unique terrain mesh names. */
	uint32_t terrain_mesh_generation = 0;

	/** Monotonic counter used to create unique generated texture names. */
	uint32_t terrain_texture_generation = 0;
};

} // namespace Terrain
