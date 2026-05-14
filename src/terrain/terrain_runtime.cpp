#include "terrain/terrain_runtime.h"

#include "imgui.h"
#include "rendering/primitve_shapes.h"
#include "rendering/utils.h"
#include "util/profiler.h"

#include <glm/gtc/matrix_transform.hpp>
#include "util/renderdoc_helpers.h"


#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace Terrain {
namespace {

uint64_t chunk_key(int32_t x, int32_t z)
{
	// Pack signed chunk coordinates through their uint32_t representation so the key is stable and reversible.
	const auto ux = static_cast<uint32_t>(x);
	const auto uz = static_cast<uint32_t>(z);
	return (static_cast<uint64_t>(ux) << 32u) | static_cast<uint64_t>(uz);
}

glm::ivec2 decode_chunk_key(uint64_t key)
{
	return glm::ivec2(
		static_cast<int32_t>(static_cast<uint32_t>(key >> 32u)),
		static_cast<int32_t>(static_cast<uint32_t>(key & 0xffffffffu)));
}

const char* material_debug_view_name(MaterialDebugView view)
{
	switch (view) {
	case MaterialDebugView::Lit: return "Lit";
	case MaterialDebugView::Albedo: return "Albedo";
	case MaterialDebugView::Normal: return "Normal";
	case MaterialDebugView::Roughness: return "Roughness";
	case MaterialDebugView::Metallic: return "Metallic";
	case MaterialDebugView::AmbientOcclusion: return "AO";
	case MaterialDebugView::Emissive: return "Emissive";
	case MaterialDebugView::ShadowFactor: return "Shadow";
	case MaterialDebugView::LightCount: return "Light Count";
	case MaterialDebugView::Depth: return "Depth";
	case MaterialDebugView::DirectionalShadowMap: return "Directional Shadow Map";
	case MaterialDebugView::LightSpaceCoords: return "Light Space Coords";
	case MaterialDebugView::CascadeBoundaries: return "Cascade Boundaries";
	}
	return "Unknown";
}

struct TerrainComputeParams {
	uint32_t seed = 0;
	uint32_t noise_type = 0;
	uint32_t texture_size = 0;
	uint32_t octaves = 0;
	float chunk_size = 0.0f;
	float height_scale = 0.0f;
	float base_frequency = 0.0f;
	float lacunarity = 0.0f;
	float persistence = 0.0f;
	float warp_strength = 0.0f;
	float warp_strength2 = 0.0f;
	float chunk_x = 0.0f;
	float chunk_z = 0.0f;
};

} // namespace

bool TerrainRuntime::initialize(
	Rendering::WSI* wsi_,
	FileSystem::Filesystem& filesystem,
	std::shared_ptr<EE::InputSystemInterface> input_system_)
{
	wsi = wsi_;
	device = wsi->get_rendering_device();
	input_system = std::move(input_system_);

	RenderUtilities::capturing_timestamps = false;

	configure_wsi();
	resources.initialize(device, filesystem);

	grid_mesh = Rendering::Shapes::upload_grid(*wsi, resources.meshes(), 50, 1.0f, "terrain_grid");
	skybox_mesh = Rendering::Shapes::upload_cube(*wsi, resources.meshes(), "terrain_skybox_cube");
	resources.load_skybox_cubemap({
		"assets://textures/skybox/right.jpg",
		"assets://textures/skybox/left.jpg",
		"assets://textures/skybox/top.jpg",
		"assets://textures/skybox/bottom.jpg",
		"assets://textures/skybox/front.jpg",
		"assets://textures/skybox/back.jpg",
	});

	render_settings.use_pbr_lighting = true;
	render_settings.draw_grid = true;
	render_settings.draw_skybox = true;

	render_pipeline.initialize(wsi, device, resources.skybox_cubemap());
	create_compute_resources();
	create_scene_resources();
	create_water_resources();

	camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
	camera.set_reset_on_resize(true);
	camera.set_mode(CameraMode::Fly);
	camera.set_position(glm::vec3(0.0f, 28.0f, 70.0f));
	camera.set_euler_degrees(22.0f, 180.0f, 0.0f);
	camera.set_move_speed(24.0f);

	wsi->submit_transfer_workers();
	return wsi->pre_frame_loop();
}

