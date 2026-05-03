#pragma once

#include "rendering/renderers/renderer.h"
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

struct forward_pass_resource : public blit_scene_input_resource
{
	FrameGraphResource shadow_map_in;
	FrameGraphResource point_shadow_in;
	FrameGraphResource shadow_uniform_set;
	FrameGraphResource framebuffer_resource;
};

namespace Rendering {

    class WSI;
    class RenderingDevice;

    // Forward rendering technique: shadow pass -> point-shadow pass -> lit scene pass.
    //
    // Lifetime:
    //   initialize()   — call once after WSI vertex/index formats are configured.
    //   setup_passes() — uploads UBOs and adds passes to the frame graph each frame.
    //
    // The cubemap RID passed to initialize() must outlive this renderer.
    class ForwardRenderer : public IRenderer {
    public:
        void initialize(WSI* wsi, RenderingDevice* device, RID cubemap);

        // IRenderer — uploads per-frame UBOs and schedules all passes.
        void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                          const SceneView& view, MeshStorage& storage) override;

        // Expose the color pipeline so callers can obtain shader_rid for material creation.
        Pipeline color_pipeline() const { return pipeline_color; }

    private:
        std::vector<Drawable> build_shadow_drawables(const SceneView& view) const;
        std::vector<Drawable> build_point_shadow_drawables(const SceneView& view) const;
        std::vector<Drawable> build_main_drawables(const SceneView& view) const;

        glm::mat4        compute_light_space_matrix(const std::vector<Light>& lights) const;
        PointShadow_UBO  build_point_shadow_ubo(const std::vector<Light>& lights) const;

        RenderingDevice* device = nullptr;

        // UBOs — declared first so they outlive the uniform sets that reference them.
        UniformBuffer<FrameData_UBO>   frame_ubo;
        UniformBuffer<LightBuffer_UBO> light_ubo;
        UniformBuffer<PointShadow_UBO> point_shadow_ubo;

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
