#pragma once

#include "vulkan_device.h"
#include "imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_vulkan.h"

namespace Vulkan
{
	class ImGuiDevice
	{
	public:
		ImGuiDevice(SDL_Window* p_window, RenderingContextDriverVulkan* p_vulkan_context, RenderingDeviceDriverVulkan* p_vulkan_driver);
		virtual ~ImGuiDevice();

		Error initialize(const uint32_t p_device_index, const uint32_t p_surface_id, const VkQueue p_queue,
			const VkPipelineCache p_pipeline_cache, const VkDescriptorPool p_descriptor_pool,
			const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const VkRenderPass p_render_pass,
			const uint32_t subpass);

		void finalize();


	public:
		RenderingContextDriverVulkan* vulkan_context;
		RenderingDeviceDriverVulkan* vulkan_driver;
		SDL_Window* window;
	};
}