void TerrainRuntime::render_frame(double frame_time, double elapsed_time)
{
	ZoneScoped;
	process_deferred_requests();
	camera.update_from_input(input_system.get(), frame_time);
	update_streaming_chunks();
	process_pending_mesh_destroys();
	dispatch_height_compute();
	device->imgui_begin_frame();
	draw_ui();
	resources.materials().upload_dirty(device);

	Rendering::SceneView view;
	view.camera.view = camera.get_view();
	view.camera.proj = camera.get_projection();
	view.camera.cameraPos = camera.get_position();
	view.camera.near_clip = camera.get_near_clip();
	view.camera.far_clip = camera.get_far_clip();
	view.elapsed = elapsed_time;
	view.extent = { device->screen_get_width(), device->screen_get_height() };
	view.use_pbr_lighting = render_settings.use_pbr_lighting;
	view.material_debug_view = render_settings.material_debug_view;
	view.directional_shadow_mode = render_settings.directional_shadow_mode;
	view.skybox_mesh = render_settings.draw_skybox ? skybox_mesh : Rendering::INVALID_MESH;
	view.grid_mesh = render_settings.draw_grid ? grid_mesh : Rendering::INVALID_MESH;

	const glm::mat4 model(1.0f);
	for (const auto& chunk : chunks) {
		view.instances.push_back(Rendering::MeshInstance{
			.mesh = chunk.mesh,
			.model = model,
			.normal_matrix = model,
			.material_sets = { resources.materials().get_uniform_set(chunk.material, render_settings.use_pbr_lighting) },
			.shadow_material_sets = { resources.materials().get_shadow_uniform_set(chunk.material) },
			.point_shadow_material_sets = { resources.materials().get_point_shadow_uniform_set(chunk.material) },
			.category = Rendering::MeshCategory::Opaque,
		});
	}

	if (water_enabled && water_mesh != Rendering::INVALID_MESH && water_material != Rendering::INVALID_MATERIAL) {
		const auto camera_position = camera.get_position();
		glm::vec3 forward = camera.get_forward();
		forward.y = 0.0f;
		if (glm::length(forward) > 0.001f)
			forward = glm::normalize(forward);
		else
			forward = glm::vec3(0.0f, 0.0f, -1.0f);
		const int32_t padded_radius = std::max(chunk_radius + water_padding_chunks, chunk_radius);
		const float chunk_window_diameter = static_cast<float>(padded_radius * 2 + 1) * terrain_settings.chunk_size;
		const float width = std::max(chunk_window_diameter, water_min_diameter);
		const float depth = width * water_depth_scale;
		// The water plane is a camera-relative rectangle, not a world ocean mesh.
		// Biasing it forward keeps the visible region covered without requiring huge terrain-sized water geometry.
		const glm::vec3 water_center = camera_position + forward * depth * water_forward_bias;
		const float display_water_level = water_level + 0.02f;
		const float yaw = std::atan2(forward.x, forward.z);
		const glm::mat4 water_model =
			model *
			glm::scale(glm::mat4(1.0f), glm::vec3(width, 1.0f, depth));
		view.instances.push_back(Rendering::MeshInstance{
			.mesh = water_mesh,
			.model = water_model,
			.normal_matrix = glm::transpose(glm::inverse(water_model)),
			.material_sets = { resources.materials().get_uniform_set(water_material, render_settings.use_pbr_lighting) },
			.category = Rendering::MeshCategory::Opaque,
		});
	}

	view.lights.push_back(Light{
		.position = glm::vec4(35.0f, 45.0f, 20.0f, 60.0f),
		.direction = glm::vec4(glm::normalize(glm::vec3(-0.6f, -1.0f, -0.35f)), 0.0f),
		.color = glm::vec4(1.0f, 0.95f, 0.85f, 2.5f),
		.type = static_cast<uint32_t>(LightType::Directional),
		.outer_angle = 0.0f,
	});

	wsi->set_render_settings(render_settings);
	render_pipeline.render(view, resources.meshes(), true);
}

void TerrainRuntime::shutdown()
{
	for (auto& pending : pending_mesh_destroys)
		resources.meshes().destroy_mesh(pending.mesh);
	pending_mesh_destroys.clear();
	height_compute_uniform_set.reset();
	height_compute_output_buffer.reset();
	height_compute_params_buffer.reset();
	height_compute_pipeline = {};
	render_pipeline.shutdown();
	resources.shutdown();
	terrain_color_textures.clear();
}

