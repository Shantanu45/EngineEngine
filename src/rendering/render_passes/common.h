#pragma  once

#include "framegraph_resources.h"
#include "imgui.h"
#include "../utils.h"

namespace Rendering
{
	struct imgui_pass_resource
	{
		FrameGraphResource ui;
	};

	struct blit_pass_resource
	{
		FrameGraphResource scene;
		FrameGraphResource ui;
	};

	template<typename T>
	void add_blit_pass(FrameGraph& fg, FrameGraphBlackboard& bb)
	{
		const auto& scene = bb.get<T>();
		const auto& ui = bb.get<imgui_pass_resource>();

		fg.add_callback_pass<blit_pass_resource>(
			"Blit Pass",

			[&](FrameGraph::Builder& builder, blit_pass_resource& data)
			{
				data.scene = builder.read(scene.scene, 1u);
				data.ui = builder.read(ui.ui, 1u);
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

				wsi->blit_render_target_to_screen(scene.texture, ui.texture);
			}
		);
	}

	void add_imgui_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
		FrameGraphResource image_handle)
	{
		bb.add<imgui_pass_resource>() =
			fg.add_callback_pass<imgui_pass_resource>(
				"Imgui Pass",

				[image_handle](FrameGraph::Builder& builder, imgui_pass_resource& data)
				{
					data.ui = builder.write(image_handle, 1u);
				},

				[=](const imgui_pass_resource& data,
					FrameGraphPassResources& resources,
					void* ctx)
				{
					auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
					auto cmd = rc.command_buffer;
					auto wsi = rc.wsi;

					// TODO: find alternative, not sure if its good to do it in a loop.
					DEBUG_ASSERT(wsi->imgui_active, "Trying to add imgui pass when imgui_active if false, NOT ALLOWED!");

					GPU_SCOPE(cmd, "Imgui Pass", Color(0.0, 0.0, 1.0, 1.0));

					auto& scene = resources.get<Rendering::FrameGraphTexture>(data.ui);

					ImGui::Render();
					rc.device->imgui_execute(ImGui::GetDrawData(), cmd);

					//rc.device->_submit_transfer_barriers(cmd);
				}
			);
	}
}
