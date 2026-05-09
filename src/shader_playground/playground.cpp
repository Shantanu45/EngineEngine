#include "application/common.h"

#include "application/service_locator.h"
#include "filesystem/filesystem.h"
#include "input/input.h"
#include "shader_playground/playground_runtime.h"

struct PlaygroundParams {
	glm::vec4 tint = glm::vec4(1.0f);
	glm::vec4 controls = glm::vec4(1.0f, 0.35f, 0.0f, 0.0f);
};

struct ShaderPlaygroundApplication : EE::Application
{
	bool pre_frame() override
	{
		auto input_system = Services::get().get<EE::InputSystemInterface>();
		auto fs_ptr = Services::get().get<FileSystem::FilesystemInterface>();
		auto& filesystem = static_cast<FileSystem::Filesystem&>(*fs_ptr);

		if (!runtime.initialize(ShaderPlayground::PlaygroundRuntimeConfig{
			.wsi = get_wsi(),
			.filesystem = filesystem,
			.input_system = input_system,
		})) {
			return false;
		}

		auto& scene = runtime.scene();
		scene.camera(glm::vec3(0.0f, 0.7f, 2.4f), glm::vec3(-12.0f, 0.0f, 0.0f));

		scene.fullscreen("assets://shaders/shader_playground/fullscreen_quad.frag");

		scene.cube(
			"assets://shaders/shader_playground/unlit.vert",
			"assets://shaders/shader_playground/unlit.frag")
			.position(glm::vec3(0.0f, 0.0f, 0.0f))
			.rotation(glm::vec3(0.0f, 30.0f, 0.0f))
			.scale(glm::vec3(0.75f))
			.texture(0, "assets://textures/stone-granite.png")
			.ubo(1, PlaygroundParams{});

		return true;
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
	ShaderPlayground::PlaygroundRuntime runtime;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try {
			return new ShaderPlaygroundApplication();
		}
		catch (const std::exception& e) {
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}
