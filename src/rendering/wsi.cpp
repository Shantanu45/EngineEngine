/*****************************************************************//**
 * \file   wsi.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "wsi.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_device.h"
#include "libassert/assert.hpp"
#include "compiler/compiler.h"

namespace Rendering
{
	WSI::WSI()
	{

	}

	Error WSI::initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window)
	{
		active_window = DisplayServerEnums::MAIN_WINDOW_ID;
		rendering_driver = p_rendering_driver;
		if (rendering_driver == "vulkan") {
			rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
		}
		else
		{
			// api not supported
			DEBUG_ASSERT(false);
			return FAILED;
		}

		if (rendering_context != nullptr) {
			if (!rendering_context->init_loader_and_extensions(windows[active_window].platfform_data.platform))
			{
				LOGE("Failed to initialize Vulkan loader.\n");
				return FAILED;
			}
			if (rendering_context->initialize() == OK && _create_rendering_context_window(active_window, rendering_driver) == OK)
			{
				rendering_device = RenderingDevice::get_singleton();
				// device initialization happens in function call below
				if (!(rendering_device->initialize(rendering_context.get(), active_window) == OK))
				{
					return FAILED;
				}
			}
		}
		return OK;
	}

	bool WSI::pre_frame_loop()
	{
		if (rendering_context && rendering_device) {
			
			DEV_ASSERT(rendering_device != nullptr);

			rendering_device->screen_create(active_window);

			set_program({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" });

			pipeline_create_default();

			return true;
		}
		return false;
	}

	bool WSI::pre_begin_frame()
	{
		// TODO
		return false;
	}

	bool WSI::begin_frame()
	{
		rendering_device->begin_frame();
		rendering_device->screen_prepare_for_drawing(active_window);

		return true;
	}

	bool WSI::end_frame()
	{
		rendering_device->swap_buffers(true);

		return true;
	}

	bool WSI::post_end_frame()
	{
		// TODO
		return false;
	}

	bool WSI::post_frame_loop()
	{
		// TODO
		return false;
	}

	void WSI::set_program(const std::vector<std::string> programs)
	{
		RDShaderSource* shaders = new RDShaderSource();
		shaders->set_language(RenderingDeviceCommons::SHADER_LANGUAGE_GLSL);
		for (auto shader_path: programs)
		{
			auto stage = shader_stage_from_compiler_stage(Compiler::stage_from_path(shader_path));
			ERR_FAIL_COND_MSG(stage == RenderingDeviceCommons::SHADER_STAGE_MAX, "could not evaluate shader stage from path!!");
			shaders->set_stage_source(stage, shader_path);
		}
		shader_program = rendering_device->shader_create_from_spirv(rendering_device->shader_compile_spirv_from_shader_source(shaders), "traingle_shader");
	}

	void WSI::set_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride)
	{
		RenderingDeviceCommons::VertexAttribute va;
		va.format = format;
		va.stride = stride;// RenderingDeviceCommons::get_format_vertex_size(format);
		//va.stride = sizeof(float) * 6;
		va.binding = binding;
		va.location = location;
		va.offset = offset;
		vertex_attributes.push_back(va);
	}

	RID WSI::get_current_pipeline()
	{
		return pipeline;
	}

	RenderingShaderContainerFormat* WSI::create_shader_container_format() 
	{
		return new ::Vulkan::RenderingShaderContainerFormatVulkan();
	}
 
	void WSI::bind_vbo_and_ibo()
	{
		rendering_device->bind_vertex_array(triangle_vertex_array);
		rendering_device->bind_index_array(triangle_index_array);
	}

	void WSI::set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data)
	{
		windows.insert({ window, data });
	}

	void WSI::push_vertex_data(void* data, size_t size)
	{
		const size_t offset = vertex_data.size();
		vertex_data.resize(offset + size);
		memcpy(vertex_data.data() + offset, data, size);
	}

	void WSI::set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat format)
	{
		index_data_format = format;
	}

	void WSI::push_index_data(void* data, size_t size)
	{
		const size_t offset = index_data.size();
		index_data.resize(offset + size);
		memcpy(index_data.data() + offset, data, size);
		
		switch (index_data_format)
		{
		case Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16:
			index_count = index_data.size() / sizeof(uint16_t);
			break;
		case Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32:
			index_count = index_data.size() / sizeof(uint32_t);
			break;
		default:
			index_count = 0;
			break;
		}
	}

	void WSI::pipeline_create()
	{
		auto fb_format = rendering_device->screen_get_framebuffer_format(active_window);

		auto vertex_format = rendering_device->vertex_format_create({});

		auto blend_state = RenderingDeviceCommons::PipelineColorBlendState::create_blend();
		pipeline = rendering_device->render_pipeline_create( shader_program, fb_format,
			vertex_format, RenderingDeviceCommons::RENDER_PRIMITIVE_TRIANGLE_STRIPS, 
			{}, RenderingDeviceCommons::PipelineMultisampleState(),
			RenderingDeviceCommons::PipelineDepthStencilState(), blend_state,
			0);
	}

	void WSI::pipeline_create_default()
	{
		DEBUG_ASSERT(!shader_program.is_null());
		DEBUG_ASSERT(!vertex_attributes.empty());
		vertex_format = rendering_device->vertex_format_create(vertex_attributes);
		RenderingDeviceCommons::PipelineRasterizationState rs;
		rs.front_face = RenderingDeviceCommons::POLYGON_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.cull_mode = RenderingDeviceCommons::POLYGON_CULL_BACK;

		auto blend_state = RenderingDeviceCommons::PipelineColorBlendState::create_blend();
		pipeline = rendering_device->create_swapchain_pipeline(active_window, shader_program,
			vertex_format, RenderingDeviceCommons::RENDER_PRIMITIVE_TRIANGLE_STRIPS,
			rs, RenderingDeviceCommons::PipelineMultisampleState(),
			RenderingDeviceCommons::PipelineDepthStencilState(), blend_state,
			0);

		_create_vertex_and_index_buffers();
		rendering_device->_submit_transfer_workers();
	}

	void WSI::teardown()
	{
	}

	WSI::~WSI()
	{

	}

	Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
	{
		// TODO: check if window entry exists

		auto wd = windows.at(p_window_id);

		Error err = rendering_context->window_create(p_window_id, &wd.platfform_data);
		ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create {} window.", p_rendering_driver));
		rendering_context->window_set_size(p_window_id, wd.window_resolution.x, wd.window_resolution.y);
		surface = rendering_context->surface_get_from_window(p_window_id);
		return OK;
	}

	std::vector<uint8_t> WSI::_get_attrib_interleaved(const std::vector<RenderingDeviceCommons::VertexAttribute>& attribs, std::vector<uint8_t> vertex_data)
	{
		std::vector<uint8_t> interleved_data;
		uint32_t vert_num = vertex_data.size() / attribs[0].stride;
		uint32_t stride = attribs[0].stride;
		interleved_data.resize(vertex_data.size());
		for (int v = 0; v < vert_num; v++)
		{
			auto vert_pos = v;
			uint32_t dst_offset;
			uint32_t src_offset;

			auto src_attrib_offset = 0;
			// for each vertex
			for (int i = 0; i < attribs.size() - 1; i++)
			{
				auto size = attribs[i + 1].offset - attribs[i].offset;		// size of the attribute

				dst_offset = (vert_pos * stride) + attribs[i].offset;
				src_offset = (i  * src_attrib_offset) + (vert_pos * size);

				memcpy(interleved_data.data() + dst_offset, vertex_data.data() + src_offset, size);
				LOGI("dst %d, src %d, size %d", dst_offset, src_offset, size);
				src_attrib_offset += (size * vert_num);
			}
			auto last_attrib_size = attribs.back().stride - attribs.back().offset;

			dst_offset = (vert_pos * stride) + attribs.back().offset;
			src_offset = (src_attrib_offset) + (vert_pos * last_attrib_size);

			memcpy(interleved_data.data() + dst_offset, vertex_data.data() + src_offset, last_attrib_size);
			LOGI("last dst %d, src %d, size %d", dst_offset, src_offset, last_attrib_size);

		}

		return interleved_data;
	}

	void WSI::_create_vertex_and_index_buffers()
	{
		DEBUG_ASSERT(index_count > 0);
		DEBUG_ASSERT(!vertex_data.empty());

		PackedByteArray interleved;

		if (vertex_data_mode == VERTEX_DATA_MODE::SEPERATE)
		{
			 interleved = _get_attrib_interleaved(vertex_attributes, vertex_data);
		}
		else
		{
			interleved = vertex_data;
		}
		uint32_t triangle_vertex_count = vertex_data.size() / vertex_attributes[0].stride;
		triangle_vertex_buffer = rendering_device->vertex_buffer_create(interleved.size(), interleved);

		std::vector<RID> buffers;
		buffers.push_back(triangle_vertex_buffer);

		triangle_vertex_array = rendering_device->vertex_array_create(triangle_vertex_count, vertex_format, buffers);

		triangle_index_buffer = rendering_device->index_buffer_create(index_count, index_data_format, index_data);

		triangle_index_array = rendering_device->index_array_create(triangle_index_buffer, 0, index_count);
		
	}

}