void TerrainRuntime::configure_wsi()
{
	using VertexDataMode = Rendering::WSI::VERTEX_DATA_MODE;
	wsi->set_vertex_data_mode(static_cast<VertexDataMode>(0));
	wsi->set_index_buffer_format(Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32);
	wsi->create_new_vertex_format(
		wsi->get_default_vertex_attribute(),
		Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
}

void TerrainRuntime::create_scene_resources()
{
	regenerate_terrain_chunks();
}

void TerrainRuntime::regenerate_terrain_chunks()
{
	for (const auto& [_, chunk] : chunk_cache)
		queue_mesh_destroy(chunk.mesh);
	chunk_cache.clear();
	rebuild_chunk_window(true);
}

void TerrainRuntime::rebuild_chunk_window(bool discard_existing)
{
	std::vector<TerrainChunk> rebuilt_chunks;
	const int32_t radius = std::clamp(chunk_radius, 0, 8);

	// Build a square window centered on chunk_center. Reusing chunk_cache avoids regenerating/uploading
	// chunks that remain visible as the camera crosses chunk boundaries.
	for (int32_t z = -radius; z <= radius; ++z) {
		for (int32_t x = -radius; x <= radius; ++x) {
			const int32_t chunk_x = chunk_center.x + x;
			const int32_t chunk_z = chunk_center.y + z;
			const uint64_t key = chunk_key(chunk_x, chunk_z);
			TerrainChunk chunk;
			if (!discard_existing) {
				auto it = chunk_cache.find(key);
				if (it != chunk_cache.end())
					chunk = it->second;
			}
			if (chunk.mesh == Rendering::INVALID_MESH) {
				chunk = create_chunk(chunk_x, chunk_z);
				chunk_cache[key] = chunk;
			}
			rebuilt_chunks.push_back(chunk);
		}
	}

	pending_chunks = std::move(rebuilt_chunks);
	// Defer making the rebuilt chunk list visible for a frame so uploads and descriptor updates
	// have a chance to settle before render_frame starts submitting the new meshes.
	pending_chunks_ready = true;
	pending_chunk_frames_left = 1;
}

TerrainRuntime::TerrainChunk TerrainRuntime::create_chunk(int32_t x, int32_t z)
{
	const auto terrain_data = generate_terrain_chunk_mesh(terrain_settings, x, z);
	const std::string mesh_name =
		"terrain_chunk_" + std::to_string(terrain_mesh_generation++) +
		"_" + std::to_string(x) + "_" + std::to_string(z);
	const auto mesh = Rendering::Shapes::upload(
		resources.meshes(),
		mesh_name,
		terrain_data,
		wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT));
	return TerrainChunk{
		.x = x,
		.z = z,
		.mesh = mesh,
		.material = create_chunk_material(x, z),
	};
}

Rendering::MaterialHandle TerrainRuntime::create_chunk_material(int32_t x, int32_t z)
{
	Rendering::Material material;
	material.base_color_factor = glm::vec4(1.0f);
	material.roughness_factor = 0.95f;
	material.metallic_factor = 0.0f;
	material.diffuse = create_chunk_color_texture(x, z);
	return resources.materials().create(
		device,
		std::move(material),
		resources.default_white_texture(),
		render_pipeline.color_pipeline().shader_rid,
		render_pipeline.pbr_color_pipeline().shader_rid,
		render_pipeline.shadow_pipeline().shader_rid,
		render_pipeline.point_shadow_pipeline().shader_rid);
}

