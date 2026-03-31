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
		ImGuiDevice(WindowPlatformData p_platform_data, RenderingContextDriver* p_vulkan_context, RenderingDeviceDriver* p_vulkan_driver);
		virtual ~ImGuiDevice();

		Error initialize(const uint32_t p_device_index, const uint32_t p_queue_family,
			const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const RenderingDeviceCommons::DataFormat p_swapchain_format,
			std::span<RenderingDeviceDriver::TextureID> p_attachments, uint32_t width, uint32_t height);

		void poll_event(SDL_Event* event);

		void begin_frame();

		void render();

		void end_frame();

		RenderingDeviceDriver::FramebufferID get_imgui_framebuffer() {
			return framebuffer;
		}

		void show_demo_window();
		void execute(void* p_draw_data, RenderingDeviceDriverVulkan::CommandBufferID p_command_buffer, RenderingDeviceDriverVulkan::PipelineID p_pipeline);
		void finalize();

	private:
		VkRenderPass _create_render_pass(VkDevice device, VkFormat swapchainFormat);
		RenderingDeviceDriver::FramebufferID _create_imgui_framebuffers(VkRenderPass p_render_pass, std::span<RenderingDeviceDriver::TextureID> p_attachments, uint32_t p_width, uint32_t p_height);
	public:
		RenderingContextDriverVulkan* vulkan_context;
		RenderingDeviceDriverVulkan* vulkan_driver;
		WindowPlatformData platform_data;
		VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

		RenderingDeviceDriver::FramebufferID framebuffer;

	};
}
