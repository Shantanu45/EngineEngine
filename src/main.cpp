/*****************************************************************//**
 * \file   main.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vma/vk_mem_alloc.h"
#include "application/application.h"
#include "application/application_entry/application_entry.h"



struct TriangleApplication : EE::Application
{
	void pre_frame() override
	{

		static const uint32_t triangle_vertex_count = 3;

		static const float triangle_vertices[3 * 3] = {
			// Vertex 0
			0.0f,  1.0f, 0.0f,

			// Vertex 1			
		   -1.0f, -1.0f, 0.0f,

		   // Vertex 2			
		   1.0f, -1.0f, 0.0f,
		};

		static const float triangle_vertices_color[3 * 3] = {
			1.0f, 0.0f, 0.0f,  // red

			//
			0.0f, 1.0f, 0.0f,  // green

			//
			0.0f, 0.0f, 1.0f   // blue
		};

		static const float triangle_vertices_color2[3 * 3] = {
			1.0f, 0.0f, 0.0f,  // red

			//
			1.0f, 0.0f, 0.0f,  // green

			//
			1.0f, 0.0f, 0.0f   // blue
		};

		auto wsi = get_wsi();


		wsi->push_vertex_data((void*)triangle_vertices, sizeof(triangle_vertices));
		wsi->push_vertex_data((void*)triangle_vertices_color, sizeof(triangle_vertices_color));
		wsi->push_vertex_data((void*)triangle_vertices_color2, sizeof(triangle_vertices_color2));
		wsi->set_vertex_attribute(0, 0, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, 0, sizeof(float) * 9);
		wsi->set_vertex_attribute(0, 1, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3, sizeof(float) * 9);
		wsi->set_vertex_attribute(0, 2, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6, sizeof(float) * 9);

		const uint32_t triangle_triangle_count = 1;
		const uint16_t triangle_triangle_indices[triangle_triangle_count * 3] = {
			0, 1, 2
		};

		wsi->push_index_data((void*)triangle_triangle_indices, sizeof(triangle_triangle_indices), Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16);

		wsi->pre_frame_loop();
	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		auto wsi = get_wsi();
		auto device = wsi->get_rendering_device();
		device->begin_for_screen(DisplayServerEnums::MAIN_WINDOW_ID);
		auto cmd_buffer = device->get_current_command_buffer();

		device->bind_render_pipeline(cmd_buffer, wsi->get_current_pipeline());
		wsi->bind_vbo_and_ibo();

		device->render_draw(cmd_buffer, 3, 1);

	}
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;
		spdlog::info("hwlloe");
		try
		{
			auto* app = new TriangleApplication();
			return app;
		}
		catch (const std::exception& e)
		{
			//LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}