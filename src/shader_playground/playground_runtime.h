#pragma once

#include <memory>

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/wsi.h"
#include "shader_playground/playground_scene.h"

namespace ShaderPlayground {

struct PlaygroundRuntimeConfig {
	Rendering::WSI* wsi = nullptr;
	FileSystem::Filesystem& filesystem;
	std::shared_ptr<EE::InputSystemInterface> input_system;
};

class PlaygroundRuntime {
public:
	bool initialize(const PlaygroundRuntimeConfig& config);
	PlaygroundScene& scene() { return playground_scene; }
	void render_frame(double frame_time, double elapsed_time);
	void shutdown();

private:
	PlaygroundScene playground_scene;
};

} // namespace ShaderPlayground
