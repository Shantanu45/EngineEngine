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
		void initialize(WSI* wsi, RenderingDevice* debive, RID cubemap);
		void upload_frame_data(RenderingDevice* device, const Camera& camera, double elapsed, const glm::mat4& light_space_matrix);
		void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage) override;

	private:
		Pipeline deferred_pipeline;

		// UBOs — declared first so they outlive the uniform sets that reference them.
		UniformBuffer<FrameData_UBO>  frame_ubo;
		UniformBuffer<LightBuffer_UBO>    light_ubo;
		UniformBuffer<PointShadow_UBO> point_shadow_ubo;

		RIDHandle sampler;

		RIDHandle uniform_set_0;

	};
}
