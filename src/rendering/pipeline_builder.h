/*****************************************************************//**
 * \file   pipeline_builder.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include "rendering_device.h"

namespace Rendering
{
	using RD = RenderingDevice;
	using RDC = RenderingDeviceCommons;
	class PipelineBuilder
	{
	public:
		PipelineBuilder() : 
			multisample_state(RDC::PipelineMultisampleState()),
			depth_stencil_state(RDC::PipelineDepthStencilState()),
			blend_state(RDC::PipelineColorBlendState::create_blend()),
			render_primitive(RDC::RENDER_PRIMITIVE_TRIANGLES)
		{
			device = RenderingDevice::get_singleton();

			RenderingDeviceCommons::PipelineRasterizationState rs;
			rs.front_face = RenderingDeviceCommons::POLYGON_FRONT_FACE_COUNTER_CLOCKWISE;
			rs.cull_mode = RenderingDeviceCommons::POLYGON_CULL_BACK;
			rasterization_state = rs;
		}

		~PipelineBuilder() = default;

		PipelineBuilder& set_shader(const std::vector<std::string>& shaders, const std::string& name)
		{
			shader = device->create_program(name, shaders);
			return *this;
		}

		PipelineBuilder& set_vertex_format(const RD::VertexFormatID p_vertex_format)
		{
			vertex_format = p_vertex_format;
			return *this;
		}

		PipelineBuilder& set_rasterization_state(RDC::PipelineRasterizationState& p_state)
		{
			rasterization_state = p_state;
			return *this;
		}

		PipelineBuilder& set_render_primitive(RDC::RenderPrimitive& p_render_primitive)
		{
			render_primitive = p_render_primitive;
			return *this;
		}

		PipelineBuilder& set_blend_state(const RDC::PipelineColorBlendState& p_blend_state)
		{
			blend_state = p_blend_state;
		}

		PipelineBuilder& set_dynamic_state_flags(const BitField<RDC::PipelineDynamicStateFlags> p_dynamic_state_flags)
		{
			dynamic_state_flags = p_dynamic_state_flags;
		}

		PipelineBuilder& set_multisample_state(const RDC::PipelineMultisampleState& p_multisample_state)
		{
			multisample_state = p_multisample_state;
		}

		PipelineBuilder& set_depth_stencil_state(const RDC::PipelineDepthStencilState& p_depth_stencil_state)
		{
			depth_stencil_state = p_depth_stencil_state;
		}

		PipelineBuilder& set_specialization_constants(const std::vector<RDC::PipelineSpecializationConstant>& p_specializatoin_constants)
		{
			specialization_constants = p_specializatoin_constants;
		}

		RID build(RD::FramebufferFormatID p_frame_buffer_format, uint32_t p_render_subpass = 0)
		{
			return device->render_pipeline_create(shader, p_frame_buffer_format,
				vertex_format, render_primitive,
				rasterization_state, multisample_state,
				depth_stencil_state, blend_state, dynamic_state_flags,
				p_render_subpass, specialization_constants);
		}

	private:
		RenderingDevice* device;
		RID shader;
		RD::VertexFormatID vertex_format;
		RDC::PipelineRasterizationState rasterization_state;
		RDC::RenderPrimitive render_primitive;
		RDC::PipelineColorBlendState blend_state;
		RDC::PipelineDynamicStateFlags dynamic_state_flags;
		RDC::PipelineMultisampleState multisample_state;
		RDC::PipelineDepthStencilState depth_stencil_state;
		std::vector<RDC::PipelineSpecializationConstant> specialization_constants;
	};
}