RID TerrainRuntime::create_chunk_color_texture(int32_t chunk_x, int32_t chunk_z)
{
	constexpr uint32_t texture_size = 128;
	const float step = terrain_settings.chunk_size / static_cast<float>(texture_size - 1u);
	const float half_chunk = terrain_settings.chunk_size * 0.5f;
	const float origin_x = static_cast<float>(chunk_x) * terrain_settings.chunk_size - half_chunk;
	const float origin_z = static_cast<float>(chunk_z) * terrain_settings.chunk_size - half_chunk;
	const float normal_sample_step = terrain_settings.chunk_size / static_cast<float>(terrain_settings.chunk_resolution);

	const glm::vec3 low_grass(0.10f, 0.23f, 0.11f);
	const glm::vec3 grass(0.26f, 0.45f, 0.18f);
	const glm::vec3 dry_grass(0.48f, 0.42f, 0.22f);
	const glm::vec3 rock(0.42f, 0.40f, 0.36f);
	const glm::vec3 snow(0.86f, 0.88f, 0.84f);

	Util::SmallVector<uint8_t> pixels;
	pixels.resize(static_cast<size_t>(texture_size) * static_cast<size_t>(texture_size) * 4u);

	for (uint32_t z = 0; z < texture_size; ++z) {
		for (uint32_t x = 0; x < texture_size; ++x) {
			const float world_x = origin_x + static_cast<float>(x) * step;
			const float world_z = origin_z + static_cast<float>(z) * step;
			const float h = sample_terrain_height(terrain_settings, world_x, world_z);

			const float h_l = sample_terrain_height(terrain_settings, world_x - normal_sample_step, world_z);
			const float h_r = sample_terrain_height(terrain_settings, world_x + normal_sample_step, world_z);
			const float h_d = sample_terrain_height(terrain_settings, world_x, world_z - normal_sample_step);
			const float h_u = sample_terrain_height(terrain_settings, world_x, world_z + normal_sample_step);
			const glm::vec3 normal = glm::normalize(glm::vec3(h_l - h_r, normal_sample_step * 2.0f, h_d - h_u));
			const float slope = 1.0f - glm::clamp(normal.y, 0.0f, 1.0f);

			// Derive an albedo palette from height and slope: low/flat areas are greener,
			// steep areas become rockier, and high elevations pick up snow.
			const float height01 = glm::clamp((h / glm::max(terrain_settings.height_scale, 0.001f) + 1.0f) * 0.5f, 0.0f, 1.0f);
			glm::vec3 color = glm::mix(low_grass, grass, glm::smoothstep(0.18f, 0.42f, height01));
			color = glm::mix(color, dry_grass, glm::smoothstep(0.45f, 0.68f, height01) * 0.35f);
			color = glm::mix(color, rock, glm::smoothstep(0.22f, 0.55f, slope));
			color = glm::mix(color, snow, glm::smoothstep(0.76f, 0.92f, height01));

			const size_t offset = (static_cast<size_t>(z) * texture_size + x) * 4u;
			pixels[offset + 0u] = static_cast<uint8_t>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
			pixels[offset + 1u] = static_cast<uint8_t>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
			pixels[offset + 2u] = static_cast<uint8_t>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
			pixels[offset + 3u] = 255u;
		}
	}

	Rendering::RenderingDeviceCommons::TextureFormat texture_format;
	texture_format.texture_type = Rendering::RenderingDeviceCommons::TEXTURE_TYPE_2D;
	texture_format.width = texture_size;
	texture_format.height = texture_size;
	texture_format.array_layers = 1;
	texture_format.usage_bits =
		Rendering::RenderingDeviceCommons::TEXTURE_USAGE_SAMPLING_BIT |
		Rendering::RenderingDeviceCommons::TEXTURE_USAGE_CAN_UPDATE_BIT;
	texture_format.format = Rendering::RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_SRGB;

	RID texture = device->texture_create(texture_format, Rendering::RenderingDevice::TextureView(), { pixels });
	device->set_resource_name(
		texture,
		"terrain_color_" + std::to_string(terrain_texture_generation++) +
		"_" + std::to_string(chunk_x) + "_" + std::to_string(chunk_z));
	terrain_color_textures.emplace_back(texture);
	return texture;
}

void TerrainRuntime::create_compute_resources()
{
	height_compute_pipeline = Rendering::ComputePipelineBuilder{}
		.set_shader("assets://shaders/terrain/height.comp", "terrain_height_compute")
		.build();
	if (!height_compute_pipeline.pipeline_rid.is_valid() || !height_compute_pipeline.shader_rid.is_valid())
		return;

	height_compute_params_buffer = RIDHandle(device->storage_buffer_create(sizeof(TerrainComputeParams)));
	if (!height_compute_params_buffer.is_valid())
		return;
	device->set_resource_name(height_compute_params_buffer, "terrain_height_compute_params");

	const uint32_t height_count = height_compute_texture_size * height_compute_texture_size;
	height_compute_output_buffer = RIDHandle(device->storage_buffer_create(height_count * sizeof(float)));
	if (!height_compute_output_buffer.is_valid())
		return;
	device->set_resource_name(height_compute_output_buffer, "terrain_height_compute_output");

	Rendering::RenderingDevice::Uniform params_uniform;
	params_uniform.uniform_type = Rendering::RenderingDeviceCommons::UNIFORM_TYPE_STORAGE_BUFFER;
	params_uniform.binding = 0;
	params_uniform.append_id(height_compute_params_buffer);

	Rendering::RenderingDevice::Uniform output_uniform;
	output_uniform.uniform_type = Rendering::RenderingDeviceCommons::UNIFORM_TYPE_STORAGE_BUFFER;
	output_uniform.binding = 1;
	output_uniform.append_id(height_compute_output_buffer);

	Util::SmallVector<Rendering::RenderingDevice::Uniform> uniforms;
	uniforms.push_back(params_uniform);
	uniforms.push_back(output_uniform);
	height_compute_uniform_set = RIDHandle(device->uniform_set_create(uniforms, height_compute_pipeline.shader_rid, 0));
}

