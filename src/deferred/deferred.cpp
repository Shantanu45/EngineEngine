#include "application/common.h"

#include "rendering/renderers/deferred_renderer.h"
#include "rendering/frame_data.h"
#include "rendering/camera.h"
#include "rendering/primitve_shapes.h"
#include "rendering/drawable.h"
#include "rendering/default_textures.h"
#include "rendering/material.h"
#include "rendering/render_passes/common.h"
#include "input/input.h"
#include "util/timer.h"
#include "tutorial/scene/components.h"
#include "entt/entt.hpp"
#include "util/small_vector.h"

struct DeferredApp : EE::Application
{
	bool pre_frame() override
	{
		input_system = Services::get().get<EE::InputSystemInterface>();

		camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.set_reset_on_resize();
		camera.set_mode(CameraMode::Fly);

		wsi    = get_wsi();
		device = wsi->get_rendering_device();

		wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
		wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);
		wsi->create_new_vertex_format(
			wsi->get_default_vertex_attribute(),
			Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

		mesh_storage->initialize(device);

		cube_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "cube");

		fallback_texture = Rendering::create_white_texture(device);

		renderer.initialize(wsi, device, RID{});

		// Simple orange material — all texture slots use the fallback white texture.
		Rendering::Material mat;
		mat.base_color_factor = glm::vec4(0.8f, 0.5f, 0.2f, 1.0f);
		mat.shininess         = 32.0f;
		mat_handle = material_registry.create(
			device, std::move(mat), fallback_texture, renderer.color_pipeline().shader_rid);

		// 3x3 grid of cubes
		for (int x = 0; x < 3; x++) {
			for (int z = 0; z < 3; z++) {
				auto e = world.create();
				world.emplace<TransformComponent>(e, TransformComponent{
					.position = glm::vec3((x - 1) * 2.5f, 0.5f, (z - 1) * 2.5f) });
				world.emplace<MeshComponent>(e, MeshComponent{
					.mesh      = cube_mesh,
					.materials = { mat_handle },
				});
			}
		}

		wsi->submit_transfer_workers();
		return wsi->pre_frame_loop();
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
		camera.update_from_input(input_system.get(), frame_time);

		device->imgui_begin_frame();
		const auto timer = Services::get().get<Util::FrameTimer>();
		ImGui::Text("FPS: %.1f", timer->get_fps());
		ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);

		Rendering::SceneView view;
		view.camera.view = camera.get_view();
		view.camera.proj = camera.get_projection();
		view.camera.cameraPos = camera.get_position();
		view.elapsed   = elapsed_time;
		view.extent    = { device->screen_get_width(), device->screen_get_height() };
		view.instances = build_main_instances();

		fg.reset();
		bb.reset();

		renderer.setup_passes(fg, bb, view, *mesh_storage);
		Rendering::add_imgui_pass(fg, bb, view.extent);
		Rendering::add_blit_pass(fg, bb, bb.get<deferred_pass_resource>());

		fg.compile();

		Rendering::RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device         = device;
		rc.wsi            = wsi;
		fg.execute(&rc, &rc);
	}

	void teardown_application() override
	{
		material_registry.free_all(device);
		mesh_storage->finalize();
	}

private:
	Util::SmallVector<Rendering::MeshInstance> build_main_instances()
	{
		material_registry.upload_dirty(device);
		Util::SmallVector<Rendering::MeshInstance> out;

		world.view<TransformComponent, MeshComponent>().each(
			[&](auto, TransformComponent& t, MeshComponent& m) {
				Util::SmallVector<RID> mat_sets;
				for (auto h : m.materials)
					mat_sets.push_back(h != Rendering::INVALID_MATERIAL
						? material_registry.get_uniform_set(h) : RID());

				out.push_back(Rendering::MeshInstance{
					.mesh          = m.mesh,
					.model         = t.get_model(),
					.normal_matrix = t.get_normal_matrix(),
					.material_sets = std::move(mat_sets),
					.category      = m.category,
				});
			});

		return out;
	}

	// Declared in destruction-safe order: textures first, then things that reference them.
	RIDHandle                   fallback_texture;
	Rendering::MaterialRegistry material_registry;

	entt::registry world;
	std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();
	Rendering::MeshHandle     cube_mesh;
	Rendering::MaterialHandle mat_handle = Rendering::INVALID_MATERIAL;

	Camera camera;
	std::shared_ptr<EE::InputSystemInterface> input_system;

	Rendering::WSI*             wsi    = nullptr;
	Rendering::RenderingDevice* device = nullptr;

	FrameGraph           fg;
	FrameGraphBlackboard bb;

	// Declared last — renderer uniform sets reference fallback_texture.
	Rendering::DeferredRenderer renderer;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try {
			return new DeferredApp();
		}
		catch (const std::exception& e) {
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}
