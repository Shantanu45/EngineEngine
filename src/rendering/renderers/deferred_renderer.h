#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/pipeline_builder.h"
#include "rendering/uniform_buffer.h"
#include "rendering/rid_handle.h"
#include "rendering/frame_data.h"
#include "rendering/light.h"
#include "rendering/camera.h"
#include "rendering/render_passes/common.h"

#include <glm/glm.hpp>

struct offscreen_pass_resource
{
	FrameGraphResource framebuffer_resource;
	FrameGraphResource position_resource;
	FrameGraphResource albedo_resource;
	FrameGraphResource normal_resource;
	FrameGraphResource depth_resource;
};

struct deferred_pass_resource : public blit_scene_input_resource
{
	FrameGraphResource framebuffer_resource;
	FrameGraphResource offscreen_tex_resources;
};

namespace Rendering {

	class WSI;
	class RenderingDevice;

	class DeferredRenderer : public IRenderer
	{
	public:
		void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);

		// IRenderer — uploads per-frame UBOs and schedules all passes.
		void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
		                  const SceneView& view, MeshStorage& storage) override;

		// Expose the offscreen pipeline so callers can obtain shader_rid for material creation.
		const Pipeline& color_pipeline() const { return offscreen_pipeline; }

	private:
		void setup_offscreen_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		                          const SceneView& view, MeshStorage& storage);
		void setup_deferred_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		                         const SceneView& view, MeshStorage& storage);

		void create_offscreen_pipeline(WSI* wsi, RenderingDevice* device);
		void create_deferred_pipeline(WSI* wsi, RenderingDevice* device);

		RenderingDevice* device = nullptr;

		Pipeline offscreen_pipeline;
		Pipeline deferred_pipeline;

		// UBOs — declared first so they outlive the uniform sets that reference them.
		UniformBuffer<FrameData_UBO>   frame_ubo;
		UniformBuffer<LightBuffer_UBO> light_ubo;
		UniformBuffer<PointShadow_UBO> point_shadow_ubo;

		RIDHandle sampler;

		RIDHandle uniform_set_0;
		RIDHandle uniform_set_0_deferred;
	};
}
