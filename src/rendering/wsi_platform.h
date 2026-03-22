/*****************************************************************//**
 * \file   wsi_platform.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include <cstdint>
#include <vector>
#include "rendering_context_driver.h"

class WSI;

namespace Rendering
{
	class WSIPlatform
	{
	public:
		virtual ~WSIPlatform() = default;

		virtual std::vector<const char*> get_instance_extensions() = 0;
		virtual std::vector<const char*> get_device_extensions()
		{
			return { "VK_KHR_swapchain" };
		}

		virtual uint32_t get_surface_width() = 0;
		virtual uint32_t get_surface_height() = 0;
		virtual bool alive(/*WSI& wsi*/) = 0;
		virtual void poll_input() = 0;

		virtual WindowPlatformData get_window_platform_data(DisplayServerEnums::WindowID p_window_id) = 0;

		virtual void release_resources()
		{
		}

	protected:
		unsigned current_swapchain_width = 0;
		unsigned current_swapchain_height = 0;

	};
}
