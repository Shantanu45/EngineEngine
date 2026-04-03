/*****************************************************************//**
 * \file   rendering_context_driver.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "rendering_context_driver.h"

namespace Rendering
{
	RenderingContextDriver::~RenderingContextDriver() {
	}

	RenderingContextDriver::SurfaceID RenderingContextDriver::surface_get_from_window(DisplayServerEnums::WindowID p_window) const {
		std::unordered_map<DisplayServerEnums::WindowID, SurfaceID>::const_iterator it = window_surface_map.find(p_window);
		if (it != window_surface_map.end()) {
			return it->second;
		}
		else {
			return SurfaceID();
		}
	}

	Error RenderingContextDriver::window_create(DisplayServerEnums::WindowID p_window, const void* p_platform_data) {
		SurfaceID surface = surface_create(p_platform_data);
		if (surface != 0) {
			window_surface_map[p_window] = surface;
			return OK;
		}
		else {
			return ERR_CANT_CREATE;
		}
	}

	void RenderingContextDriver::window_set_size(DisplayServerEnums::WindowID p_window, uint32_t p_width, uint32_t p_height) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_size(surface, p_width, p_height);
		}
	}

	void RenderingContextDriver::window_set_vsync_mode(DisplayServerEnums::WindowID p_window, DisplayServerEnums::VSyncMode p_vsync_mode) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_vsync_mode(surface, p_vsync_mode);
		}
	}

	DisplayServerEnums::VSyncMode RenderingContextDriver::window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			return surface_get_vsync_mode(surface);
		}
		else {
			return DisplayServerEnums::VSYNC_DISABLED;
		}
	}

	void RenderingContextDriver::window_set_hdr_output_enabled(DisplayServerEnums::WindowID p_window, bool p_enabled) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_hdr_output_enabled(surface, p_enabled);
		}
	}

	bool RenderingContextDriver::window_get_hdr_output_enabled(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			return surface_get_hdr_output_enabled(surface);
		}
		else {
			return false;
		}
	}

	void RenderingContextDriver::window_set_hdr_output_reference_luminance(DisplayServerEnums::WindowID p_window, float p_reference_luminance) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_hdr_output_reference_luminance(surface, p_reference_luminance);
		}
	}

	float RenderingContextDriver::window_get_hdr_output_reference_luminance(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			return surface_get_hdr_output_reference_luminance(surface);
		}
		else {
			return 0.0f;
		}
	}

	void RenderingContextDriver::window_set_hdr_output_max_luminance(DisplayServerEnums::WindowID p_window, float p_max_luminance) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_hdr_output_max_luminance(surface, p_max_luminance);
		}
	}

	float RenderingContextDriver::window_get_hdr_output_max_luminance(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			return surface_get_hdr_output_max_luminance(surface);
		}
		else {
			return 0.0f;
		}
	}

	void RenderingContextDriver::window_set_hdr_output_linear_luminance_scale(DisplayServerEnums::WindowID p_window, float p_linear_luminance_scale) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_set_hdr_output_linear_luminance_scale(surface, p_linear_luminance_scale);
		}
	}

	float RenderingContextDriver::window_get_hdr_output_linear_luminance_scale(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			return surface_get_hdr_output_linear_luminance_scale(surface);
		}
		else {
			return 0.0f;
		}
	}

	float RenderingContextDriver::window_get_output_max_linear_value(DisplayServerEnums::WindowID p_window) const {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			if (surface_get_hdr_output_enabled(surface)) {
				return surface_get_hdr_output_max_value(surface);
			}
		}
		return 1.0f; // SDR
	}

	void RenderingContextDriver::window_destroy(DisplayServerEnums::WindowID p_window) {
		SurfaceID surface = surface_get_from_window(p_window);
		if (surface) {
			surface_destroy(surface);
		}

		window_surface_map.erase(p_window);
	}

	std::string RenderingContextDriver::get_driver_and_device_memory_report() const {
		std::string report;

		const uint32_t num_tracked_obj_types = static_cast<uint32_t>(get_tracked_object_type_count());

		report += "=== Driver Memory Report ===";

		report += "\nLaunch with --extra-gpu-memory-tracking and build with "
			"DEBUG_ENABLED for this functionality to work.";
		report += "\nDevice memory may be unavailable if the API does not support it"
			"(e.g. VK_EXT_device_memory_report is unsupported).";
		report += "\n";

		report += "\nTotal Driver Memory:";
		report += std::to_string(double(get_driver_total_memory()) / (1024.0 * 1024.0));
		report += " MB";
		report += "\nTotal Driver Num Allocations: ";
		report += std::to_string(get_driver_allocation_count());

		report += "\nTotal Device Memory:";
		report += std::to_string(double(get_device_total_memory()) / (1024.0 * 1024.0));
		report += " MB";
		report += "\nTotal Device Num Allocations: ";
		report += std::to_string(get_device_allocation_count());

		report += "\n\nMemory use by object type (CSV format):";
		report += "\n\nCategory; Driver memory in MB; Driver Allocation Count; "
			"Device memory in MB; Device Allocation Count";

		for (uint32_t i = 0u; i < num_tracked_obj_types; ++i) {
			report += "\n";
			report += get_tracked_object_name(i);
			report += ";";
			report += std::to_string(double(get_driver_memory_by_object_type(i)) / (1024.0 * 1024.0));
			report += ";";
			report += std::to_string(get_driver_allocs_by_object_type(i));
			report += ";";
			report += std::to_string(double(get_device_memory_by_object_type(i)) / (1024.0 * 1024.0));
			report += ";";
			report += std::to_string(get_device_allocs_by_object_type(i));
		}

		return report;
	}

	const char* RenderingContextDriver::get_tracked_object_name(uint32_t p_type_index) const {
		return "Tracking Unsupported by API";
	}

	uint64_t RenderingContextDriver::get_tracked_object_type_count() const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_driver_total_memory() const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_driver_allocation_count() const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_driver_memory_by_object_type(uint32_t) const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_driver_allocs_by_object_type(uint32_t) const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_device_total_memory() const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_device_allocation_count() const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_device_memory_by_object_type(uint32_t) const {
		return 0;
	}

	uint64_t RenderingContextDriver::get_device_allocs_by_object_type(uint32_t) const {
		return 0;
	}

}
