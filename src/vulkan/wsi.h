#pragma once
#include <vector>
#include "vulkan_common.h"
#include "vulkan_context.h"
#include "vulkan_device.h"
#include "rendering/wsi_platform.h"

namespace Vulkan
{
	class WSI;

	struct Frame {
		// The command pool used by the command buffer.
		RenderingDeviceDriverVulkan::CommandPoolID command_pool;

		// The command buffer used by the main thread when recording the frame.
		RenderingDeviceDriverVulkan::CommandBufferID command_buffer;

		// Signaled by the command buffer submission. Present must wait on this semaphore.
		RenderingDeviceDriverVulkan::SemaphoreID semaphore;

		// Signaled by the command buffer submission. Must wait on this fence before beginning command recording for the frame.
		RenderingDeviceDriverVulkan::FenceID fence;
		bool fence_signaled = false;

		// Semaphores the frame must wait on before executing the command buffer.
		std::vector<RenderingDeviceDriverVulkan::SemaphoreID> semaphores_to_wait_on;
		//  Swap chains prepared for drawing during the frame that must be presented.
		std::vector<RenderingDeviceDriverVulkan::SwapChainID> swap_chains_to_present;

		// Semaphores the transfer workers can use to wait before rendering the frame.
		// This must have the same size of the transfer worker pool.
		std::vector<RenderingDeviceDriverVulkan::SemaphoreID> transfer_worker_semaphores;

		// Extra command buffer pool used for driver workarounds or to reduce GPU bubbles by
		// splitting the final render pass to the swapchain into its own cmd buffer.
		//Device::CommandBufferPool command_buffer_pool;

		uint64_t index = 0;
	};

	struct WindowData {
		WindowPlatformData platfform_data;
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

		void teardown();

	private:

		Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver = "vulkan");
		void _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);

		void free_pending_resources(int p_frame);
		WSIPlatform* platform = nullptr;

		RenderingContextDriverVulkan context;
		std::unique_ptr<RenderingDeviceDriverVulkan> device_ptr = nullptr;

		RenderingContextDriverVulkan::SurfaceID surface;
		RenderingDeviceDriverVulkan::SwapChainID swapchain;

		uint32_t frame_count = 0;

		std::vector<Frame> frames;
		uint32_t curr_frame = 0;
		RenderingDeviceDriverVulkan::CommandQueueID main_queue;
		RenderingDeviceDriverVulkan::PipelineID pipeline;

		std::map<DisplayServerEnums::WindowID, WindowData> windows;

	};
}
