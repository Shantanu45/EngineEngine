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
		auto window_id = DisplayServerEnums::MAIN_WINDOW_ID;
		rendering_driver = p_rendering_driver;
		if (rendering_driver == "vulkan") {
			rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
		}
		else
		{
			// api not supported
			DEBUG_ASSERT(false);
		}

		if (rendering_context != nullptr) {
			if (!rendering_context->init_loader(windows[window_id].platfform_data.platform))
			{
				LOGE("Failed to initialize Vulkan loader.\n");
				return FAILED;
			}
			//rendering_context->set_platform_surface_extension(windows[window_id].platform_instance_extensions);		// TODO: remove hardcoded zero
			if (rendering_context->initialize() == OK) {
				//DEBUG_ASSERT(platform || main_window_created);

				if (_create_rendering_context_window(window_id, rendering_driver) == OK) {
					rendering_device = RenderingDevice::get_singleton();
					// device initialization happens in function call below
					if (!(rendering_device->initialize(rendering_context.get(), window_id) == OK)) {
						return FAILED;
					}
				}
			}
		}

		if (rendering_context) {
			DEV_ASSERT(rendering_device != nullptr);

			rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
		}
		return OK;
	}

	bool WSI::begin_frame()
	{
		rendering_device->begin_frame();
		return true;
	}

	// draw viewport
	void WSI::draw_viewport(bool p_swap_buffers)
	{
		// blit_render_targets_to_screen
			// screen_prepare_for_drawing
		rendering_device->screen_prepare_for_drawing(DisplayServerEnums::MAIN_WINDOW_ID);
		
	}

	bool WSI::end_frame()
	{
		rendering_device->swap_buffers(true);
		//platform->poll_input();
		return true;
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

	void WSI::pipeline_create()
	{
		auto fb_format = rendering_device->screen_get_framebuffer_format(DisplayServerEnums::MAIN_WINDOW_ID);

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
		std::vector<RenderingDeviceCommons::VertexAttribute> attributes;
		{
			RenderingDeviceCommons::VertexAttribute va;
			va.format = RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT;
			va.stride = sizeof(float) * 3;
			va.binding = 0;
			attributes.push_back(va);
		}
		vertex_format = rendering_device->vertex_format_create(attributes);
		//auto vertex_format = rendering_device->vertex_format_create({});

		auto blend_state = RenderingDeviceCommons::PipelineColorBlendState::create_blend();
		pipeline = rendering_device->create_swapchain_pipeline(DisplayServerEnums::MAIN_WINDOW_ID, shader_program,
			vertex_format, RenderingDeviceCommons::RENDER_PRIMITIVE_TRIANGLE_STRIPS,
			{}, RenderingDeviceCommons::PipelineMultisampleState(),
			RenderingDeviceCommons::PipelineDepthStencilState(), blend_state,
			0);

		create_triangle();
		rendering_device->_submit_transfer_workers();


	}

	RID WSI::get_current_pipeline()
	{
		return pipeline;
	}

	RenderingShaderContainerFormat* WSI::create_shader_container_format() 
	{
		return new ::Vulkan::RenderingShaderContainerFormatVulkan();
	}
 
	void WSI::create_triangle()
	{
		static const uint32_t triangle_vertex_count = 3;
		static const float triangle_vertices[triangle_vertex_count * 3] = {
			0.0f,  1.0f, 0.0f,   // Vertex 0
		   -1.0f, -1.0f, 0.0f,   // Vertex 1
			1.0f, -1.0f, 0.0f    // Vertex 2
		};

		static const uint32_t triangle_triangle_count = 1;
		static const uint16_t triangle_triangle_indices[triangle_triangle_count * 3] = {
			0, 1, 2
		};

		std::vector<uint8_t> vertex_data;
		vertex_data.resize(sizeof(float) * triangle_vertex_count * 3);
		memcpy(vertex_data.data(), triangle_vertices, vertex_data.size());

		triangle_vertex_buffer = rendering_device->vertex_buffer_create(vertex_data.size(), vertex_data);

		std::vector<uint8_t> index_data;
		index_data.resize(sizeof(uint16_t) * triangle_triangle_count * 3);
		memcpy(index_data.data(), triangle_triangle_indices, index_data.size());

		triangle_index_buffer = rendering_device->index_buffer_create(triangle_triangle_count * 3, RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16, index_data);

		std::vector<RID> buffers;
		buffers.push_back(triangle_vertex_buffer);

		triangle_vertex_array = rendering_device->vertex_array_create(triangle_vertex_count, vertex_format, buffers);

		triangle_index_array = rendering_device->index_array_create(triangle_index_buffer, 0, triangle_triangle_count * 3);
		
	}

	void WSI::bind()
	{
		rendering_device->bind_vertex_array(triangle_vertex_array);
		rendering_device->bind_index_array(triangle_index_array);
	}

	void WSI::teardown()
	{
	}

	WSI::~WSI()
	{

	}

	void WSI::set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data)
	{
		windows.insert({ window, data });
	}

	Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
	{
		// TODO: check if window entry exsists

		auto wd = windows[p_window_id];

		Error err = rendering_context->window_create(p_window_id, &wd.platfform_data);
		ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create {} window.", p_rendering_driver));
		rendering_context->window_set_size(p_window_id, wd.window_resolution.x, wd.window_resolution.y);
		surface = rendering_context->surface_get_from_window(p_window_id);
		return OK;
	}

}
