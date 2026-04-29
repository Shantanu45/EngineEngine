#pragma  once

#include "framegraph_resources.h"
#include "imgui.h"
#include "../utils.h"

struct basic_pass_resource
{
	FrameGraphResource scene;
	FrameGraphResource depth;
	FrameGraphResource shadow_map_in;
	FrameGraphResource point_shadow_in;
	FrameGraphResource shadow_uniform_set;
	FrameGraphResource framebuffer_resource;
};

namespace Rendering
{

	struct imgui_pass_resource
	{
		FrameGraphResource ui;
		FrameGraphResource framebuffer_resource;
	};

	struct blit_pass_resource
	{
		FrameGraphResource scene;
		FrameGraphResource ui;
	};

	void add_blit_pass(FrameGraph& fg, FrameGraphBlackboard& bb)
	{
		auto& scene_handle = bb.get<basic_pass_resource>();
		auto& ui_handle = bb.get<imgui_pass_resource>();

		fg.add_callback_pass<blit_pass_resource>(
			"Blit Pass",

			[&](FrameGraph::Builder& builder, blit_pass_resource& data)
			{
				data.scene = builder.read(scene_handle.scene, TEXTURE_READ_FLAGS::READ_COLOR);
				data.ui = builder.read(ui_handle.ui, TEXTURE_READ_FLAGS::READ_COLOR);
				builder.read(scene_handle.depth, TEXTURE_READ_FLAGS::READ_COUNT);
				builder.set_side_effect();		// mark as non cull able
			},

			[=](const blit_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto wsi = rc.wsi;

				auto& scene = resources.get<Rendering::FrameGraphTexture>(data.scene);
				auto& ui = resources.get<Rendering::FrameGraphTexture>(data.ui);
				GPU_SCOPE(rc.command_buffer, "Blit Pass", Color(0.0, 1.0, 0.0, 1.0));

				wsi->blit_render_target_to_screen(scene.texture_rid, ui.texture_rid);
			}
		);
	}

	void add_imgui_pass(FrameGraph& fg, FrameGraphBlackboard& bb, Size2i extent)
	{
		bb.add<imgui_pass_resource>() =
			fg.add_callback_pass<imgui_pass_resource>(
				"Imgui Pass",

				[&](FrameGraph::Builder& builder, imgui_pass_resource& data)
				{
					RD::TextureFormat tf;
					tf.texture_type = RD::TEXTURE_TYPE_2D;
					tf.width = extent.x;
					tf.height = extent.y;
					tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf.format = RD::DATA_FORMAT_B8G8R8A8_UNORM;

					data.ui = builder.create<Rendering::FrameGraphTexture>("imgui texture", { tf, RD::TextureView(), "imgui texture" });

					data.ui = builder.write(data.ui, TEXTURE_WRITE_FLAGS::WRITE_COLOR);

					data.framebuffer_resource = builder.create<Rendering::FrameGraphFramebuffer>(
						"imgui framebuffer",
						{
							.build = [&fg, ui_id = data.ui](Rendering::RenderContext& rc) -> RID {
								auto& ui_tex = fg.get_resource<Rendering::FrameGraphTexture>(ui_id);
								return rc.device->framebuffer_create({ ui_tex.texture_rid });
							},
							.name = "imgui framebuffer"
						});
					data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
				},

				[=](const imgui_pass_resource& data,
					FrameGraphPassResources& resources,
					void* ctx)
				{
					auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
					auto cmd = rc.command_buffer;
					auto wsi = rc.wsi;

					DEBUG_ASSERT(wsi->imgui_active, "Trying to add imgui pass when imgui_active if false, NOT ALLOWED!");

					GPU_SCOPE(cmd, "Imgui Pass", Color(0.0, 0.0, 1.0, 1.0));
					RID frame_buffer = resources.get<Rendering::FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

					ImGui::Render();
					rc.device->imgui_execute(ImGui::GetDrawData(), cmd, frame_buffer);

					//rc.device->_submit_transfer_barriers(cmd);
				}
			);
	}
}