void TerrainRuntime::dispatch_height_compute()
{
	if (!height_compute_enabled || !height_compute_pipeline.pipeline_rid.is_valid() || !height_compute_uniform_set.is_valid())
		return;

	const TerrainComputeParams params{
		.seed = terrain_settings.seed,
		.noise_type = static_cast<uint32_t>(terrain_settings.noise_type),
		.texture_size = height_compute_texture_size,
		.octaves = terrain_settings.octaves,
		.chunk_size = terrain_settings.chunk_size,
		.height_scale = terrain_settings.height_scale,
		.base_frequency = terrain_settings.base_frequency,
		.lacunarity = terrain_settings.lacunarity,
		.persistence = terrain_settings.persistence,
		.warp_strength = terrain_settings.warp_strength,
		.warp_strength2 = terrain_settings.warp_strength2,
		.chunk_x = static_cast<float>(chunk_center.x),
		.chunk_z = static_cast<float>(chunk_center.y),
	};

	device->buffer_update(height_compute_params_buffer, 0, sizeof(params), &params);

	Util::SmallVector<RID> sets;
	sets.push_back(height_compute_uniform_set);
	const uint32_t groups = (height_compute_texture_size + 7u) / 8u;
	device->compute_dispatch(height_compute_pipeline.pipeline_rid, sets, groups, groups, 1);
	++height_compute_dispatches;

	if (height_compute_validation_requested) {
		height_compute_validation_requested = false;
		validate_height_compute();
	}
}

void TerrainRuntime::validate_height_compute()
{
	const uint32_t height_count = height_compute_texture_size * height_compute_texture_size;
	const uint32_t byte_count = height_count * sizeof(float);
	const auto data = device->buffer_get_data(height_compute_output_buffer, 0, byte_count);
	if (data.size() != byte_count) {
		height_compute_validation_valid = false;
		return;
	}

	const float* gpu_heights = reinterpret_cast<const float*>(data.data());
	const float step = terrain_settings.chunk_size / static_cast<float>(height_compute_texture_size - 1u);
	const float half_chunk = terrain_settings.chunk_size * 0.5f;
	const float origin_x = static_cast<float>(chunk_center.x) * terrain_settings.chunk_size - half_chunk;
	const float origin_z = static_cast<float>(chunk_center.y) * terrain_settings.chunk_size - half_chunk;

	double total_error = 0.0;
	float max_error = 0.0f;
	for (uint32_t z = 0; z < height_compute_texture_size; ++z) {
		for (uint32_t x = 0; x < height_compute_texture_size; ++x) {
			const float world_x = origin_x + static_cast<float>(x) * step;
			const float world_z = origin_z + static_cast<float>(z) * step;
			const float cpu_height = sample_terrain_height(terrain_settings, world_x, world_z);
			const float error = std::abs(gpu_heights[z * height_compute_texture_size + x] - cpu_height);
			total_error += static_cast<double>(error);
			max_error = std::max(max_error, error);
		}
	}

	height_compute_max_error = max_error;
	height_compute_avg_error = static_cast<float>(total_error / static_cast<double>(height_count));
	height_compute_validation_valid = true;
}

void TerrainRuntime::create_water_resources()
{
	water_mesh = Rendering::Shapes::upload_plane(
		*wsi,
		resources.meshes(),
		1,
		"terrain_water_plane");

	Rendering::Material material;
	material.base_color_factor = glm::vec4(0.12f, 0.32f, 0.46f, 1.0f);
	material.roughness_factor = 0.18f;
	material.metallic_factor = 0.0f;
	water_material = resources.materials().create(
		device,
		std::move(material),
		resources.default_white_texture(),
		render_pipeline.color_pipeline().shader_rid,
		render_pipeline.pbr_color_pipeline().shader_rid,
		render_pipeline.shadow_pipeline().shader_rid,
		render_pipeline.point_shadow_pipeline().shader_rid);
}

