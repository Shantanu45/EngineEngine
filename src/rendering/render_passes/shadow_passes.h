#pragma once
#include "framegraph_resources.h"
#include "rendering/drawable.h"
#include "math/math_common.h"
#include "util/small_vector.h"

struct shadow_pass_resource {
    FrameGraphResource shadow_map;
    FrameGraphResource framebuffer_resource;
};

struct point_shadow_pass_resource {
    FrameGraphResource shadow_cubemap;
    FrameGraphResource framebuffer_resource;
};

namespace Rendering
{
    inline void add_shadow_pass(
        FrameGraph& fg,
        FrameGraphBlackboard& bb,
        Size2i shadow_extent,
        std::vector<Drawable> drawables,
        MeshStorage& storage)
    {
        bb.add<shadow_pass_resource>() =
            fg.add_callback_pass<shadow_pass_resource>(
                "Shadow Pass",
                [&](FrameGraph::Builder& builder, shadow_pass_resource& data)
                {
                    RD::TextureFormat tf;
                    tf.texture_type = RD::TEXTURE_TYPE_2D;
                    tf.width        = shadow_extent.x;
                    tf.height       = shadow_extent.y;
                    tf.usage_bits   = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                    | RD::TEXTURE_USAGE_SAMPLING_BIT;
                    tf.format       = RD::DATA_FORMAT_D32_SFLOAT;

                    data.shadow_map = builder.create<FrameGraphTexture>(
                        "shadow map", { tf, RD::TextureView(), "shadow map" });
                    data.shadow_map = builder.write(
                        data.shadow_map, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);

                    data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
                        "shadow framebuffer",
                        {
                            .build = [&fg, shadow_id = data.shadow_map](RenderContext& rc) -> RID {
                                auto& shadow_tex = fg.get_resource<FrameGraphTexture>(shadow_id);
                                return rc.device->framebuffer_create({ shadow_tex.texture_rid });
                            },
                            .name = "shadow framebuffer"
                        });
                    data.framebuffer_resource = builder.write(
                        data.framebuffer_resource, FrameGraph::kFlagsIgnored);
                },
                [=, &storage](const shadow_pass_resource& data,
                    FrameGraphPassResources& resources,
                    void* ctx)
                {
                    auto& rc  = *static_cast<RenderContext*>(ctx);
                    auto  cmd = rc.command_buffer;

                    RID fb = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

                    GPU_SCOPE(cmd, "Shadow Pass", Color(1.0f, 0.5f, 0.0f, 1.0f));
                    std::array<RDD::RenderPassClearValue, 1> clear_values;
                    clear_values[0].depth   = 1.0f;
                    clear_values[0].stencil = 0;

                    rc.device->begin_render_pass_from_frame_buffer(
                        fb, Rect2i(0, 0, shadow_extent.x, shadow_extent.y), clear_values);

                    for (const auto& drawable : drawables)
                        submit_drawable(rc, cmd, drawable, storage);

                    rc.wsi->end_render_pass(cmd);
                });
    }

    inline void add_point_shadow_pass(
        FrameGraph& fg,
        FrameGraphBlackboard& bb,
        uint32_t face_size,
        std::vector<Drawable> drawables,
        MeshStorage& storage)
    {
        bb.add<point_shadow_pass_resource>() =
            fg.add_callback_pass<point_shadow_pass_resource>(
                "Point Shadow Pass",
                [&](FrameGraph::Builder& builder, point_shadow_pass_resource& data)
                {
                    RD::TextureFormat tf;
                    tf.texture_type  = RD::TEXTURE_TYPE_CUBE;
                    tf.width         = face_size;
                    tf.height        = face_size;
                    tf.array_layers  = 6;
                    tf.usage_bits    = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                     | RD::TEXTURE_USAGE_SAMPLING_BIT;
                    tf.format        = RD::DATA_FORMAT_D32_SFLOAT;

                    data.shadow_cubemap = builder.create<FrameGraphTexture>(
                        "point shadow cubemap",
                        { tf, RD::TextureView(), "point shadow cubemap" });
                    data.shadow_cubemap = builder.write(
                        data.shadow_cubemap, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);

                    data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
                        "point shadow framebuffer",
                        {
                            .build = [&fg, cube_id = data.shadow_cubemap](RenderContext& rc) -> RID {
                                auto& cube_tex = fg.get_resource<FrameGraphTexture>(cube_id);
                                return rc.device->framebuffer_create({ cube_tex.texture_rid });
                            },
                            .name = "point shadow framebuffer"
                        });
                    data.framebuffer_resource = builder.write(
                        data.framebuffer_resource, FrameGraph::kFlagsIgnored);
                },
                [=, &storage](const point_shadow_pass_resource& data,
                    FrameGraphPassResources& resources,
                    void* ctx)
                {
                    auto& rc  = *static_cast<RenderContext*>(ctx);
                    auto  cmd = rc.command_buffer;

                    RID fb = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

                    GPU_SCOPE(cmd, "Point Shadow Pass", Color(0.8f, 0.2f, 1.0f, 1.0f));
                    std::array<RDD::RenderPassClearValue, 1> clear_values;
                    clear_values[0].depth   = 1.0f;
                    clear_values[0].stencil = 0;

                    rc.device->begin_render_pass_from_frame_buffer(
                        fb, Rect2i(0, 0, face_size, face_size), clear_values);

                    for (const auto& drawable : drawables)
                        submit_drawable(rc, cmd, drawable, storage);

                    rc.wsi->end_render_pass(cmd);
                });
    }
}
