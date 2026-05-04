#include "rendering/renderers/debug_renderer.h"

#include "rendering/wsi.h"
#include "rendering/render_passes/framegraph_resources.h"
#include "math/rect2.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Rendering {

void DebugRenderer::initialize(RenderingDevice* dev) {
    device = dev;

    // Vertex format: vec3 pos + vec4 color (28 bytes stride)
    RDC::VertexAttribute dbg_pos{};
    dbg_pos.binding  = 0;
    dbg_pos.location = 0;
    dbg_pos.format   = RDC::DATA_FORMAT_R32G32B32_SFLOAT;
    dbg_pos.offset   = 0;
    dbg_pos.stride   = sizeof(DebugVertex);

    RDC::VertexAttribute dbg_col{};
    dbg_col.binding  = 0;
    dbg_col.location = 1;
    dbg_col.format   = RDC::DATA_FORMAT_R32G32B32A32_SFLOAT;
    dbg_col.offset   = offsetof(DebugVertex, color);
    dbg_col.stride   = sizeof(DebugVertex);

    debug_vformat = device->vertex_format_create({ dbg_pos, dbg_col });
    debug_vbuf    = RIDHandle(device->vertex_buffer_create(MAX_DEBUG_VERTS * sizeof(DebugVertex)));
    debug_varray  = RIDHandle(device->vertex_array_create(MAX_DEBUG_VERTS, debug_vformat, { debug_vbuf.get() }));

    // Framebuffer format with LOAD ops (same color/depth formats as main scene pass)
    RD::AttachmentFormat color_att;
    color_att.format      = RD::DATA_FORMAT_R8G8B8A8_UNORM;
    color_att.usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
    color_att.load_op     = RDD::ATTACHMENT_LOAD_OP_LOAD;

    RD::AttachmentFormat depth_att;
    depth_att.format      = RD::DATA_FORMAT_D32_SFLOAT;
    depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_att.load_op     = RDD::ATTACHMENT_LOAD_OP_LOAD;

    auto debug_fb_format = RD::get_singleton()->framebuffer_format_create({ color_att, depth_att });

    RDC::PipelineDepthStencilState ds;
    ds.enable_depth_test      = true;
    ds.enable_depth_write     = false;
    ds.depth_compare_operator = RDC::COMPARE_OP_LESS;

    pipeline_debug = PipelineBuilder{}
        .set_shader({ "assets://shaders/debug/debug.vert",
                      "assets://shaders/debug/debug.frag" }, "debug_lines")
        .set_vertex_format(debug_vformat)
        .set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
        .set_depth_stencil_state(ds)
        .build(debug_fb_format);
}

void DebugRenderer::add_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
                              FrameGraphResource scene_res,
                              FrameGraphResource depth_res,
                              const Camera& camera,
                              glm::uvec2 extent)
{
    auto& dd = DebugDraw::get();
    if (dd.empty())
        return;

    const auto& verts = dd.vertices();
    uint32_t vert_count = static_cast<uint32_t>(
        std::min(verts.size(), static_cast<size_t>(MAX_DEBUG_VERTS)));
    device->buffer_update(debug_vbuf.get(), 0,
        vert_count * sizeof(DebugVertex), verts.data());
    dd.clear();

    glm::mat4 vp = camera.get_projection() * camera.get_view();

    struct DebugPassData {
        FrameGraphResource scene;
        FrameGraphResource depth;
        FrameGraphResource framebuffer;
    };

    fg.add_callback_pass<DebugPassData>(
        "Debug Lines",
        [&](FrameGraph::Builder& builder, DebugPassData& data) {
            data.scene = builder.write(scene_res, TEXTURE_WRITE_FLAGS::WRITE_COLOR_LOAD);
            data.depth = builder.write(depth_res, TEXTURE_WRITE_FLAGS::WRITE_DEPTH_LOAD);

            data.framebuffer = builder.create<FrameGraphFramebuffer>(
                "debug framebuffer",
                {
                    .build = [&fg, scene_id = scene_res, depth_id = depth_res](RenderContext& rc) -> RID {
                        auto& scene_tex = fg.get_resource<FrameGraphTexture>(scene_id);
                        auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
                        return rc.device->framebuffer_create_load(
                            { scene_tex.texture_rid, depth_tex.texture_rid });
                    },
                    .name = "debug framebuffer"
                });
            data.framebuffer = builder.write(data.framebuffer, FrameGraph::kFlagsIgnored);
        },
        [pipeline = pipeline_debug, varr = debug_varray.get(),
         count = vert_count, vp, extent](
            const DebugPassData& data, FrameGraphPassResources& resources, void* ctx)
        {
            auto& rc  = *static_cast<RenderContext*>(ctx);
            auto  cmd = rc.command_buffer;

            RID fb = resources.get<FrameGraphFramebuffer>(data.framebuffer).framebuffer_rid;

            std::array<RDD::RenderPassClearValue, 2> clear{};
            clear[0].color = Color();
            clear[1].depth = 1.0f;

            rc.device->begin_render_pass_from_frame_buffer(fb, Rect2i(0, 0, extent.x, extent.y), clear);
            rc.device->bind_render_pipeline(cmd, pipeline.pipeline_rid);
            rc.device->set_push_constant(&vp, sizeof(vp), pipeline.shader_rid);
            rc.device->bind_vertex_array(varr);
            rc.device->render_draw(cmd, count, 1);
            rc.wsi->end_render_pass(cmd);
        });
}

} // namespace Rendering
