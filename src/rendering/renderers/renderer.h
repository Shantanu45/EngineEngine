#pragma once

#include "rendering/render_passes/framegraph_resources.h"
#include "rendering/mesh_storage.h"
#include "rendering/light.h"
#include "rendering/camera.h"
#include "rendering/mesh_category.h"
#include "rendering/render_settings.h"
#include "math/math_common.h"

#include <glm/glm.hpp>
#include <vector>
#include "util/small_vector.h"

namespace Rendering {

struct MeshInstance {
    MeshHandle          mesh;
    glm::mat4           model;
    glm::mat4           normal_matrix;
    Util::SmallVector<RID>    material_sets; // pre-resolved from MaterialRegistry, one per primitive
    MeshCategory        category = MeshCategory::Opaque;
};

struct SceneView {
    Camera*                   camera      = nullptr;
    RenderSettings*           settings    = nullptr;
    double                    elapsed     = 0.0;
    MeshHandle                skybox_mesh;
    MeshHandle                grid_mesh;
    Util::SmallVector<MeshInstance> instances;
    Util::SmallVector<Light>        lights;
    Size2i                    extent;
};

struct IRenderer {
    virtual void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                              const SceneView& view, MeshStorage& storage) = 0;
    virtual ~IRenderer() = default;
};

} // namespace Rendering
