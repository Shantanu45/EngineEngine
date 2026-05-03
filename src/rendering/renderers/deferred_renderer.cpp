
#include "deferred_renderer.h"
#include <rendering/uniform_set_builder.h>

namespace Rendering
{
	void DeferredRenderer::initialize(WSI* wsi, RenderingDevice* device, RID cubemap)
	{
		auto vertex_format = wsi->get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS::DEFAULT);
		
		// Framebuffers -----------------------
		// Position (World space)
		RD::AttachmentFormat position_att;
		position_att.format = RDC::DATA_FORMAT_R16G16B16A16_UNORM;
		position_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Albedo 8
		RD::AttachmentFormat albedo_att;
		albedo_att.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;
		albedo_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Normal 16 (World space)
		RD::AttachmentFormat normal_att;
		normal_att.format = RDC::DATA_FORMAT_R16G16B16A16_UNORM;
		normal_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Depth
		RD::AttachmentFormat depth_att;
		depth_att.format = RDC::DATA_FORMAT_D32_SFLOAT;
		depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		auto main_fb_format = RD::get_singleton()->framebuffer_format_create({position_att, 
			albedo_att, normal_att, depth_att});

		RD::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test = true;
		depth_state.enable_depth_write = true;
		depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

		deferred_pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/mrt.vert", "assets://shaders/deferred/mrt.frag"}, 
				"deferred_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(depth_state)
			.build(main_fb_format);

		sampler = RIDHandle(device->sampler_create({})); // nearest + clamp (default)

		// --- UBOs ---
		frame_ubo.create(device, "Frame UBO");

		// --- Uniform sets (set 0) ---
		uniform_set_0 = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add_sampler(3, sampler)
			.build(device, deferred_pipeline.shader_rid, 0);
	}

	void DeferredRenderer::upload_frame_data(RenderingDevice* device, const Camera& camera,
		double elapsed, const glm::mat4& light_space_matrix) {
		FrameData_UBO data{};
		data.camera.view = camera.get_view();
		data.camera.proj = camera.get_projection();
		data.camera.cameraPos = camera.get_position();
		data.time = static_cast<float>(elapsed);
		data.light_space_matrix = light_space_matrix;
		frame_ubo.upload(device, data);
	}

	void DeferredRenderer::setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb, 
		const SceneView& view, MeshStorage& storage)
	{
		setup_offscreen_pass(fg, bb, view, storage);
		setup_deferred_pass(fg, bb, view, storage);
		return;
	}

	void DeferredRenderer::setup_offscreen_pass(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage)
	{
		auto pipeline_rid = deferred_pipeline.pipeline_rid;

		bb.add<offscreen_pass_resource>() =
			fg.add_callback_pass<offscreen_pass_resource>(
				"Offscreen Pass",
				[&](FrameGraph::Builder& builder, offscreen_pass_resource& data)
				{

					RD::TextureFormat tf_depth;
					tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
					tf_depth.width = view.extent.x;
					tf_depth.height = view.extent.y;
					tf_depth.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
					tf_depth.format = RD::DATA_FORMAT_D32_SFLOAT;
					data.depth_resource = builder.create<FrameGraphTexture>("scene depth texture", { tf_depth, RD::TextureView(), "scene depth texture" });

					//data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.depth_resource = builder.write(data.depth_resource, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
					// ----

					RD::TextureFormat tf_color;
					tf_color.texture_type = RD::TEXTURE_TYPE_2D;
					tf_color.width = view.extent.x;
					tf_color.height = view.extent.y;
					tf_color.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf_color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					data.albedo_resource = builder.create<FrameGraphTexture>("albedo texture", { tf_color, RD::TextureView(), "albedo texture" });

					RD::TextureFormat tf_data;
					tf_data.texture_type = RD::TEXTURE_TYPE_2D;
					tf_data.width = view.extent.x;
					tf_data.height = view.extent.y;
					tf_data.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf_data.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
					data.normal_resource = builder.create<FrameGraphTexture>("normal texture", { tf_data, RD::TextureView(), "normal texture" });
					data.position_resource = builder.create<FrameGraphTexture>("position texture", { tf_data, RD::TextureView(), "position texture" });

					// framebuffer setup
					data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
						"offscreen framebuffer",
						{
							.build = [&fg, albedo = data.albedo_resource, normal = data.normal_resource, position = data.position_resource, depth_id = data.depth_resource](RenderContext& rc) -> RID {
								auto& position_tex = fg.get_resource<FrameGraphTexture>(position);
								auto& normal_tex = fg.get_resource<FrameGraphTexture>(normal);
								auto& albedo_tex = fg.get_resource<FrameGraphTexture>(albedo);
								auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
								return rc.device->framebuffer_create({ position_tex.texture_rid, normal_tex.texture_rid, albedo_tex.texture_rid, depth_tex.texture_rid });
							},
							.name = "offscreen framebuffer"
						});

					data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
				},
				[drawables = view.main_drawables, &storage, pipeline_rid](
					const offscreen_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
				{
					auto& rc = *static_cast<RenderContext*>(ctx);
					auto  cmd = rc.command_buffer;

					uint32_t w = rc.device->screen_get_width();
					uint32_t h = rc.device->screen_get_height();

					GPU_SCOPE(cmd, "Offscreen Pass", Color(1.0f, 0.0f, 0.0f, 1.0f));
					std::array<RDD::RenderPassClearValue, 2> clear_values;
					clear_values[0].color = Color();
					clear_values[1].depth = 1.0f;
					clear_values[1].stencil = 0;

					RID frame_buffer = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

					rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

					for (auto drawable : drawables) {
						submit_drawable(rc, cmd, drawable, storage);
					}

					rc.wsi->end_render_pass(cmd);
				});
	}

	void DeferredRenderer::setup_deferred_pass(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage)
	{
		auto pipeline_rid = deferred_pipeline.pipeline_rid;

		bb.add<deferred_pass_resource>() =
			fg.add_callback_pass<deferred_pass_resource>(
				"Differed Pass",
				[&](FrameGraph::Builder& builder, deferred_pass_resource& data)
				{
				},
				[drawables = view.main_drawables, &storage, pipeline_rid](
					const deferred_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
				{
					auto& rc = *static_cast<RenderContext*>(ctx);
					auto  cmd = rc.command_buffer;

					uint32_t w = rc.device->screen_get_width();
					uint32_t h = rc.device->screen_get_height();
				});
	}

}
