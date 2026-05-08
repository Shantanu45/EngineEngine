
#include "deferred_renderer.h"
#include <rendering/uniform_set_builder.h>
#include <rendering/drawable.h>
#include <rendering/wsi.h>
#include "math/rect2.h"
#include "util/small_vector.h"

namespace Rendering
{
	void DeferredRenderer::initialize(WSI* wsi, RenderingDevice* dev, RID cubemap)
	{
		device = dev;
		create_offscreen_pipeline(wsi, dev);
		create_deferred_pipeline(wsi, dev);
	}

	void DeferredRenderer::setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
		const SceneView& view, MeshStorage& storage)
	{
		// Upload per-frame UBOs
		{
			FrameData_UBO frame{};
			frame.camera                  = view.camera;
			frame.time                     = static_cast<float>(view.elapsed);
			frame.directional_shadow_index = 0;
			frame.point_shadow_index       = 0;
			frame_ubo.upload(device, frame);
		}
		{
			LightBuffer_UBO buf{};
			for (const auto& l : view.lights) {
				if (buf.count >= MAX_LIGHTS) break;
				buf.lights[buf.count++] = l;
			}
			light_ubo.upload(device, buf);
		}

		setup_offscreen_pass(fg, bb, view, storage);
		setup_deferred_pass(fg, bb, view, storage);
	}

	void DeferredRenderer::setup_offscreen_pass(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage)
	{
		// Build drawables from instances — only opaque geometry goes into the G-buffer.
		std::vector<Drawable> drawables;
		for (const auto& inst : view.instances) {
			if (inst.category != MeshCategory::Opaque) continue;
			drawables.push_back(Drawable::make(
				offscreen_pipeline, inst.mesh,
				PushConstantData::from(ObjectData_UBO{ inst.model, inst.normal_matrix }),
				{ { (RID)uniform_set_0, 0 } },
				inst.material_sets
			));
		}

		auto pipeline_rid = offscreen_pipeline.pipeline_rid;

		bb.add<offscreen_pass_resource>() =
			fg.add_callback_pass<offscreen_pass_resource>(
				"Offscreen Pass",
				[&view, &fg](FrameGraph::Builder& builder, offscreen_pass_resource& data)
				{

					RD::TextureFormat tf_depth;
					tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
					tf_depth.width = view.extent.x;
					tf_depth.height = view.extent.y;
					tf_depth.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
					tf_depth.format = RD::DATA_FORMAT_D32_SFLOAT;
					data.depth_resource = builder.create<FrameGraphTexture>("scene depth texture", { tf_depth, RD::TextureView(), "scene depth texture" });

					data.depth_resource = builder.write(data.depth_resource, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
					// ----

					RD::TextureFormat tf_color;
					tf_color.texture_type = RD::TEXTURE_TYPE_2D;
					tf_color.width = view.extent.x;
					tf_color.height = view.extent.y;
					tf_color.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf_color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					data.albedo_resource = builder.create<FrameGraphTexture>("albedo texture", { tf_color, RD::TextureView(), "albedo texture" });
					data.albedo_resource = builder.write(data.albedo_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);

					RD::TextureFormat tf_data;
					tf_data.texture_type = RD::TEXTURE_TYPE_2D;
					tf_data.width = view.extent.x;
					tf_data.height = view.extent.y;
					tf_data.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf_data.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
					data.normal_resource = builder.create<FrameGraphTexture>("normal texture", { tf_data, RD::TextureView(), "normal texture" });
					data.normal_resource = builder.write(data.normal_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.position_resource = builder.create<FrameGraphTexture>("position texture", { tf_data, RD::TextureView(), "position texture" });
					data.position_resource = builder.write(data.position_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);

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
				[drawables = std::move(drawables), &storage, pipeline_rid](
					const offscreen_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
				{
					auto& rc = *static_cast<RenderContext*>(ctx);
					auto  cmd = rc.command_buffer;

					uint32_t w = rc.device->screen_get_width();
					uint32_t h = rc.device->screen_get_height();

					GPU_SCOPE(cmd, "Offscreen Pass", Color(1.0f, 0.0f, 0.0f, 1.0f));
					std::array<RDD::RenderPassClearValue, 4> clear_values;
					clear_values[0].color = Color(); // position
					clear_values[1].color = Color(); // normal
					clear_values[2].color = Color(); // albedo
					clear_values[3].depth = 1.0f;
					clear_values[3].stencil = 0;

					RID frame_buffer = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

					rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

					for (const auto& drawable : drawables) {
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
				"Deferred Pass",
				[&](FrameGraph::Builder& builder, deferred_pass_resource& data)
				{
					auto& offscreen_resource = bb.get<offscreen_pass_resource>();

					RD::TextureFormat tf;
					tf.texture_type = RD::TEXTURE_TYPE_2D;
					tf.width = view.extent.x;
					tf.height = view.extent.y;
					tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					data.scene = builder.create<FrameGraphTexture>("scene texture", { tf, RD::TextureView(), "scene texture" });

					// read from offscreen pass.
					data.depth = builder.read(offscreen_resource.depth_resource, TEXTURE_READ_FLAGS::READ_DEPTH);
					builder.read(offscreen_resource.position_resource, TEXTURE_READ_FLAGS::READ_COLOR);
					builder.read(offscreen_resource.normal_resource, TEXTURE_READ_FLAGS::READ_COLOR);
					builder.read(offscreen_resource.albedo_resource, TEXTURE_READ_FLAGS::READ_COLOR);

					data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);

					data.offscreen_tex_resources = builder.create<FrameGraphUniformSet>(
						"offscreen uniform set",
						{
							.build = [&fg,
										shader_rid = deferred_pipeline.shader_rid,
										albedo = offscreen_resource.albedo_resource,
										position = offscreen_resource.position_resource,
										normal = offscreen_resource.normal_resource]
									 (RenderContext& rc) -> RID {
								auto& albedo_tex = fg.get_resource<FrameGraphTexture>(albedo);
								auto& position_tex = fg.get_resource<FrameGraphTexture>(position);
								auto& normal_tex = fg.get_resource<FrameGraphTexture>(normal);

								return UniformSetBuilder{}
									.add_texture_only(0, albedo_tex.texture_rid)
									.add_texture_only(1, position_tex.texture_rid)
									.add_texture_only(2, normal_tex.texture_rid)
									.build(rc.device, shader_rid, 1);
							},
							.name = "offscreen uniform set"
						});
					data.offscreen_tex_resources = builder.write(data.offscreen_tex_resources, FrameGraph::kFlagsIgnored);

					data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
						"deferred framebuffer",
						{
							.build = [&fg, scene_id = data.scene, depth_id = data.depth](RenderContext& rc) -> RID {
								auto& scene_tex = fg.get_resource<FrameGraphTexture>(scene_id);
								auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
								return rc.device->framebuffer_create({ scene_tex.texture_rid, depth_tex.texture_rid });
							},
							.name = "deferred framebuffer"
						});
					data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
				},
				[pipeline_rid,
				 shader_rid = deferred_pipeline.shader_rid,
				 uniform_set_0_rid = (RID)uniform_set_0_deferred](
					const deferred_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
				{
					auto& rc = *static_cast<RenderContext*>(ctx);
					auto  cmd = rc.command_buffer;

					uint32_t w = rc.device->screen_get_width();
					uint32_t h = rc.device->screen_get_height();

					RID uniform_set_1 = resources.get<FrameGraphUniformSet>(data.offscreen_tex_resources).uniform_set_rid;
					RID frame_buffer = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

					GPU_SCOPE(cmd, "Deferred Pass", Color(1.0f, 0.0f, 0.0f, 1.0f));
					std::array<RDD::RenderPassClearValue, 2> clear_values;
					clear_values[0].color = Color();
					clear_values[1].depth = 1.0f;
					clear_values[1].stencil = 0;

					rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

					rc.device->bind_render_pipeline(cmd, pipeline_rid);
					rc.device->bind_uniform_set(shader_rid, uniform_set_0_rid, 0);
					rc.device->bind_uniform_set(shader_rid, uniform_set_1, 1);
					rc.device->render_draw(cmd, 3, 1);

					rc.wsi->end_render_pass(cmd);
				});
	}

	void DeferredRenderer::create_offscreen_pipeline(WSI* wsi, RenderingDevice* dev)
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

		auto main_fb_format = RD::get_singleton()->framebuffer_format_create({ position_att,
			normal_att, albedo_att, depth_att });

		RD::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test = true;
		depth_state.enable_depth_write = true;
		depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

		offscreen_pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/mrt.vert", "assets://shaders/deferred/mrt.frag" },
				"offscreen_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(depth_state)
			.set_blend_state(RDC::PipelineColorBlendState::create_disabled(3))
			.build(main_fb_format);

		sampler = RIDHandle(dev->sampler_create({}));

		// --- UBOs ---
		frame_ubo.create(dev, "Frame UBO");

		// --- Uniform sets (set 0) ---
		uniform_set_0 = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add_sampler(3, sampler)
			.build(dev, offscreen_pipeline.shader_rid, 0);
	}

	void DeferredRenderer::create_deferred_pipeline(WSI* wsi, RenderingDevice* dev)
	{
		// --- Framebuffer formats ---

		RD::AttachmentFormat color_att;
		color_att.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
		color_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		RD::AttachmentFormat depth_att;
		depth_att.format = RD::DATA_FORMAT_D32_SFLOAT;
		depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		auto main_fb_format = RD::get_singleton()->framebuffer_format_create({ color_att, depth_att });

		// --- Pipelines ---

		RDC::PipelineDepthStencilState ds_standard;
		ds_standard.enable_depth_test = false;
		ds_standard.enable_depth_write = false;
		ds_standard.depth_compare_operator = RDC::COMPARE_OP_LESS;

		deferred_pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/deferred.vert",
						  "assets://shaders/deferred/deferred.frag" }, "deferred")
			.set_blend_state(RDC::PipelineColorBlendState::create_disabled())
			.build(main_fb_format);

		// --- UBOs ---
		light_ubo.create(dev, "Light UBO");
		shadow_ubo.create(dev, "Shadow UBO");

		// --- Uniform sets (set 0) ---
		uniform_set_0_deferred = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(light_ubo.as_uniform(2))
			.add_sampler(3, sampler)
			.build(dev, deferred_pipeline.shader_rid, 0);
	}

}
