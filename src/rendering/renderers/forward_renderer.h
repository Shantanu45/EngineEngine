#pragma once

#include "rendering/renderers/renderer.h"
#include "rendering/renderers/debug_renderer.h"
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
#include "util/small_vector.h"

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
        void shutdown();

        // IRenderer — uploads per-frame UBOs and schedules all passes.
        void setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                          const SceneView& view, MeshStorage& storage) override;

        // Expose the color pipeline so callers can obtain shader_rid for material creation.
        Pipeline color_pipeline() const { return pipeline_color; }
        Pipeline pbr_color_pipeline() const { return pipeline_pbr; }
        Pipeline shadow_pipeline() const { return pipeline_shadow; }
        Pipeline point_shadow_pipeline() const { return pipeline_point_shadow; }

    private:
        std::vector<Drawable> build_main_drawables(const SceneView& view) const;
        void create_shared_resources(RenderingDevice* device);
        void create_samplers(RenderingDevice* device);
        void create_main_pipelines(WSI* wsi, RenderingDevice* device);
        void create_overlay_pipelines(WSI* wsi, RenderingDevice* device);
        void create_shadow_pipelines(WSI* wsi, RenderingDevice* device);
        void create_main_uniform_sets(RenderingDevice* device);
        void create_overlay_uniform_sets(RenderingDevice* device, RID cubemap);
        void create_shadow_uniform_sets(RenderingDevice* device);

        RenderingDevice* device = nullptr;
        DebugRenderer debug_renderer;

        // UBOs — declared first so they outlive the uniform sets that reference them.
        UniformBuffer<FrameData_UBO>   frame_ubo;
        UniformBuffer<LightBuffer_UBO> light_ubo;
        UniformBuffer<ShadowBuffer_UBO> shadow_ubo;

        // Samplers
        RIDHandle sampler;
        RIDHandle sampler_cube;
        RIDHandle shadow_sampler;
        RIDHandle point_shadow_sampler;

        // Pipelines
        Pipeline pipeline_color;
        Pipeline pipeline_pbr;
        Pipeline pipeline_light;
        Pipeline pipeline_grid;
        Pipeline pipeline_shadow;
        Pipeline pipeline_point_shadow;
        Pipeline pipeline_skybox;

        // Uniform sets — declared last so they are destroyed first.
        RIDHandle uniform_set_0;
        RIDHandle uniform_set_0_pbr;
        RIDHandle uniform_set_0_light;
        RIDHandle uniform_set_0_shadow;
        RIDHandle uniform_set_0_point_shadow;
        RIDHandle uniform_set_skybox;
    };

} // namespace Rendering
