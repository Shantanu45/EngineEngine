#include "wsi.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_device.h"
#include "libassert/assert.hpp"

namespace Rendering
{
	WSI::WSI(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error& r_error)
	{
		rendering_driver = p_rendering_driver;
		if (rendering_driver == "vulkan") {
			rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
		}
		else
		{
			// api not supported
			DEBUG_ASSERT(false);
		}

		if (rendering_context != nullptr) {
			rendering_context->set_platform_surface_extension(platform->get_instance_extensions());
			if (rendering_context->initialize() == OK) {
				DEBUG_ASSERT(platform || main_window_created);

				if (_create_rendering_context_window(DisplayServerEnums::MAIN_WINDOW_ID, rendering_driver) == OK) {
					rendering_device = RenderingDevice::get_singleton();
					if (rendering_device->initialize(rendering_context.get(), DisplayServerEnums::MAIN_WINDOW_ID) == OK) {
					}
				}
			}
		}
	}
}
