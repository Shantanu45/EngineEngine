#include "wsi.h"
#include "libassert/assert.hpp"

namespace Vulkan
{
	void WSI::set_platform(WSIPlatform* wsi_platform)
	{
		platform = wsi_platform;
	}

	bool WSI::init_context()
	{
		frame_count = 2;
		context.set_platform_surface_extension(platform->get_instance_extensions());
		context.initialize();
		auto surface_khr = platform->create_surface(context.instance_get(), context.physical_device_get(0));
		DEBUG_ASSERT(surface_khr != VK_NULL_HANDLE);
		surface = context.set_surface(surface_khr);
		return true;
	}

	bool WSI::init_device()
	{
		device_ptr = std::make_unique<Device>(&context);
		device_ptr->initialize(0, 2);			//TODO: figure out the parameters
		swapchain = device_ptr->swap_chain_create(surface);

		BitField<Device::CommandQueueFamilyBits> main_queue_bits = {};
		main_queue_bits.set_flag(Device::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);

		auto main_queue_family = device_ptr->command_queue_family_get(main_queue_bits, surface);
		ERR_FAIL_COND_V(!main_queue_family, FAILED);

		main_queue = device_ptr->command_queue_create(main_queue_family, true);
		ERR_FAIL_COND_V(!main_queue, FAILED);

		frames.resize(frame_count);

		// Create data for all the frames.
		bool frame_failed = false;
		for (uint32_t i = 0; i < frames.size(); i++) {
			frames[i].index = 0;
			frames[i].command_pool = device_ptr->command_pool_create(main_queue_family, Device::COMMAND_BUFFER_TYPE_PRIMARY);
			if (!frames[i].command_pool) {
				frame_failed = true;
				break;
			}
			frames[i].command_buffer = device_ptr->command_buffer_create(frames[i].command_pool);
			if (!frames[i].command_buffer) {
				frame_failed = true;
				break;
			}
			frames[i].semaphore = device_ptr->semaphore_create();
			if (!frames[i].semaphore) {
				frame_failed = true;
				break;
			}
			frames[i].fence = device_ptr->fence_create();
			if (!frames[i].fence) {
				frame_failed = true;
				break;
			}
			frames[i].fence_signaled = false;
		}
		if (frame_failed) {
			// Clean up created data.
			for (uint32_t i = 0; i < frames.size(); i++) {
				if (frames[i].command_pool) {
					device_ptr->command_pool_free(frames[i].command_pool);
				}
				if (frames[i].semaphore) {
					device_ptr->semaphore_free(frames[i].semaphore);
				}
				if (frames[i].fence) {
					device_ptr->fence_free(frames[i].fence);
				}
				//if (frames[i].timestamp_pool) {
				//	device_ptr->timestamp_query_pool_free(frames[i].timestamp_pool);
				//}
				//for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
				//	if (frames[i].transfer_worker_semaphores[j]) {
				//		device_ptr->semaphore_free(frames[i].transfer_worker_semaphores[j]);
				//	}
				//}
			}
			frames.clear();
			ERR_FAIL_V_MSG(FAILED, "Failed to create frame data.");
		}


		device_ptr->swap_chain_resize(main_queue, swapchain, frame_count);

		return true;
	}

	bool WSI::begin_frame()
	{
		VkResult result = VK_SUCCESS;
		do
		{
			bool resize_required;
			Device::FramebufferID framebuffer = device_ptr->swap_chain_acquire_framebuffer(main_queue, swapchain, resize_required);
			auto command_buffer = frames[curr_frame].command_buffer;
			auto render_pass = device_ptr->swap_chain_get_render_pass(swapchain);
			device_ptr->command_buffer_begin(command_buffer);
			Device::RenderPassClearValue val;
			val.color = Color{ 1.0 , 0.0 , 0.0 , 0.0 };
			device_ptr->command_begin_render_pass(command_buffer, render_pass, framebuffer, Device::COMMAND_BUFFER_TYPE_PRIMARY, Rect2i{ 0, 0, (int)platform->get_surface_width(), (int)platform->get_surface_height() }, { val });
			device_ptr->command_end_render_pass(command_buffer);
			device_ptr->command_buffer_end(command_buffer);

			device_ptr->command_queue_execute_and_present(main_queue, {}, { &command_buffer, 1 }, {}, frames[curr_frame].fence, { &swapchain, 1 });

			if (result >= 0)
			{
				platform->poll_input();
			}
		} while (result < 0);

		device_ptr->fence_wait(frames[curr_frame].fence);

		curr_frame = (curr_frame + 1) % 2;

		return true;
	}

	bool WSI::end_frame()
	{
		return true;
	}
}
