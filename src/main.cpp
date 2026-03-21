#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vma/vk_mem_alloc.h"
#include "application/application.h"
#include "application/application_entry/application_entry.h"

struct TriangleApplication : EE::Application
{
	void render_frame(double frame_time, double elapsed_time) override
	{
		auto wsi = get_wsi();
		auto device = wsi->get_rendering_device();
		device->begin_for_screen(DisplayServerEnums::MAIN_WINDOW_ID);
		auto cmd_buffer = device->get_current_command_buffer();
		device->bind_render_pipeline(cmd_buffer, wsi->get_current_pipeline());

		device->render_draw(cmd_buffer, 3, 1);

		{
			
		}
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