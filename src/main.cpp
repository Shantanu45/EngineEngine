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
#include "libassert/assert.hpp"
#include <cmath> 
#include <cstddef>

struct TriangleApplication : EE::Application
{
	struct alignas(16) UBO {
		float x, y, z;
		float _pad;  // pad to 16 bytes
	};

	void pre_frame() override
	{
		auto wsi = get_wsi();

		wsi->set_default_vertex_attribute();

		wsi->load_gltf("assets://gltf/two_cubes.glb");

		auto device = wsi->get_rendering_device();

		state_uniform = device->uniform_buffer_create(sizeof(UBO));

		std::vector<Rendering::RenderingDevice::Uniform> uniforms;
		Rendering::RenderingDevice::Uniform u;
		u.uniform_type = Rendering::RenderingDeviceCommons::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(state_uniform);
		uniforms.push_back(u);

		DEV_ASSERT(rendering_device != nullptr);

		device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);

		wsi->set_program({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" });

		uniform_set = device->uniform_set_create(uniforms, wsi->get_bound_shader(), 0);
	
		wsi->pipeline_create_default();
	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		auto wsi = get_wsi();
		auto device = wsi->get_rendering_device();

		double intpart;
		double fracpart = std::modf(elapsed_time, &intpart);
		UBO state;
		state.x = 1.0;
		state.y = 0.0;
		state.z = fracpart;
		auto err = device->buffer_update(state_uniform, 0, sizeof(UBO), &state);

		device->begin_for_screen(DisplayServerEnums::MAIN_WINDOW_ID);
		auto cmd_buffer = device->get_current_command_buffer();

		device->bind_render_pipeline(cmd_buffer, wsi->get_current_pipeline());
		device->bind_uniform_set(wsi->get_bound_shader(), uniform_set, 0);
		wsi->bind_and_draw_indexed(cmd_buffer);

	}

private:
	RID state_uniform;
	RID uniform_set;
	Rendering::MeshPrimitive prim;
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