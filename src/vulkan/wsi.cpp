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
		auto swapchainId = device_ptr->swap_chain_create(surface);

		BitField<Device::CommandQueueFamilyBits> main_queue_bits = {};
		main_queue_bits.set_flag(Device::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);

		auto main_queue_family = device_ptr->command_queue_family_get(main_queue_bits, surface);
		ERR_FAIL_COND_V(!main_queue_family, FAILED);

		auto main_queue = device_ptr->command_queue_create(main_queue_family, true);
		ERR_FAIL_COND_V(!main_queue, FAILED);

		device_ptr->swap_chain_resize(main_queue, swapchainId, frame_count);


		return true;
	}

	bool WSI::begin_frame()
	{
		VkResult result = VK_SUCCESS;
		do
		{

			// swap_chain_acquire_framebuffer
			if (result >= 0)
			{
				platform->poll_input();
			}
		} while (result < 0);
		return true;
	}

	bool WSI::end_frame()
	{
		return true;
	}
}
