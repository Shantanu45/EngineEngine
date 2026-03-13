#pragma once
#include <vector>
#include "vulkan_common.h"
#include "vulkan_context.h"
#include "vulkan_device.h"

namespace Vulkan
{
	class WSI;

	struct Frame {
		// The command pool used by the command buffer.
		Device::CommandPoolID command_pool;

		// The command buffer used by the main thread when recording the frame.
		Device::CommandBufferID command_buffer;

		// Signaled by the command buffer submission. Present must wait on this semaphore.
		Device::SemaphoreID semaphore;

		// Signaled by the command buffer submission. Must wait on this fence before beginning command recording for the frame.
		Device::FenceID fence;
		bool fence_signaled = false;

		// Semaphores the frame must wait on before executing the command buffer.
		std::vector<Device::SemaphoreID> semaphores_to_wait_on;
		//  Swap chains prepared for drawing during the frame that must be presented.
		std::vector<Device::SwapChainID> swap_chains_to_present;

		// Semaphores the transfer workers can use to wait before rendering the frame.
		// This must have the same size of the transfer worker pool.
		std::vector<Device::SemaphoreID> transfer_worker_semaphores;

		// Extra command buffer pool used for driver workarounds or to reduce GPU bubbles by
		// splitting the final render pass to the swapchain into its own cmd buffer.
		//Device::CommandBufferPool command_buffer_pool;

		uint64_t index = 0;
	};

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
		Device::SwapChainID swapchain;

		uint32_t frame_count = 0;

		std::vector<Frame> frames;
		uint32_t curr_frame = 0;
		Device::CommandQueueID main_queue;


	};
}
