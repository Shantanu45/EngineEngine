#include "wsi.h"

namespace Vulkan
{
	void WSI::set_platform(WSIPlatform* wsi_platform)
	{
		platform = wsi_platform;
	}

	bool WSI::init_context()
	{
		context.set_platform_surface_extension(platform->get_instance_extensions()[0]);
		context.initialize();
		return true;
	}

	bool WSI::init_device()
	{
		device_ptr = std::make_unique<Device>(&context);
		device_ptr->initialize(0, 2);			//TODO: figure out the parameters
		return true;
	}
}
