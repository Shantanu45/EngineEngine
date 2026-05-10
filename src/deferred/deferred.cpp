#include "application/common.h"

#include "application/application_options.h"
#include "filesystem/filesystem.h"
#include "deferred/deferred_runtime.h"

struct DeferredApp : EE::Application
{
	bool pre_frame() override
	{
		auto input_system = Services::get().get<EE::InputSystemInterface>();
		auto fs_ptr = Services::get().get<FileSystem::FilesystemInterface>();
		auto& filesystem = static_cast<FileSystem::Filesystem&>(*fs_ptr);
		bool use_pbr_lighting = true;

		if (Services::get().has<EE::AppOptions>()) {
			auto options = Services::get().get<EE::AppOptions>();
			use_pbr_lighting = options->render_mode == "pbr";
		}

		return runtime.initialize(DeferredRuntimeConfig{
			.wsi = get_wsi(),
			.filesystem = filesystem,
			.input_system = input_system,
			.scene_path = "assets://gltf/Sponza/glTF/Sponza.gltf",
			.scene_name_prefix = "sponza",
			.skybox_faces = {
				"assets://textures/skybox/right.jpg",
				"assets://textures/skybox/left.jpg",
				"assets://textures/skybox/top.jpg",
				"assets://textures/skybox/bottom.jpg",
				"assets://textures/skybox/front.jpg",
				"assets://textures/skybox/back.jpg",
			},
			.camera = DeferredCameraConfig{
				.fov_degrees = 60.0f,
				.aspect = 16.0f / 9.0f,
				.near_plane = 0.1f,
				.far_plane = 1000.0f,
				.mode = CameraMode::Fly,
				.reset_aspect_on_resize = true,
			},
			.enable_default_lights = true,
			.enable_default_ui = true,
			.use_pbr_lighting = use_pbr_lighting,
			});
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
		runtime.render_frame(frame_time, elapsed_time);
	}

	void teardown_application() override
	{
		runtime.shutdown();
	}

private:
	DeferredRuntime runtime;
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
