#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "application/application.h"
#include "application/application_entry/application_entry.h"

struct TriangleApplication : EE::Application
{
	void render_frame(double frame_time, double elapsed_time) override
	{

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