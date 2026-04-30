#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/pipeline_builder.h"
#include "rendering/uniform_buffer.h"
#include "rendering/rid_handle.h"
#include "rendering/frame_data.h"
#include "rendering/light.h"
#include "rendering/camera.h"

#include <glm/glm.hpp>

namespace Rendering {

    class WSI;
    class RenderingDevice;

    // Forward rendering technique: shadow pass -> point-shadow pass -> lit scene pass.
    //
    // Lifetime:
    //   initialize()       — call once after WSI vertex/index formats are configured.
    //   upload_*()         — call every frame before setup_passes().
    //   setup_passes()     — adds passes to the frame graph each frame.
    //
    // The cubemap RID passed to initialize() must outlive this renderer.
    class ForwardRenderer : public IRenderer {
    public:
        void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);

        // Per-frame UBO uploads — call before setup_passes each frame.
        void upload_frame_data(RenderingDevice* device, const Camera& camera,
                               double elapsed, const glm::mat4& light_space_matrix);
        void upload_light_data(RenderingDevice* device, const LightBuffer& lights);
        void upload_point_shadow_data(RenderingDevice* device, const PointShadowUBO& data);

        // IRenderer
        void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                          const SceneView& view, MeshStorage& storage) override;

        // Pipeline accessors — used by the application to build Drawables.
        Pipeline color_pipeline()        const { return pipeline_color; }
        Pipeline light_pipeline()        const { return pipeline_light; }
        Pipeline grid_pipeline()         const { return pipeline_grid; }
        Pipeline shadow_pipeline()       const { return pipeline_shadow; }
        Pipeline point_shadow_pipeline() const { return pipeline_point_shadow; }
        Pipeline skybox_pipeline()       const { return pipeline_skybox; }

        // Set-0 uniform set accessors — used by the application to build Drawables.
        RID color_set0()        const { return uniform_set_0; }
        RID light_set0()        const { return uniform_set_0_light; }
        RID shadow_set0()       const { return uniform_set_0_shadow; }
        RID point_shadow_set0() const { return uniform_set_0_point_shadow; }
        RID skybox_set0()       const { return uniform_set_skybox; }

    private:
        // UBOs — declared first so they outlive the uniform sets that reference them.
        UniformBuffer<FrameData_UBO>  frame_ubo;
        UniformBuffer<LightBuffer>    light_ubo;
        UniformBuffer<PointShadowUBO> point_shadow_ubo;

        // Samplers
        RIDHandle sampler;
        RIDHandle sampler_cube;
        RIDHandle shadow_sampler;
        RIDHandle point_shadow_sampler;

        // Pipelines
        Pipeline pipeline_color;
        Pipeline pipeline_light;
        Pipeline pipeline_grid;
        Pipeline pipeline_shadow;
        Pipeline pipeline_point_shadow;
        Pipeline pipeline_skybox;

        // Uniform sets — declared last so they are destroyed first.
        RIDHandle uniform_set_0;
        RIDHandle uniform_set_0_light;
        RIDHandle uniform_set_0_shadow;
        RIDHandle uniform_set_0_point_shadow;
        RIDHandle uniform_set_skybox;
    };

} // namespace Rendering
