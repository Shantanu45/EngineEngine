#pragma once
#include <vector>
#include "vulkan_common.h"
#include "vulkan_context.h"
#include "vulkan_device.h"

namespace Vulkan
{
	class WSI;

	class WSIPlatform
	{
	public:
		virtual ~WSIPlatform() = default;
		virtual VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice gpu) = 0;

		virtual std::vector<const char*> get_instance_extensions() = 0;
		virtual std::vector<const char*> get_device_extensions()
		{
			return { "VK_KHR_swapchain" };
		}

		virtual uint32_t get_surface_width() = 0;
		virtual uint32_t get_surface_height() = 0;
		virtual bool alive(WSI& wsi) = 0;
		virtual void poll_input() = 0;

	protected:
		unsigned current_swapchain_width = 0;
		unsigned current_swapchain_height = 0;

	};

	class WSI
	{
	public:
		WSI() {};
		void set_platform(WSIPlatform* platform);

		bool init_context();
		bool init_device();
		bool begin_frame();
		bool end_frame();

	private:
		WSIPlatform* platform = nullptr;

		Context context;
		std::unique_ptr<Device> device_ptr = nullptr;

		Context::SurfaceID surface;

		uint32_t frame_count = 0;

	};
}