void TerrainRuntime::update_streaming_chunks()
{
	if (!stream_chunks)
		return;

	const float chunk_size = terrain_settings.chunk_size;
	if (chunk_size <= 0.0f)
		return;

	const auto position = camera.get_position();
	const glm::ivec2 camera_chunk(
		static_cast<int32_t>(std::floor(position.x / chunk_size)),
		static_cast<int32_t>(std::floor(position.z / chunk_size)));

	if (camera_chunk == chunk_center)
		return;

	chunk_center = camera_chunk;
	rebuild_chunk_window(false);
	prune_chunk_cache();
}

void TerrainRuntime::prune_chunk_cache()
{
	const int32_t keep_radius = std::clamp(chunk_radius + chunk_cache_margin, chunk_radius, 16);
	for (auto it = chunk_cache.begin(); it != chunk_cache.end();) {
		const glm::ivec2 coord = decode_chunk_key(it->first);
		const glm::ivec2 delta = glm::abs(coord - chunk_center);
		if (delta.x > keep_radius || delta.y > keep_radius) {
			// Mesh destruction is delayed because the previous visible chunk list may still be referenced by GPU work.
			queue_mesh_destroy(it->second.mesh);
			it = chunk_cache.erase(it);
		}
		else {
			++it;
		}
	}
}

void TerrainRuntime::queue_mesh_destroy(Rendering::MeshHandle mesh)
{
	if (mesh == Rendering::INVALID_MESH)
		return;

	pending_mesh_destroys.push_back(PendingMeshDestroy{
		.mesh = mesh,
		.frames_left = 5,
	});
}

void TerrainRuntime::process_pending_mesh_destroys()
{
	for (auto it = pending_mesh_destroys.begin(); it != pending_mesh_destroys.end();) {
		if (it->frames_left > 0) {
			--it->frames_left;
			++it;
			continue;
		}

		resources.meshes().destroy_mesh(it->mesh);
		it = pending_mesh_destroys.erase(it);
	}
}

void TerrainRuntime::process_deferred_requests()
{
	if (pending_chunks_ready) {
		if (pending_chunk_frames_left > 0) {
			--pending_chunk_frames_left;
			return;
		}
		// Swap the entire visible chunk list at once so render_frame never sees a partially rebuilt window.
		chunks = std::move(pending_chunks);
		pending_chunks.clear();
		pending_chunks_ready = false;
	}

	if (regenerate_requested) {
		regenerate_requested = false;
		regenerate_terrain_chunks();
	}
}

