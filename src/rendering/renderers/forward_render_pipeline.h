#pragma once

#include "rendering/fg/blackboard.h"
#include "rendering/fg/frame_graph.h"
#include "rendering/renderers/forward_renderer.h"

namespace Rendering {

class ForwardRenderPipeline {
public:
	void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);

	void render(const SceneView& view, MeshStorage& storage);

	Pipeline color_pipeline() const { return renderer.color_pipeline(); }

private:
	void schedule_passes(const SceneView& view, MeshStorage& storage);

	WSI* wsi = nullptr;
	RenderingDevice* device = nullptr;
	FrameGraph frame_graph;
	FrameGraphBlackboard blackboard;
	ForwardRenderer renderer;
};

} // namespace Rendering
