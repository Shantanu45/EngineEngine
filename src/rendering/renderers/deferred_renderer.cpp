
#include "deferred_renderer.h"
#include <rendering/uniform_set_builder.h>
#include <rendering/drawable.h>
#include <rendering/wsi.h>
#include "rendering/render_passes/shadow_passes.h"
#include "rendering/shadow_system.h"
#include "math/rect2.h"
#include "util/small_vector.h"

#include <iterator>

namespace Rendering
{
	void DeferredRenderer::initialize(WSI* wsi, RenderingDevice* dev, RID cubemap)
	{
		device = dev;
		create_offscreen_pipeline(wsi, dev);
		create_deferred_pipeline(wsi, dev, cubemap);
		debug_renderer.initialize(dev);
	}

	void DeferredRenderer::shutdown()
	{
		uniform_set_0_deferred_regular.reset();
		uniform_set_0_deferred.reset();
		uniform_set_skybox.reset();
		uniform_set_0_point_shadow.reset();
		uniform_set_0_shadow.reset();
		uniform_set_0_light.reset();
		uniform_set_0_pbr.reset();
		uniform_set_0.reset();

		point_shadow_sampler.reset();
		shadow_sampler.reset();
		sampler_cube.reset();
		gbuffer_sampler.reset();
		sampler.reset();

		shadow_ubo.free();
		light_ubo.free();
		frame_ubo.free();

		device = nullptr;
	}

