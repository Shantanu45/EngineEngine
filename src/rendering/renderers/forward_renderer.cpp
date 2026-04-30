#include "rendering/renderers/forward_renderer.h"

#include "rendering/wsi.h"
#include "rendering/uniform_set_builder.h"
#include "rendering/render_passes/common.h"
#include "rendering/render_passes/shadow_passes.h"
#include "math/rect2.h"

namespace Rendering {

void ForwardRenderer::initialize(WSI* wsi, RenderingDevice* device, RID cubemap) {
    auto vertex_format = wsi->get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS::DEFAULT);

    // --- Framebuffer formats ---

    RD::AttachmentFormat color_att;
    color_att.format      = RD::DATA_FORMAT_R8G8B8A8_UNORM;
    color_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    RD::AttachmentFormat depth_att;
    depth_att.format      = RD::DATA_FORMAT_D32_SFLOAT;
    depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    auto main_fb_format = RD::get_singleton()->framebuffer_format_create({ color_att, depth_att });

    RD::AttachmentFormat shadow_att;
    shadow_att.format      = RD::DATA_FORMAT_D32_SFLOAT;
    shadow_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
    auto shadow_fb_format  = RD::get_singleton()->framebuffer_format_create({ shadow_att });

    // --- Pipelines ---

    RDC::PipelineDepthStencilState ds_standard;
    ds_standard.enable_depth_test      = true;
    ds_standard.enable_depth_write     = true;
    ds_standard.depth_compare_operator = RDC::COMPARE_OP_LESS;

    pipeline_color = PipelineBuilder{}
        .set_shader({ "assets://shaders/tutorial/light_map.vert",
                      "assets://shaders/tutorial/light_map.frag" }, "light_map")
        .set_vertex_format(vertex_format)
        .set_depth_stencil_state(ds_standard)
        .build(main_fb_format);

    pipeline_light = PipelineBuilder{}
        .set_shader({ "assets://shaders/light_cube.vert",
                      "assets://shaders/light_cube.frag" }, "cube_shader")
        .set_vertex_format(vertex_format)
        .set_depth_stencil_state(ds_standard)
        .build(main_fb_format);

    pipeline_grid = PipelineBuilder{}
        .set_shader({ "assets://shaders/grid.vert",
                      "assets://shaders/grid.frag" }, "grid_shader")
        .set_vertex_format(vertex_format)
        .set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
        .set_depth_stencil_state(ds_standard)
        .build(main_fb_format);

    {
        RDC::PipelineDepthStencilState ds_skybox;
        ds_skybox.enable_depth_test      = true;
        ds_skybox.enable_depth_write     = false;
        ds_skybox.depth_compare_operator = RDC::COMPARE_OP_LESS_OR_EQUAL;
        RDC::PipelineRasterizationState rs_skybox;
        rs_skybox.cull_mode = RDC::POLYGON_CULL_DISABLED;
        pipeline_skybox = PipelineBuilder{}
            .set_shader({ "assets://shaders/skybox.vert",
                          "assets://shaders/skybox.frag" }, "skybox_shader")
            .set_vertex_format(vertex_format)
            .set_depth_stencil_state(ds_skybox)
            .set_rasterization_state(rs_skybox)
            .build(main_fb_format);
    }

    {
        RDC::PipelineRasterizationState rs_shadow;
        rs_shadow.cull_mode = RDC::POLYGON_CULL_FRONT;
        pipeline_shadow = PipelineBuilder{}
            .set_shader({ "assets://shaders/shadow.vert",
                          "assets://shaders/shadow.frag" }, "shadow_shader")
            .set_vertex_format(vertex_format)
            .set_depth_stencil_state(ds_standard)
            .set_rasterization_state(rs_shadow)
            .build(shadow_fb_format);
    }

    pipeline_point_shadow = PipelineBuilder{}
        .set_shader({
            "assets://shaders/point_shadow.vert",
            "assets://shaders/point_shadow.geom",
            "assets://shaders/point_shadow.frag"
        }, "point_shadow_shader")
        .set_vertex_format(vertex_format)
        .set_depth_stencil_state(ds_standard)
        .build(shadow_fb_format);

    // --- UBOs ---
    frame_ubo.create(device,       "Frame UBO");
    light_ubo.create(device,       "Light UBO");
    point_shadow_ubo.create(device, "Point Shadow UBO");

    // --- Samplers ---
    sampler              = RIDHandle(device->sampler_create({})); // nearest + clamp (default)
    sampler_cube         = RIDHandle(device->sampler_create({}));
    point_shadow_sampler = RIDHandle(device->sampler_create({}));

    {
        RDC::SamplerState ss;
        ss.mag_filter     = RDC::SAMPLER_FILTER_LINEAR;
        ss.min_filter     = RDC::SAMPLER_FILTER_LINEAR;
        ss.enable_compare = true;
        ss.compare_op     = RDC::COMPARE_OP_LESS_OR_EQUAL;
        shadow_sampler = RIDHandle(device->sampler_create(ss));
    }

    // --- Uniform sets (set 0) ---
    uniform_set_0 = UniformSetBuilder{}
        .add(frame_ubo.as_uniform(0))
        .add(light_ubo.as_uniform(2))
        .add_sampler(3, sampler)
        .add_sampler(4, shadow_sampler)
        .add_sampler(5, point_shadow_sampler)
        .build(device, pipeline_color.shader_rid, 0);

    uniform_set_0_light = UniformSetBuilder{}
        .add(frame_ubo.as_uniform(0))
        .build(device, pipeline_light.shader_rid, 0);

    uniform_set_0_shadow = UniformSetBuilder{}
        .add(frame_ubo.as_uniform(0))
        .build(device, pipeline_shadow.shader_rid, 0);

