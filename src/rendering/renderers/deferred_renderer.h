#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/pipeline_builder.h"
#include "rendering/uniform_buffer.h"
#include "rendering/rid_handle.h"
#include "rendering/frame_data.h"
#include "rendering/light.h"
#include "rendering/camera.h"

#include <glm/glm.hpp>

namespace Rendering {

	class WSI;
	class RenderingDevice;

	class DeferredRenderer : public IRenderer
	{

	public:
		void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage) override;

	};
}