	void DeferredRenderer::setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb,
		const SceneView& view, MeshStorage& storage)
	{
		const ShadowBuildResult shadow_build = ShadowSystem::build_shadow_buffer(view.lights);
		shadow_ubo.upload(device, shadow_build.buffer);

		// Upload per-frame UBOs
		{
			FrameData_UBO frame{};
			frame.camera                  = view.camera;
			frame.time                     = static_cast<float>(view.elapsed);
			frame.directional_shadow_index = shadow_build.directional_shadow_index;
			frame.point_shadow_index       = shadow_build.point_shadow_index;
			frame.material_debug_view      = static_cast<uint32_t>(view.material_debug_view);
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

		auto shadow_draws = ShadowSystem::build_shadow_drawables(
			view,
			{ ShadowProjection::Directional2D, pipeline_shadow, (RID)uniform_set_0_shadow });
		auto point_shadow_draws = ShadowSystem::build_shadow_drawables(
			view,
			{ ShadowProjection::PointCube, pipeline_point_shadow, (RID)uniform_set_0_point_shadow });

		add_point_shadow_pass(fg, bb, 1024, point_shadow_draws, storage);
		add_shadow_pass(fg, bb, { 2048, 2048 }, shadow_draws, storage);
		setup_offscreen_pass(fg, bb, view, storage);
		setup_deferred_pass(fg, bb, view, storage);
		setup_overlay_pass(fg, bb, view, storage);

		auto& deferred = bb.get<deferred_pass_resource>();
		debug_renderer.add_pass(fg, bb, deferred.scene, deferred.depth, view.camera, view.extent);
	}

	std::vector<Drawable> DeferredRenderer::build_overlay_drawables(const SceneView& view) const
	{
		std::vector<Drawable> out;
		out.reserve(view.instances.size() + 2);
		std::vector<Drawable> visualizer_drawables;
		visualizer_drawables.reserve(view.instances.size());

		glm::mat4 identity = glm::mat4(1.0f);

		if (view.skybox_mesh != INVALID_MESH)
			out.push_back(Drawable::make(pipeline_skybox, view.skybox_mesh,
				PushConstantData::from(ObjectData_UBO{ identity, identity }),
				{ { (RID)uniform_set_skybox, 0 } }));

		if (view.grid_mesh != INVALID_MESH)
			out.push_back(Drawable::make(pipeline_grid, view.grid_mesh,
				PushConstantData::from(ObjectData_UBO{ identity, glm::transpose(glm::inverse(identity)) }),
				{ { (RID)uniform_set_0_light, 0 } }));

		for (const auto& inst : view.instances) {
			if (inst.category == MeshCategory::Opaque)
				continue;

			visualizer_drawables.push_back(Drawable::make(
				pipeline_light, inst.mesh,
				PushConstantData::from(ObjectData_UBO{ inst.model, inst.normal_matrix }),
				{ { (RID)uniform_set_0_light, 0 } },
				inst.material_sets
			));
		}

		sort_drawables_for_state_reuse(visualizer_drawables);
		out.insert(out.end(),
			std::make_move_iterator(visualizer_drawables.begin()),
			std::make_move_iterator(visualizer_drawables.end()));

		return out;
	}

	void DeferredRenderer::setup_offscreen_pass(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage)
	{
		// Build drawables from instances — only opaque geometry goes into the G-buffer.
		std::vector<Drawable> drawables;
		drawables.reserve(view.instances.size());
		for (const auto& inst : view.instances) {
			if (inst.category != MeshCategory::Opaque) continue;
			drawables.push_back(Drawable::make(
				pipeline_color, inst.mesh,
				PushConstantData::from(ObjectData_UBO{ inst.model, inst.normal_matrix }),
				{ { (RID)uniform_set_0, 0 } },
				inst.material_sets
			));
		}
		sort_drawables_for_state_reuse(drawables);

		auto pipeline_rid = pipeline_color.pipeline_rid;

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
					tf_data.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					data.normal_resource = builder.create<FrameGraphTexture>("normal texture", { tf_data, RD::TextureView(), "normal texture" });
					data.normal_resource = builder.write(data.normal_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.position_resource = builder.create<FrameGraphTexture>("position texture", { tf_data, RD::TextureView(), "position texture" });
					data.position_resource = builder.write(data.position_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.material_resource = builder.create<FrameGraphTexture>("material texture", { tf_color, RD::TextureView(), "material texture" });
					data.material_resource = builder.write(data.material_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
					data.emissive_resource = builder.create<FrameGraphTexture>("emissive texture", { tf_color, RD::TextureView(), "emissive texture" });
					data.emissive_resource = builder.write(data.emissive_resource, TEXTURE_WRITE_FLAGS::WRITE_COLOR);

					// framebuffer setup
					data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
						"offscreen framebuffer",
						{
							.build = [&fg, albedo = data.albedo_resource, normal = data.normal_resource,
							          position = data.position_resource, material = data.material_resource,
							          emissive = data.emissive_resource, depth_id = data.depth_resource](RenderContext& rc) -> RID {
								auto& position_tex = fg.get_resource<FrameGraphTexture>(position);
								auto& normal_tex = fg.get_resource<FrameGraphTexture>(normal);
								auto& albedo_tex = fg.get_resource<FrameGraphTexture>(albedo);
								auto& material_tex = fg.get_resource<FrameGraphTexture>(material);
								auto& emissive_tex = fg.get_resource<FrameGraphTexture>(emissive);
								auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
								return rc.device->framebuffer_create({
									position_tex.texture_rid,
									normal_tex.texture_rid,
									albedo_tex.texture_rid,
									material_tex.texture_rid,
									emissive_tex.texture_rid,
									depth_tex.texture_rid });
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
					std::array<RDD::RenderPassClearValue, 6> clear_values;
					clear_values[0].color = Color(); // position
					clear_values[1].color = Color(); // normal
					clear_values[2].color = Color(); // albedo
					clear_values[3].color = Color(); // material
					clear_values[4].color = Color(); // emissive
					clear_values[5].depth = 1.0f;
					clear_values[5].stencil = 0;

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
		const Pipeline active_pipeline = view.use_pbr_lighting ? deferred_pipeline : deferred_regular_pipeline;
		auto pipeline_rid = active_pipeline.pipeline_rid;
		auto shader_rid = active_pipeline.shader_rid;
		auto uniform_set_0_rid = view.use_pbr_lighting
			? (RID)uniform_set_0_deferred
			: (RID)uniform_set_0_deferred_regular;

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
					builder.read(offscreen_resource.material_resource, TEXTURE_READ_FLAGS::READ_COLOR);
					builder.read(offscreen_resource.emissive_resource, TEXTURE_READ_FLAGS::READ_COLOR);

					data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);

					auto& shadow_res       = bb.get<shadow_pass_resource>();
					auto& point_shadow_res = bb.get<point_shadow_pass_resource>();
					data.shadow_map_in     = builder.read(shadow_res.shadow_map, TEXTURE_READ_FLAGS::READ_DEPTH);
					data.point_shadow_in   = builder.read(point_shadow_res.shadow_cubemap, TEXTURE_READ_FLAGS::READ_DEPTH);

					data.offscreen_tex_resources = builder.create<FrameGraphUniformSet>(
						"offscreen uniform set",
						{
							.build = [&fg,
										shader_rid,
										albedo = offscreen_resource.albedo_resource,
										position = offscreen_resource.position_resource,
										normal = offscreen_resource.normal_resource,
										material = offscreen_resource.material_resource,
										emissive = offscreen_resource.emissive_resource]
									 (RenderContext& rc) -> RID {
								auto& albedo_tex = fg.get_resource<FrameGraphTexture>(albedo);
								auto& position_tex = fg.get_resource<FrameGraphTexture>(position);
								auto& normal_tex = fg.get_resource<FrameGraphTexture>(normal);
								auto& material_tex = fg.get_resource<FrameGraphTexture>(material);
								auto& emissive_tex = fg.get_resource<FrameGraphTexture>(emissive);

								return UniformSetBuilder{}
									.add_texture_only(0, albedo_tex.texture_rid)
									.add_texture_only(1, position_tex.texture_rid)
									.add_texture_only(2, normal_tex.texture_rid)
									.add_texture_only(3, material_tex.texture_rid)
									.add_texture_only(4, emissive_tex.texture_rid)
									.build_cached(rc.device, shader_rid, 1);
							},
							.name = "offscreen uniform set",
							.owns_rid = false
						});
					data.offscreen_tex_resources = builder.write(data.offscreen_tex_resources, FrameGraph::kFlagsIgnored);

					data.shadow_uniform_set = builder.create<FrameGraphUniformSet>(
						"deferred shadow uniform set",
						{
							.build = [&fg,
							          shader_rid,
							          shadow_id = shadow_res.shadow_map,
							          point_id = point_shadow_res.shadow_cubemap]
							         (RenderContext& rc) -> RID {
								auto& shadow_tex = fg.get_resource<FrameGraphTexture>(shadow_id);
								auto& point_shadow_tex = fg.get_resource<FrameGraphTexture>(point_id);
								return UniformSetBuilder{}
									.add_texture_only(0, shadow_tex.texture_rid)
									.add_texture_only(1, point_shadow_tex.texture_rid)
									.build_cached(rc.device, shader_rid, 2);
							},
							.name = "deferred shadow uniform set",
							.owns_rid = false
						});
					data.shadow_uniform_set = builder.write(data.shadow_uniform_set, FrameGraph::kFlagsIgnored);

					data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
						"deferred framebuffer",
						{
							.build = [&fg, scene_id = data.scene](RenderContext& rc) -> RID {
								auto& scene_tex = fg.get_resource<FrameGraphTexture>(scene_id);
								return rc.device->framebuffer_create({ scene_tex.texture_rid });
							},
							.name = "deferred framebuffer"
						});
					data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
				},
				[pipeline_rid,
				 shader_rid,
				 uniform_set_0_rid](
					const deferred_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
				{
					auto& rc = *static_cast<RenderContext*>(ctx);
					auto  cmd = rc.command_buffer;

					uint32_t w = rc.device->screen_get_width();
					uint32_t h = rc.device->screen_get_height();

					RID uniform_set_1 = resources.get<FrameGraphUniformSet>(data.offscreen_tex_resources).uniform_set_rid;
					RID uniform_set_2 = resources.get<FrameGraphUniformSet>(data.shadow_uniform_set).uniform_set_rid;
					RID frame_buffer = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

					GPU_SCOPE(cmd, "Deferred Pass", Color(1.0f, 0.0f, 0.0f, 1.0f));
					std::array<RDD::RenderPassClearValue, 1> clear_values;
					clear_values[0].color = Color();

					rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

					rc.device->bind_render_pipeline(cmd, pipeline_rid);
					rc.device->bind_uniform_set(shader_rid, uniform_set_0_rid, 0);
					rc.device->bind_uniform_set(shader_rid, uniform_set_1, 1);
					rc.device->bind_uniform_set(shader_rid, uniform_set_2, 2);
					rc.device->render_draw(cmd, 3, 1);

					rc.wsi->end_render_pass(cmd);
				});
	}

	void DeferredRenderer::setup_overlay_pass(FrameGraph& fg, FrameGraphBlackboard& bb, const SceneView& view, MeshStorage& storage)
	{
		auto drawables = build_overlay_drawables(view);
		if (drawables.empty())
			return;

		struct OverlayPassData {
			FrameGraphResource scene;
			FrameGraphResource depth;
			FrameGraphResource framebuffer_resource;
		};

		fg.add_callback_pass<OverlayPassData>(
			"Deferred Overlay Pass",
			[&](FrameGraph::Builder& builder, OverlayPassData& data)
			{
				auto& deferred = bb.get<deferred_pass_resource>();
				data.scene = builder.write(deferred.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR_LOAD);
				data.depth = builder.write(deferred.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH_LOAD);

				deferred.scene = data.scene;
				deferred.depth = data.depth;

				data.framebuffer_resource = builder.create<FrameGraphFramebuffer>(
					"deferred overlay framebuffer",
					{
						.build = [&fg, scene_id = data.scene, depth_id = data.depth](RenderContext& rc) -> RID {
							auto& scene_tex = fg.get_resource<FrameGraphTexture>(scene_id);
							auto& depth_tex = fg.get_resource<FrameGraphTexture>(depth_id);
							return rc.device->framebuffer_create_load({ scene_tex.texture_rid, depth_tex.texture_rid });
						},
						.name = "deferred overlay framebuffer"
					});
				data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
			},
			[drawables = std::move(drawables), &storage, extent = view.extent](
				const OverlayPassData& data, FrameGraphPassResources& resources, void* ctx)
			{
				auto& rc = *static_cast<RenderContext*>(ctx);
				auto  cmd = rc.command_buffer;

				RID frame_buffer = resources.get<FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

				GPU_SCOPE(cmd, "Deferred Overlay Pass", Color(0.2f, 0.7f, 1.0f, 1.0f));
				std::array<RDD::RenderPassClearValue, 2> clear_values;
				clear_values[0].color = Color();
				clear_values[1].depth = 1.0f;
				clear_values[1].stencil = 0;

				rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, extent.x, extent.y), clear_values);

				for (const auto& drawable : drawables) {
					submit_drawable(rc, cmd, drawable, storage);
				}

				rc.wsi->end_render_pass(cmd);
			});
	}

	void DeferredRenderer::create_offscreen_pipeline(WSI* wsi, RenderingDevice* dev)
	{
		auto vertex_format = wsi->get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS::DEFAULT);

		// Framebuffers -----------------------
		// Position (World space)
		RD::AttachmentFormat position_att;
		position_att.format = RDC::DATA_FORMAT_R16G16B16A16_SFLOAT;
		position_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Albedo 8
		RD::AttachmentFormat albedo_att;
		albedo_att.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;
		albedo_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Normal 16 (World space)
		RD::AttachmentFormat normal_att;
		normal_att.format = RDC::DATA_FORMAT_R16G16B16A16_SFLOAT;
		normal_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Depth
		RD::AttachmentFormat depth_att;
		depth_att.format = RDC::DATA_FORMAT_D32_SFLOAT;
		depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		auto main_fb_format = RD::get_singleton()->framebuffer_format_create({ position_att,
			normal_att, albedo_att, albedo_att, albedo_att, depth_att });

		RD::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test = true;
		depth_state.enable_depth_write = true;
		depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

		pipeline_color = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/mrt.vert", "assets://shaders/deferred/mrt.frag" },
				"offscreen_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(depth_state)
			.set_blend_state(RDC::PipelineColorBlendState::create_disabled(5))
			.build(main_fb_format);
		pipeline_pbr = pipeline_color;

		{
			RDC::SamplerState ss;
			ss.mag_filter     = RDC::SAMPLER_FILTER_LINEAR;
			ss.min_filter     = RDC::SAMPLER_FILTER_LINEAR;
			ss.mip_filter     = RDC::SAMPLER_FILTER_LINEAR;
			ss.repeat_u       = RDC::SAMPLER_REPEAT_MODE_REPEAT;
			ss.repeat_v       = RDC::SAMPLER_REPEAT_MODE_REPEAT;
			ss.repeat_w       = RDC::SAMPLER_REPEAT_MODE_REPEAT;
			ss.use_anisotropy = true;
			ss.anisotropy_max = 16.0f;
			sampler = RIDHandle(dev->sampler_create(ss));
		}
		gbuffer_sampler = RIDHandle(dev->sampler_create({}));
		point_shadow_sampler = RIDHandle(dev->sampler_create({}));

		{
			RDC::SamplerState ss;
			ss.mag_filter     = RDC::SAMPLER_FILTER_LINEAR;
			ss.min_filter     = RDC::SAMPLER_FILTER_LINEAR;
			ss.enable_compare = true;
			ss.compare_op     = RDC::COMPARE_OP_LESS_OR_EQUAL;
			shadow_sampler = RIDHandle(dev->sampler_create(ss));
		}

		RD::AttachmentFormat shadow_att;
		shadow_att.format      = RD::DATA_FORMAT_D32_SFLOAT;
		shadow_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		auto shadow_fb_format  = RD::get_singleton()->framebuffer_format_create({ shadow_att });

		{
			RDC::PipelineRasterizationState rs_shadow;
			rs_shadow.cull_mode = RDC::POLYGON_CULL_FRONT;
			pipeline_shadow = PipelineBuilder{}
				.set_shader({ "assets://shaders/shadow.vert",
				              "assets://shaders/shadow.frag" }, "shadow_shader")
				.set_vertex_format(vertex_format)
				.set_depth_stencil_state(depth_state)
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
			.set_depth_stencil_state(depth_state)
			.build(shadow_fb_format);

		// --- UBOs ---
		frame_ubo.create(dev, "Frame UBO");
		shadow_ubo.create(dev, "Shadow UBO");

		// --- Uniform sets (set 0) ---
		uniform_set_0 = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add_sampler(3, sampler)
			.build(dev, pipeline_color.shader_rid, 0);

		uniform_set_0_shadow = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(shadow_ubo.as_uniform(1))
			.add_sampler(3, sampler)
			.build(dev, pipeline_shadow.shader_rid, 0);

		uniform_set_0_point_shadow = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(shadow_ubo.as_uniform(1))
			.add_sampler(3, sampler)
			.build(dev, pipeline_point_shadow.shader_rid, 0);
	}

	void DeferredRenderer::create_deferred_pipeline(WSI* wsi, RenderingDevice* dev, RID cubemap)
	{
		auto vertex_format = wsi->get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS::DEFAULT);

		// --- Framebuffer formats ---

		RD::AttachmentFormat color_att;
		color_att.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
		color_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		auto deferred_fb_format = RD::get_singleton()->framebuffer_format_create({ color_att });

		RD::AttachmentFormat depth_att;
		depth_att.format = RD::DATA_FORMAT_D32_SFLOAT;
		depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		auto overlay_fb_format = RD::get_singleton()->framebuffer_format_create({ color_att, depth_att });

		// --- Pipelines ---

		RDC::PipelineDepthStencilState ds_standard;
		ds_standard.enable_depth_test = false;
		ds_standard.enable_depth_write = false;
		ds_standard.depth_compare_operator = RDC::COMPARE_OP_LESS;

		deferred_pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/deferred.vert",
						  "assets://shaders/deferred/deferred.frag" }, "deferred")
			.set_blend_state(RDC::PipelineColorBlendState::create_disabled())
			.build(deferred_fb_format);

		deferred_regular_pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/deferred/deferred.vert",
						  "assets://shaders/deferred/deferred_regular.frag" }, "deferred_regular")
			.set_blend_state(RDC::PipelineColorBlendState::create_disabled())
			.build(deferred_fb_format);

		RDC::PipelineDepthStencilState ds_overlay;
		ds_overlay.enable_depth_test      = true;
		ds_overlay.enable_depth_write     = true;
		ds_overlay.depth_compare_operator = RDC::COMPARE_OP_LESS;

		pipeline_light = PipelineBuilder{}
			.set_shader({ "assets://shaders/light_cube.vert",
			              "assets://shaders/light_cube.frag" }, "deferred_light_cube_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(ds_overlay)
			.build(overlay_fb_format);

		pipeline_grid = PipelineBuilder{}
			.set_shader({ "assets://shaders/grid.vert",
			              "assets://shaders/grid.frag" }, "deferred_grid_shader")
			.set_vertex_format(vertex_format)
			.set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
			.set_depth_stencil_state(ds_overlay)
			.build(overlay_fb_format);

		{
			RDC::PipelineDepthStencilState ds_skybox;
			ds_skybox.enable_depth_test      = true;
			ds_skybox.enable_depth_write     = false;
			ds_skybox.depth_compare_operator = RDC::COMPARE_OP_LESS_OR_EQUAL;
			RDC::PipelineRasterizationState rs_skybox;
			rs_skybox.cull_mode = RDC::POLYGON_CULL_DISABLED;
			pipeline_skybox = PipelineBuilder{}
				.set_shader({ "assets://shaders/skybox.vert",
				              "assets://shaders/skybox.frag" }, "deferred_skybox_shader")
				.set_vertex_format(vertex_format)
				.set_depth_stencil_state(ds_skybox)
				.set_rasterization_state(rs_skybox)
				.build(overlay_fb_format);
		}

		// --- UBOs ---
		light_ubo.create(dev, "Light UBO");
		sampler_cube = RIDHandle(dev->sampler_create({}));

		// --- Uniform sets (set 0) ---
		uniform_set_0_deferred = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(shadow_ubo.as_uniform(1))
			.add(light_ubo.as_uniform(2))
			.add_sampler(3, gbuffer_sampler)
			.add_sampler(4, shadow_sampler)
			.add_sampler(5, point_shadow_sampler)
			.build(dev, deferred_pipeline.shader_rid, 0);

		uniform_set_0_deferred_regular = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(shadow_ubo.as_uniform(1))
			.add(light_ubo.as_uniform(2))
			.add_sampler(3, gbuffer_sampler)
			.add_sampler(4, shadow_sampler)
			.add_sampler(5, point_shadow_sampler)
			.build(dev, deferred_regular_pipeline.shader_rid, 0);

		uniform_set_0_light = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.build(dev, pipeline_light.shader_rid, 0);

		uniform_set_skybox = UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add_texture(1, sampler_cube, cubemap)
			.build(dev, pipeline_skybox.shader_rid, 0);
	}

}
