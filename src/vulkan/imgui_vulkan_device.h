#pragma once
#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "vulkan_device.h"
#include "imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_vulkan.h"

namespace Vulkan
{
	class ImGuiDevice
	{
	public:
		ImGuiDevice(RenderingContextDriver* p_vulkan_context, RenderingDeviceDriver* p_vulkan_driver);
		virtual ~ImGuiDevice();

		Error initialize(const uint32_t p_device_index, const uint32_t p_surface_id,
			const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const RenderingDeviceDriver::RenderPassID p_render_pass,
			const uint32_t subpass);

		void poll_event(SDL_Event* event);

		void begin_frame();

		void render();

		void end_frame();

		void show_demo_window();
		void execute(void* p_draw_data, RenderingDeviceDriverVulkan::CommandBufferID p_command_buffer, RenderingDeviceDriverVulkan::PipelineID p_pipeline);
		void finalize();
	public:
		RenderingContextDriverVulkan* vulkan_context;
		RenderingDeviceDriverVulkan* vulkan_driver;
		//WindowPlatformData* platform_data;
		VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

	};
}
