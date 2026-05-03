
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
		position_att.format = RDC::DATA_FORMAT_R16G16B16_SFLOAT;
		position_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Albedo 8
		RD::AttachmentFormat albedo_att;
		albedo_att.format = RDC::DATA_FORMAT_R8G8B8_UNORM;
		albedo_att.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		// Normal 16 (World space)
		RD::AttachmentFormat normal_att;
		normal_att.format = RDC::DATA_FORMAT_R16G16B16_SFLOAT;
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

	void Rendering::DeferredRenderer::setup_passes(FrameGraph& fg, FrameGraphBlackboard& bb, 
		const SceneView& view, MeshStorage& storage)
	{
		auto pipeline_rid = deferred_pipeline.pipeline_rid;

		throw std::logic_error("The method or operation is not implemented.");
	}

}



