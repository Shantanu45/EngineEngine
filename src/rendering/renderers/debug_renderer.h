#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/pipeline_builder.h"
#include "rendering/rid_handle.h"
#include "rendering/debug_draw.h"
#include "rendering/camera.h"
#include "rendering/render_passes/common.h"
#include "rendering/render_passes/framegraph_resources.h"
#include "rendering/fg/frame_graph.h"
#include "rendering/fg/blackboard.h"

#include <glm/glm.hpp>

namespace Rendering {

class WSI;
class RenderingDevice;

// Standalone debug-line renderer. Owns all GPU resources; draws into an existing
// scene pass (LOAD_OP_LOAD) so it can be used after any renderer's scene pass.
//
// Usage per frame:
//   1. Fill DebugDraw singleton with geometry.
//   2. Call add_pass(fg, bb, scene_res, depth_res, camera, extent)
//      after the scene pass has been scheduled in the frame graph.
class DebugRenderer {
public:
    void initialize(RenderingDevice* device);

    // Uploads debug vertices and appends a frame graph pass that draws them
    // into the existing scene/depth textures (no clear).
    // scene_res and depth_res must be FrameGraphTexture resources already
    // written by the preceding scene pass.
    void add_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
                  FrameGraphResource& scene_res,
                  FrameGraphResource& depth_res,
                  const Camera& camera,
                  glm::uvec2 extent);

private:
    static constexpr uint32_t MAX_DEBUG_VERTS = 65536;

    RenderingDevice*   device       = nullptr;
    RD::VertexFormatID debug_vformat = RDC::INVALID_ID;
    RIDHandle          debug_vbuf;
    RIDHandle          debug_varray;
    Pipeline           pipeline_debug;
};

} // namespace Rendering