void TerrainRuntime::draw_ui()
{
	ImGui::Begin("Terrain");

	int noise_type = static_cast<int>(terrain_settings.noise_type);
	if (ImGui::Combo("Noise Type", &noise_type, "Value\0Perlin\0"))
		terrain_settings.noise_type = static_cast<NoiseType>(noise_type);

	int seed = static_cast<int>(terrain_settings.seed);
	if (ImGui::InputInt("Seed", &seed))
		terrain_settings.seed = static_cast<uint32_t>(std::max(seed, 0));

	float warp_strength = static_cast<int>(terrain_settings.warp_strength);
	if (ImGui::InputFloat("Warp Strength", &warp_strength))
		terrain_settings.warp_strength = static_cast<uint32_t>(warp_strength);

	float warp_strength2 = static_cast<int>(terrain_settings.warp_strength2);
	if (ImGui::InputFloat("Warp Strength 2", &warp_strength2))
		terrain_settings.warp_strength2 = static_cast<uint32_t>(warp_strength2);

	int resolution = static_cast<int>(terrain_settings.chunk_resolution);
	if (ImGui::SliderInt("Resolution", &resolution, 8, 256))
		terrain_settings.chunk_resolution = static_cast<uint32_t>(resolution);

	ImGui::DragFloat("Chunk Size", &terrain_settings.chunk_size, 0.25f, 8.0f, 400.0f);
	int radius = chunk_radius;
	if (ImGui::SliderInt("Chunk Radius", &radius, 0, 8))
		chunk_radius = radius;
	ImGui::SliderInt("Cache Margin", &chunk_cache_margin, 0, 8);
	if (ImGui::Checkbox("Stream Chunks", &stream_chunks)) {
		if (!stream_chunks) {
			chunk_center = glm::ivec2(0);
		}
		else if (terrain_settings.chunk_size > 0.0f) {
			const auto position = camera.get_position();
			chunk_center = glm::ivec2(
				static_cast<int32_t>(std::floor(position.x / terrain_settings.chunk_size)),
				static_cast<int32_t>(std::floor(position.z / terrain_settings.chunk_size)));
		}
		regenerate_terrain_chunks();
	}
	ImGui::DragFloat("Height", &terrain_settings.height_scale, 0.1f, 0.0f, 80.0f);
	ImGui::DragFloat("Frequency", &terrain_settings.base_frequency, 0.001f, 0.001f, 0.25f, "%.3f");

	int octaves = static_cast<int>(terrain_settings.octaves);
	if (ImGui::SliderInt("Octaves", &octaves, 1, 8))
		terrain_settings.octaves = static_cast<uint32_t>(octaves);

	ImGui::DragFloat("Lacunarity", &terrain_settings.lacunarity, 0.01f, 1.0f, 4.0f);
	ImGui::DragFloat("Persistence", &terrain_settings.persistence, 0.01f, 0.0f, 1.0f);

	if (ImGui::Button("Regenerate"))
		regenerate_requested = true;

	ImGui::Separator();
	ImGui::Checkbox("GPU Height Compute", &height_compute_enabled);
	if (ImGui::Button("Validate GPU Heights"))
		height_compute_validation_requested = true;
	ImGui::Text("Compute dispatches: %llu", static_cast<unsigned long long>(height_compute_dispatches));
	if (height_compute_validation_valid)
		ImGui::Text("Height error avg %.4f max %.4f", height_compute_avg_error, height_compute_max_error);
	else
		ImGui::Text("Height validation: not run");

	ImGui::Separator();
	ImGui::Checkbox("PBR", &render_settings.use_pbr_lighting);
	ImGui::Checkbox("Grid", &render_settings.draw_grid);
	ImGui::Checkbox("Skybox", &render_settings.draw_skybox);
	ImGui::Checkbox("Water", &water_enabled);
	ImGui::DragFloat("Water Level", &water_level, 0.1f, -40.0f, 80.0f);
	ImGui::SliderInt("Water Padding", &water_padding_chunks, 0, 64);
	ImGui::SliderFloat("Water Forward Bias", &water_forward_bias, 0.0f, 0.8f);
	ImGui::DragFloat("Water Min Size", &water_min_diameter, 10.0f, 80.0f, 2000.0f);
	ImGui::SliderFloat("Water Depth Scale", &water_depth_scale, 1.0f, 6.0f);
	int shadow_mode = static_cast<int>(render_settings.directional_shadow_mode);
	if (ImGui::Combo("Directional Shadow", &shadow_mode, "Single Map\0Cascaded\0"))
		render_settings.directional_shadow_mode = static_cast<DirectionalShadowMode>(shadow_mode);

	int debug_view = static_cast<int>(render_settings.material_debug_view);
	if (ImGui::BeginCombo("Debug", material_debug_view_name(render_settings.material_debug_view))) {
		for (int i = 0; i <= static_cast<int>(MaterialDebugView::CascadeBoundaries); ++i) {
			const auto value = static_cast<MaterialDebugView>(i);
			const bool selected = debug_view == i;
			if (ImGui::Selectable(material_debug_view_name(value), selected)) {
				debug_view = i;
				render_settings.material_debug_view = value;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if ((Util::rdoc != nullptr) && (ImGui::BeginMenu("Ops")))
	{
		if (ImGui::Button("Capture Rdoc Frame"))
		{
			Util::capture_next_frame();
		}

		rdoc_frame_captured = Util::get_rdoc_num_captures();

		if (ImGui::Button(std::format("Open Rdoc Capture {}", rdoc_frame_captured).c_str()))            // more optimzed way to handle dynamic string?
		{
			if (rdoc_frame_captured > 0)
			{
				Util::open_last_captured();
			}
		}
		ImGui::EndMenu();
	}

	ImGui::Text("Palette: height + slope");

	ImGui::End();
}

} // namespace Terrain