    uniform_set_0_point_shadow = UniformSetBuilder{}
        .add(frame_ubo.as_uniform(0))
        .add(point_shadow_ubo.as_uniform(1))
        .build(device, pipeline_point_shadow.shader_rid, 0);

    uniform_set_skybox = UniformSetBuilder{}
        .add(frame_ubo.as_uniform(0))
        .add_texture(1, sampler_cube, cubemap)
        .build(device, pipeline_skybox.shader_rid, 0);
}

void ForwardRenderer::upload_frame_data(RenderingDevice* device, const Camera& camera,
                                        double elapsed, const glm::mat4& light_space_matrix) {
    FrameData_UBO data{};
    data.camera.view        = camera.get_view();
    data.camera.proj        = camera.get_projection();
    data.camera.cameraPos   = camera.get_position();
    data.time               = static_cast<float>(elapsed);
    data.light_space_matrix = light_space_matrix;
    frame_ubo.upload(device, data);
}

void ForwardRenderer::upload_light_data(RenderingDevice* device, const LightBuffer& lights) {
    light_ubo.upload(device, lights);
}

void ForwardRenderer::upload_point_shadow_data(RenderingDevice* device, const PointShadowUBO& data) {
    point_shadow_ubo.upload(device, data);
}

void ForwardRenderer::setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
                                   const SceneView& view, MeshStorage& storage) {
    add_point_shadow_pass(fg, bb, 1024, view.point_shadow_drawables, storage);
    add_shadow_pass(fg, bb, { 2048, 2048 }, view.shadow_drawables, storage);

    // Cache the pipeline RID so the execute lambda can compare without capturing `this`.
    auto color_pipeline_rid = pipeline_color.pipeline_rid;

    bb.add<basic_pass_resource>() =
        fg.add_callback_pass<basic_pass_resource>(
            "Basic Pass",
            [&](FrameGraph::Builder& builder, basic_pass_resource& data)
            {
                RD::TextureFormat tf;
                tf.texture_type = RD::TEXTURE_TYPE_2D;
                tf.width        = view.extent.x;
                tf.height       = view.extent.y;
                tf.usage_bits   = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
                tf.format       = RD::DATA_FORMAT_R8G8B8A8_UNORM;
                data.scene = builder.create<FrameGraphTexture>("scene texture", { tf, RD::TextureView(), "scene texture" });

                RD::TextureFormat tf_depth;
                tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
                tf_depth.width        = view.extent.x;
                tf_depth.height       = view.extent.y;
                tf_depth.usage_bits   = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                tf_depth.format       = RD::DATA_FORMAT_D32_SFLOAT;
                data.depth = builder.create<FrameGraphTexture>("scene depth texture", { tf_depth, RD::TextureView(), "scene depth texture" });

                data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
                data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);

                auto& shadow_res       = bb.get<shadow_pass_resource>();
                auto& point_shadow_res = bb.get<point_shadow_pass_resource>();
                data.shadow_map_in   = builder.read(shadow_res.shadow_map,            TEXTURE_READ_FLAGS::READ_DEPTH);
                data.point_shadow_in = builder.read(point_shadow_res.shadow_cubemap,  TEXTURE_READ_FLAGS::READ_DEPTH);

                data.shadow_uniform_set = builder.create<FrameGraphUniformSet>(
                    "shadow uniform set",
                    {
                        .build = [&fg,
                                  shader_rid = pipeline_color.shader_rid,
                                  shadow_id  = shadow_res.shadow_map,
                                  point_id   = point_shadow_res.shadow_cubemap]
                                 (RenderContext& rc) -> RID {
                            auto& shadow_tex      = fg.get_resource<FrameGraphTexture>(shadow_id);
                            auto& point_shadow_tex = fg.get_resource<FrameGraphTexture>(point_id);
                            return UniformSetBuilder{}
                                .add_texture_only(0, shadow_tex.texture_rid)
                                .add_texture_only(1, point_shadow_tex.texture_rid)
                                .build(rc.device, shader_rid, 1);
                        },
                        .name = "shadow uniform set"
                    });
                data.shadow_uniform_set = builder.write(data.shadow_uniform_set, FrameGraph::kFlagsIgnored);

                data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
                    "basic framebuffer",
                    {
                        .build = [&fg, scene_id = data.scene, depth_id = data.depth](RenderContext& rc) -> RID {
                            auto& scene_tex = fg.get_resource<FrameGraphTexture>(scene_id);
                            auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
                            return rc.device->framebuffer_create({ scene_tex.texture_rid, depth_tex.texture_rid });
                        },
                        .name = "basic framebuffer"
                    });
                data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
            },
            [drawables = view.main_drawables, &storage, color_pipeline_rid](
                const basic_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
            {
                auto& rc  = *static_cast<RenderContext*>(ctx);
                auto  cmd = rc.command_buffer;

                RID uniform_set_1 = resources.get<FrameGraphUniformSet>(data.shadow_uniform_set).uniform_set_rid;
                RID frame_buffer  = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

                uint32_t w = rc.device->screen_get_width();
                uint32_t h = rc.device->screen_get_height();

                GPU_SCOPE(cmd, "Basic Pass", Color(1.0f, 0.0f, 0.0f, 1.0f));
                std::array<RDD::RenderPassClearValue, 2> clear_values;
                clear_values[0].color   = Color();
                clear_values[1].depth   = 1.0f;
                clear_values[1].stencil = 0;

                rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

                for (auto drawable : drawables) {
                    if (drawable.pipeline.pipeline_rid == color_pipeline_rid)
                        drawable.set_uniform_set({ uniform_set_1, 1 });
                    submit_drawable(rc, cmd, drawable, storage);
                }

                rc.wsi->end_render_pass(cmd);
            });
}

} // namespace Rendering
