#include "shader_playground/playground_runtime.h"

namespace ShaderPlayground {

bool PlaygroundRuntime::initialize(const PlaygroundRuntimeConfig& config)
{
	return playground_scene.initialize(config.wsi, config.filesystem);
}

void PlaygroundRuntime::render_frame(double frame_time, double elapsed_time)
{
	playground_scene.render(frame_time, elapsed_time);
}

void PlaygroundRuntime::shutdown()
{
	playground_scene.shutdown();
}

} // namespace ShaderPlayground
