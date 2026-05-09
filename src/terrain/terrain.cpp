#include "application/common.h"

#include "application/application_options.h"
#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/aabb.h"
#include "rendering/camera.h"
#include "rendering/material.h"
#include "rendering/primitve_shapes.h"
#include "rendering/render_resource_store.h"
#include "rendering/renderers/forward_render_pipeline.h"
#include "rendering/utils.h"
#include "terrain/terrain_generator.h"
#include "util/profiler.h"

#include <array>
#include <cstring>
#include <utility>

namespace {

struct TerrainRuntime {
	bool initialize(Rendering::WSI* wsi_, FileSystem::Filesystem& filesystem, std::shared_ptr<EE::InputSystemInterface> input_system_)
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

		render_pipeline.initialize(wsi, device, resources.skybox_cubemap());
		create_scene_resources();

		camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.set_reset_on_resize(true);
		camera.set_mode(CameraMode::Fly);
		camera.set_position(glm::vec3(0.0f, 20.0f, 55.0f));
		camera.set_euler_degrees(-20.0f, 180.0f, 0.0f);

		wsi->submit_transfer_workers();
		return wsi->pre_frame_loop();
	}

	void render_frame(double frame_time, double elapsed_time)
	{
		ZoneScoped;
		camera.update_from_input(input_system.get(), frame_time);
		device->imgui_begin_frame();

		Rendering::SceneView view;
		view.camera.view = camera.get_view();
		view.camera.proj = camera.get_projection();
		view.camera.cameraPos = camera.get_position();
		view.elapsed = elapsed_time;
		view.extent = { device->screen_get_width(), device->screen_get_height() };
		view.use_pbr_lighting = true;
		view.skybox_mesh = skybox_mesh;
		view.grid_mesh = grid_mesh;

		const glm::mat4 model(1.0f);
		view.instances.push_back(Rendering::MeshInstance{
			.mesh = terrain_mesh,
			.model = model,
			.normal_matrix = glm::transpose(glm::inverse(model)),
			.material_sets = { resources.materials().get_uniform_set(terrain_material, true) },
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

		wsi->set_render_settings(RenderSettings{});
		render_pipeline.render(view, resources.meshes(), true);
	}

	void shutdown()
	{
		render_pipeline.shutdown();
		resources.shutdown();
	}

private:
	void configure_wsi()
	{
		using VertexDataMode = Rendering::WSI::VERTEX_DATA_MODE;
		wsi->set_vertex_data_mode(static_cast<VertexDataMode>(0));
		wsi->set_index_buffer_format(Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32);
		wsi->create_new_vertex_format(
			wsi->get_default_vertex_attribute(),
			Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
	}

	void create_scene_resources()
	{
		const Terrain::TerrainSettings settings;
		const auto terrain_data = Terrain::generate_terrain_mesh(settings);
		terrain_mesh = Rendering::Shapes::upload(
			resources.meshes(),
			"terrain_preview_mesh",
			terrain_data,
			wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT));

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

	Rendering::WSI* wsi = nullptr;
	Rendering::RenderingDevice* device = nullptr;
	std::shared_ptr<EE::InputSystemInterface> input_system;

	Camera camera;
	Rendering::RenderResourceStore resources;
	Rendering::ForwardRenderPipeline render_pipeline;
	Rendering::MeshHandle terrain_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;
	Rendering::MaterialHandle terrain_material = Rendering::INVALID_MATERIAL;
};

struct TerrainApplication : EE::Application {
	bool pre_frame() override
	{
		auto input_system = Services::get().get<EE::InputSystemInterface>();
		auto fs_ptr = Services::get().get<FileSystem::FilesystemInterface>();
		auto& filesystem = static_cast<FileSystem::Filesystem&>(*fs_ptr);
		return runtime.initialize(get_wsi(), filesystem, input_system);
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
		runtime.render_frame(frame_time, elapsed_time);
	}

	void teardown_application() override
	{
		runtime.shutdown();
	}

	std::string get_name() override
	{
		return "terrain";
	}

private:
	TerrainRuntime runtime;
};

} // namespace

namespace EE {

Application* application_create(int, char**)
{
	EE_APPLICATION_SETUP;

	try {
		return new TerrainApplication();
	}
	catch (const std::exception& e) {
		LOGE("application_create() threw exception: %s\n", e.what());
		return nullptr;
	}
}

} // namespace EE
