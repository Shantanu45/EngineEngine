#include "terrain/terrain_runtime.h"

#include "imgui.h"
#include "rendering/primitve_shapes.h"
#include "rendering/utils.h"
#include "util/profiler.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace Terrain {
namespace {

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
	}
	return "Unknown";
}

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
	create_scene_resources();

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
	camera.update_from_input(input_system.get(), frame_time);
	device->imgui_begin_frame();
	draw_ui();
	resources.materials().upload_dirty(device);

	Rendering::SceneView view;
	view.camera.view = camera.get_view();
	view.camera.proj = camera.get_projection();
	view.camera.cameraPos = camera.get_position();
	view.elapsed = elapsed_time;
	view.extent = { device->screen_get_width(), device->screen_get_height() };
	view.use_pbr_lighting = render_settings.use_pbr_lighting;
	view.material_debug_view = render_settings.material_debug_view;
	view.skybox_mesh = render_settings.draw_skybox ? skybox_mesh : Rendering::INVALID_MESH;
	view.grid_mesh = render_settings.draw_grid ? grid_mesh : Rendering::INVALID_MESH;

	const glm::mat4 model(1.0f);
	view.instances.push_back(Rendering::MeshInstance{
		.mesh = terrain_mesh,
		.model = model,
		.normal_matrix = glm::transpose(glm::inverse(model)),
		.material_sets = { resources.materials().get_uniform_set(terrain_material, render_settings.use_pbr_lighting) },
		.shadow_material_sets = { resources.materials().get_shadow_uniform_set(terrain_material) },
		.point_shadow_material_sets = { resources.materials().get_point_shadow_uniform_set(terrain_material) },
		.category = Rendering::MeshCategory::Opaque,
	});

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
	render_pipeline.shutdown();
	resources.shutdown();
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
	regenerate_terrain_mesh();
	create_terrain_material();
}

void TerrainRuntime::create_terrain_material()
{
	Rendering::Material material;
	material.base_color_factor = glm::vec4(0.32f, 0.58f, 0.28f, 1.0f);
	material.roughness_factor = 0.85f;
	material.metallic_factor = 0.0f;
	terrain_material = resources.materials().create(
		device,
		std::move(material),
		resources.default_white_texture(),
		render_pipeline.color_pipeline().shader_rid,
		render_pipeline.pbr_color_pipeline().shader_rid,
		render_pipeline.shadow_pipeline().shader_rid,
		render_pipeline.point_shadow_pipeline().shader_rid);
}

void TerrainRuntime::regenerate_terrain_mesh()
{
	const auto terrain_data = generate_terrain_mesh(terrain_settings);
	terrain_mesh = Rendering::Shapes::upload(
		resources.meshes(),
		"terrain_preview_mesh_" + std::to_string(terrain_mesh_generation++),
		terrain_data,
		wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT));
	wsi->submit_transfer_workers();
}

void TerrainRuntime::draw_ui()
{
	ImGui::Begin("Terrain");

	int seed = static_cast<int>(terrain_settings.seed);
	if (ImGui::InputInt("Seed", &seed))
		terrain_settings.seed = static_cast<uint32_t>(std::max(seed, 0));

	int resolution = static_cast<int>(terrain_settings.resolution);
	if (ImGui::SliderInt("Resolution", &resolution, 8, 256))
		terrain_settings.resolution = static_cast<uint32_t>(resolution);

	ImGui::DragFloat("Size", &terrain_settings.size, 0.25f, 8.0f, 400.0f);
	ImGui::DragFloat("Height", &terrain_settings.height_scale, 0.1f, 0.0f, 80.0f);
	ImGui::DragFloat("Frequency", &terrain_settings.base_frequency, 0.001f, 0.001f, 0.25f, "%.3f");

	int octaves = static_cast<int>(terrain_settings.octaves);
	if (ImGui::SliderInt("Octaves", &octaves, 1, 8))
		terrain_settings.octaves = static_cast<uint32_t>(octaves);

	ImGui::DragFloat("Lacunarity", &terrain_settings.lacunarity, 0.01f, 1.0f, 4.0f);
	ImGui::DragFloat("Persistence", &terrain_settings.persistence, 0.01f, 0.0f, 1.0f);

	if (ImGui::Button("Regenerate"))
		regenerate_terrain_mesh();

	ImGui::Separator();
	ImGui::Checkbox("PBR", &render_settings.use_pbr_lighting);
	ImGui::Checkbox("Grid", &render_settings.draw_grid);
	ImGui::Checkbox("Skybox", &render_settings.draw_skybox);

	int debug_view = static_cast<int>(render_settings.material_debug_view);
	if (ImGui::BeginCombo("Debug", material_debug_view_name(render_settings.material_debug_view))) {
		for (int i = 0; i <= static_cast<int>(MaterialDebugView::Depth); ++i) {
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

	if (terrain_material != Rendering::INVALID_MATERIAL) {
		auto& material = resources.materials().get(terrain_material);
		glm::vec3 color = glm::vec3(material.base_color_factor);
		if (ImGui::ColorEdit3("Color", &color.x)) {
			material.base_color_factor = glm::vec4(color, material.base_color_factor.a);
			material.dirty = true;
		}
		if (ImGui::SliderFloat("Roughness", &material.roughness_factor, 0.04f, 1.0f))
			material.dirty = true;
	}

	ImGui::End();
}

} // namespace Terrain
