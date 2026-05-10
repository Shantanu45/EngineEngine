#pragma once

#include "rendering/render_passes/framegraph_resources.h"
#include "rendering/mesh_storage.h"
#include "rendering/light.h"
#include "rendering/frame_data.h"
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
    Util::SmallVector<RID>    shadow_material_sets;
    Util::SmallVector<RID>    point_shadow_material_sets;
    Util::SmallVector<RID>    transparent_material_sets;
    MeshCategory        category = MeshCategory::Opaque;
    bool                transparent = false;
    glm::vec3           sort_center = glm::vec3(0.0f);
};

struct ShadowCasterInstance {
    MeshHandle          mesh;
    glm::mat4           model;
    glm::mat4           normal_matrix;
    Util::SmallVector<RID> shadow_material_sets;
    Util::SmallVector<RID> point_shadow_material_sets;
    MeshCategory        category = MeshCategory::Opaque;
};

struct SceneView {
    CameraData                camera;
    double                    elapsed     = 0.0;
    MeshHandle                skybox_mesh = INVALID_MESH;
    MeshHandle                grid_mesh   = INVALID_MESH;
    Util::SmallVector<MeshInstance> instances;
    Util::SmallVector<ShadowCasterInstance> shadow_casters;
    Util::SmallVector<Light>        lights;
    Size2i                    extent{};
    bool                      use_pbr_lighting = false;
    MaterialDebugView         material_debug_view = MaterialDebugView::Lit;
    DirectionalShadowMode     directional_shadow_mode = DirectionalShadowMode::SingleMap;
    glm::vec4                 shadow_bias = glm::vec4(0.002f, 0.0005f, 0.005f, 0.0001f);
};

struct IRenderer {
    virtual void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                              const SceneView& view, MeshStorage& storage) = 0;
    virtual ~IRenderer() = default;
};

} // namespace Rendering
