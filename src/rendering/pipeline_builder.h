/*****************************************************************//**
 * \file   pipeline_builder.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include "rendering_device.h"
#include "util/small_vector.h"

namespace Rendering
{
	using RD = RenderingDevice;
	using RDC = RenderingDeviceCommons;

	// Bundles the pipeline RID and its shader RID together so callers never
	// need to do a string lookup to find the shader after building a pipeline.
	struct Pipeline {
		RID pipeline_rid;
		RID shader_rid;
	};

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
			dynamic_state_flags = {};
			rasterization_state = rs;
		}

		~PipelineBuilder() = default;

		PipelineBuilder& set_shader(const Util::SmallVector<std::string>& shaders, const std::string& name)
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

		PipelineBuilder& set_render_primitive(RDC::RenderPrimitive p_render_primitive)
		{
			render_primitive = p_render_primitive;
			return *this;
		}

		PipelineBuilder& set_blend_state(const RDC::PipelineColorBlendState& p_blend_state)
		{
			blend_state = p_blend_state;
			return *this;
		}

		PipelineBuilder& set_dynamic_state_flags(const BitField<RDC::PipelineDynamicStateFlags> p_dynamic_state_flags)
		{
			dynamic_state_flags = p_dynamic_state_flags;
			return *this;
		}

		PipelineBuilder& set_multisample_state(const RDC::PipelineMultisampleState& p_multisample_state)
		{
			multisample_state = p_multisample_state;
			return *this;
		}

		PipelineBuilder& set_depth_stencil_state(const RDC::PipelineDepthStencilState& p_depth_stencil_state)
		{
			depth_stencil_state = p_depth_stencil_state;
			return *this;
		}

		PipelineBuilder& set_specialization_constants(const Util::SmallVector<RDC::PipelineSpecializationConstant>& p_specializatoin_constants)
		{
			specialization_constants = p_specializatoin_constants;
			return *this;
		}

		Pipeline build(RD::FramebufferFormatID p_frame_buffer_format, uint32_t p_render_subpass = 0)
		{
			RID p = device->render_pipeline_create(shader, p_frame_buffer_format,
				vertex_format, render_primitive,
				rasterization_state, multisample_state,
				depth_stencil_state, blend_state, dynamic_state_flags,
				p_render_subpass, specialization_constants);
			return { p, shader };
		}

		Pipeline build_from_frame_buffer(RID p_frame_buffer, uint32_t p_render_subpass = 0)
		{
			RID p = device->render_pipeline_create_from_frame_buffer(shader, p_frame_buffer,
				vertex_format, render_primitive,
				rasterization_state, multisample_state,
				depth_stencil_state, blend_state, dynamic_state_flags,
				p_render_subpass, specialization_constants);
			return { p, shader };
		}

	private:
		RenderingDevice* device;
		RID shader;
		RD::VertexFormatID vertex_format = RDC::INVALID_ID;
		RDC::PipelineRasterizationState rasterization_state;
		RDC::RenderPrimitive render_primitive;
		RDC::PipelineColorBlendState blend_state;
		RDC::PipelineDynamicStateFlags dynamic_state_flags;
		RDC::PipelineMultisampleState multisample_state;
		RDC::PipelineDepthStencilState depth_stencil_state;
		Util::SmallVector<RDC::PipelineSpecializationConstant> specialization_constants;
	};

	class ComputePipelineBuilder
	{
	public:
		ComputePipelineBuilder()
		{
			device = RenderingDevice::get_singleton();
		}

		~ComputePipelineBuilder() = default;

		ComputePipelineBuilder& set_shader(const std::string& shader_path, const std::string& name)
		{
			Util::SmallVector<std::string> shaders;
			shaders.push_back(shader_path);
			shader = device->create_program(name, shaders);
			return *this;
		}

		ComputePipelineBuilder& set_shader(RID p_shader)
		{
			shader = p_shader;
			return *this;
		}

		ComputePipelineBuilder& set_specialization_constants(const Util::SmallVector<RDC::PipelineSpecializationConstant>& p_specialization_constants)
		{
			specialization_constants = p_specialization_constants;
			return *this;
		}

		Pipeline build()
		{
			RID p = device->compute_pipeline_create(shader, specialization_constants);
			return { p, shader };
		}

	private:
		RenderingDevice* device;
		RID shader;
		Util::SmallVector<RDC::PipelineSpecializationConstant> specialization_constants;
	};
}
