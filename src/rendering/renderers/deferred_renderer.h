#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/renderers/debug_renderer.h"
#include "rendering/drawable.h"
#include "rendering/pipeline_builder.h"
#include "rendering/uniform_buffer.h"
#include "rendering/rid_handle.h"
#include "rendering/frame_data.h"
#include "rendering/light.h"
#include "rendering/camera.h"
#include "rendering/render_passes/common.h"

#include <glm/glm.hpp>
#include <vector>
#include "util/small_vector.h"

struct offscreen_pass_resource
{
	FrameGraphResource framebuffer_resource;
	FrameGraphResource position_resource;
	FrameGraphResource albedo_resource;
	FrameGraphResource normal_resource;
	FrameGraphResource material_resource;
	FrameGraphResource emissive_resource;
	FrameGraphResource depth_resource;
};

struct deferred_pass_resource : public blit_scene_input_resource
{
	FrameGraphResource framebuffer_resource;
	FrameGraphResource offscreen_tex_resources;
	FrameGraphResource shadow_map_in;
	FrameGraphResource point_shadow_in;
	FrameGraphResource shadow_uniform_set;
};

namespace Rendering {

	class WSI;
	class RenderingDevice;

	class DeferredRenderer : public IRenderer
	{
	public:
		void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);
		void shutdown();

		// IRenderer — uploads per-frame UBOs and schedules all passes.
		void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
		                  const SceneView& view, MeshStorage& storage) override;

		// Expose the color pipeline so callers can obtain shader_rid for material creation.
		Pipeline color_pipeline() const { return pipeline_color; }
		Pipeline pbr_color_pipeline() const { return pipeline_pbr; }
		Pipeline shadow_pipeline() const { return pipeline_shadow; }
		Pipeline point_shadow_pipeline() const { return pipeline_point_shadow; }

	private:
		void setup_offscreen_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		                          const SceneView& view, MeshStorage& storage);
		void setup_deferred_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		                         const SceneView& view, MeshStorage& storage);
		void setup_overlay_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		                        const SceneView& view, MeshStorage& storage);

		void create_offscreen_pipeline(WSI* wsi, RenderingDevice* device);
		void create_deferred_pipeline(WSI* wsi, RenderingDevice* device, RID cubemap);

	private:
		std::vector<Drawable> build_overlay_drawables(const SceneView& view) const;

		RenderingDevice* device = nullptr;
		DebugRenderer debug_renderer;

		// UBOs — declared first so they outlive the uniform sets that reference them.
		UniformBuffer<FrameData_UBO>   frame_ubo;
		UniformBuffer<LightBuffer_UBO> light_ubo;
		UniformBuffer<ShadowBuffer_UBO> shadow_ubo;

		// Samplers
		RIDHandle sampler;
		RIDHandle sampler_cube;
		RIDHandle shadow_sampler;
		RIDHandle point_shadow_sampler;

		// Pipelines
		Pipeline pipeline_color;			// offscreen
		Pipeline pipeline_pbr;				// offscreen pbr			
		Pipeline pipeline_light;
		Pipeline pipeline_grid;
		Pipeline pipeline_shadow;
		Pipeline pipeline_point_shadow;
		Pipeline pipeline_skybox;

		Pipeline deferred_pipeline;

		// Uniform sets — declared last so they are destroyed first.
		RIDHandle uniform_set_0;
		RIDHandle uniform_set_0_pbr;
		RIDHandle uniform_set_0_light;
		RIDHandle uniform_set_0_shadow;
		RIDHandle uniform_set_0_point_shadow;
		RIDHandle uniform_set_skybox;

		RIDHandle uniform_set_0_deferred;
	};
}
