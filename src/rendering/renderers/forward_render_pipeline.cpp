#include "rendering/renderers/forward_render_pipeline.h"

#include "rendering/render_passes/common.h"
#include "rendering/utils.h"
#include "util/profiler.h"

namespace Rendering {

void ForwardRenderPipeline::initialize(WSI* wsi_, RenderingDevice* device_, RID cubemap)
{
	wsi = wsi_;
	device = device_;
	renderer.initialize(wsi, device, cubemap);
}

void ForwardRenderPipeline::render(const SceneView& view, MeshStorage& storage)
{
	frame_graph.reset();
	blackboard.reset();

	schedule_passes(view, storage);

	frame_graph.compile();

	RenderContext rc;
	rc.command_buffer = device->get_current_command_buffer();
	rc.device         = device;
	rc.wsi            = wsi;
	TIMESTAMP_BEGIN();
	frame_graph.execute(&rc, &rc);
	RENDER_TIMESTAMP("Frame End");
}

void ForwardRenderPipeline::schedule_passes(const SceneView& view, MeshStorage& storage)
{
	renderer.setup_passes(frame_graph, blackboard, view, storage);
	add_imgui_pass(frame_graph, blackboard, view.extent);
	add_blit_pass(frame_graph, blackboard, blackboard.get<forward_pass_resource>());
}

} // namespace Rendering
