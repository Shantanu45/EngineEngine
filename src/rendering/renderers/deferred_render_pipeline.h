#pragma once

#include "rendering/fg/blackboard.h"
#include "rendering/fg/frame_graph.h"
#include "deferred_renderer.h"

namespace Rendering {

	class DeferredRenderPipeline {
	public:
		void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);
		void shutdown();

		void render(const SceneView& view, MeshStorage& storage, bool include_imgui_pass = true);

		Pipeline color_pipeline() const { return renderer.color_pipeline(); }
		Pipeline pbr_color_pipeline() const { return renderer.pbr_color_pipeline(); }
		Pipeline shadow_pipeline() const { return renderer.shadow_pipeline(); }
		Pipeline point_shadow_pipeline() const { return renderer.point_shadow_pipeline(); }

	private:
		void schedule_passes(const SceneView& view, MeshStorage& storage, bool include_imgui_pass);

		WSI* wsi = nullptr;
		RenderingDevice* device = nullptr;
		FrameGraph frame_graph;
		FrameGraphBlackboard blackboard;
		DeferredRenderer renderer;
	};

} // namespace Rendering
