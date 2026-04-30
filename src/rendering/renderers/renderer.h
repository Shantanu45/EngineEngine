#pragma once

// Must come before drawable.h — defines RenderContext which submit_drawable uses.
#include "rendering/render_passes/framegraph_resources.h"
#include "rendering/drawable.h"
#include "rendering/mesh_storage.h"
#include "math/math_common.h"

#include <vector>

namespace Rendering {

struct SceneView {
    std::vector<Drawable> shadow_drawables;
    std::vector<Drawable> point_shadow_drawables;
    std::vector<Drawable> main_drawables;
    Size2i                extent;
};

struct IRenderer {
    virtual void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                              const SceneView& view, MeshStorage& storage) = 0;
    virtual ~IRenderer() = default;
};

} // namespace Rendering
