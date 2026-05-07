#include "application/common.h"

#include "filesystem/filesystem.h"
#include "forward/forward_runtime.h"
#include "input/input.h"

struct ForwardApplication : EE::Application
{
	bool pre_frame() override
	{
		auto input_system = Services::get().get<EE::InputSystemInterface>();
		auto fs_ptr = Services::get().get<FileSystem::FilesystemInterface>();
		auto& filesystem = static_cast<FileSystem::Filesystem&>(*fs_ptr);

		return runtime.initialize(ForwardRuntimeConfig{
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
	ForwardRuntime runtime;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try {
			return new ForwardApplication();
		}
		catch (const std::exception& e) {
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}
