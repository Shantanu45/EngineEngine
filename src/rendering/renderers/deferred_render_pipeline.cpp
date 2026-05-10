#include "deferred_render_pipeline.h"

#include "rendering/render_passes/common.h"
#include "rendering/utils.h"
#include "util/profiler.h"

namespace Rendering {

	void DeferredRenderPipeline::initialize(WSI* wsi_, RenderingDevice* device_, RID cubemap)
	{
		wsi = wsi_;
		device = device_;
		renderer.initialize(wsi, device, cubemap);
	}

	void DeferredRenderPipeline::shutdown()
	{
		frame_graph.reset();
		blackboard.reset();
		renderer.shutdown();
		wsi = nullptr;
		device = nullptr;
	}

	void DeferredRenderPipeline::render(const SceneView& view, MeshStorage& storage, bool include_imgui_pass)
	{
		frame_graph.reset();
		blackboard.reset();

		schedule_passes(view, storage, include_imgui_pass);

		frame_graph.compile();

		RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device = device;
		rc.wsi = wsi;
		TIMESTAMP_BEGIN();
		frame_graph.execute(&rc, &rc);
		RENDER_TIMESTAMP("Frame End");
	}

	void DeferredRenderPipeline::schedule_passes(const SceneView& view, MeshStorage& storage, bool include_imgui_pass)
	{
		renderer.setup_passes(frame_graph, blackboard, view, storage);
		if (include_imgui_pass)
			add_imgui_pass(frame_graph, blackboard, view.extent);
		add_blit_pass(frame_graph, blackboard, blackboard.get<deferred_pass_resource>());
	}

} // namespace Rendering
