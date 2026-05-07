/*****************************************************************//**
 * \file   rendering_context_driver.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include "util/error_macros.h"
#include "util/small_vector.h"

namespace DisplayServerEnums
{
	typedef int WindowID;

	// Keep the VSyncMode enum values in sync with the `display/window/vsync/vsync_mode`
	// project setting hint.
	enum VSyncMode {
		VSYNC_DISABLED,
		VSYNC_ENABLED,
		VSYNC_ADAPTIVE,
		VSYNC_MAILBOX
	};

	enum {
		MAIN_WINDOW_ID = 0,
		INVALID_WINDOW_ID = -1,
		INVALID_INDICATOR_ID = -1
	};


	enum {
		INVALID_SCREEN = -1,
		SCREEN_WITH_MOUSE_FOCUS = -4,
		SCREEN_WITH_KEYBOARD_FOCUS = -3,
		SCREEN_PRIMARY = -2,
		SCREEN_OF_MAIN_WINDOW = -1, // Note: for the main window, determine screen from position.
	};

	enum WindowMode {
		WINDOW_MODE_WINDOWED,
		WINDOW_MODE_MINIMIZED,
		WINDOW_MODE_MAXIMIZED,
		WINDOW_MODE_FULLSCREEN,
		WINDOW_MODE_EXCLUSIVE_FULLSCREEN,
	};

	enum Context {
		CONTEXT_EDITOR,
		CONTEXT_ENGINE,
	};

}

struct WindowPlatformData {
	enum class Platform { SDL3, Win32};
	Platform platform;

	union {
		struct { void* window; void* instance; } sdl;   // SDL_Window*, hinstance for win32 fallback
		struct { void* hwnd; void* hinstance; } win32;
	};
};

namespace Rendering
{
	class RenderingDeviceDriver;
	class RenderingContextDriver
	{
	public:

		typedef uint64_t SurfaceID;
	private:
		std::unordered_map<DisplayServerEnums::WindowID, SurfaceID> window_surface_map;

	public:
		SurfaceID surface_get_from_window(DisplayServerEnums::WindowID p_window) const;
		Error window_create(DisplayServerEnums::WindowID p_window, const void* p_platform_data);
		void window_set_size(DisplayServerEnums::WindowID p_window, uint32_t p_width, uint32_t p_height);
		void window_set_vsync_mode(DisplayServerEnums::WindowID p_window, DisplayServerEnums::VSyncMode p_vsync_mode);
		DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const;
		void window_set_hdr_output_enabled(DisplayServerEnums::WindowID p_window, bool p_enabled);
		bool window_get_hdr_output_enabled(DisplayServerEnums::WindowID p_window) const;
		void window_set_hdr_output_reference_luminance(DisplayServerEnums::WindowID p_window, float p_reference_luminance);
		float window_get_hdr_output_reference_luminance(DisplayServerEnums::WindowID p_window) const;
		void window_set_hdr_output_max_luminance(DisplayServerEnums::WindowID p_window, float p_max_luminance);
		float window_get_hdr_output_max_luminance(DisplayServerEnums::WindowID p_window) const;
		void window_set_hdr_output_linear_luminance_scale(DisplayServerEnums::WindowID p_window, float p_linear_luminance_scale);
		float window_get_hdr_output_linear_luminance_scale(DisplayServerEnums::WindowID p_window) const;
		float window_get_output_max_linear_value(DisplayServerEnums::WindowID p_window) const;
		void window_destroy(DisplayServerEnums::WindowID p_window);

	public:
		// Not an enum as these values are matched against values returned by
		// the various drivers, which report them in uint32_t. Casting to an
		// enum value is dangerous in this case as we don't actually know what
		// range the driver is reporting a value in.
		struct Vendor {
			constexpr static uint32_t VENDOR_UNKNOWN = 0x0;
			constexpr static uint32_t VENDOR_AMD = 0x1002;
			constexpr static uint32_t VENDOR_IMGTEC = 0x1010;
			constexpr static uint32_t VENDOR_APPLE = 0x106B;
			constexpr static uint32_t VENDOR_NVIDIA = 0x10DE;
			constexpr static uint32_t VENDOR_ARM = 0x13B5;
			constexpr static uint32_t VENDOR_MICROSOFT = 0x1414;
			constexpr static uint32_t VENDOR_QUALCOMM = 0x5143;
			constexpr static uint32_t VENDOR_INTEL = 0x8086;
		};

		enum DeviceType {
			DEVICE_TYPE_OTHER = 0x0,
			DEVICE_TYPE_INTEGRATED_GPU = 0x1,
			DEVICE_TYPE_DISCRETE_GPU = 0x2,
			DEVICE_TYPE_VIRTUAL_GPU = 0x3,
			DEVICE_TYPE_CPU = 0x4,
			DEVICE_TYPE_MAX = 0x5
		};

		struct Workarounds {
			bool avoid_compute_after_draw = false;
		};

		struct Device {
			std::string name = "Unknown";
			uint32_t vendor = Vendor::VENDOR_UNKNOWN;
			DeviceType type = DEVICE_TYPE_OTHER;
			Workarounds workarounds;
		};

		virtual ~RenderingContextDriver();
		virtual Error initialize() = 0;
		virtual const Device& device_get(uint32_t p_device_index) const = 0;
		virtual uint32_t device_get_count() const = 0;
		virtual bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const = 0;
		virtual RenderingDeviceDriver* driver_create() = 0;
		virtual void driver_free(RenderingDeviceDriver* p_driver) = 0;
		virtual SurfaceID surface_create(const void* p_platform_data) = 0;
		virtual void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) = 0;
		virtual void surface_set_vsync_mode(SurfaceID p_surface, DisplayServerEnums::VSyncMode p_vsync_mode) = 0;
		virtual DisplayServerEnums::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const = 0;
		virtual void surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) = 0;
		virtual bool surface_get_hdr_output_enabled(SurfaceID p_surface) const = 0;
		virtual void surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) = 0;
		virtual float surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const = 0;
		virtual void surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) = 0;
		virtual float surface_get_hdr_output_max_luminance(SurfaceID p_surface) const = 0;
		virtual void surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) = 0;
		virtual float surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const = 0;
		virtual float surface_get_hdr_output_max_value(SurfaceID p_surface) const = 0;
		virtual uint32_t surface_get_width(SurfaceID p_surface) const = 0;
		virtual uint32_t surface_get_height(SurfaceID p_surface) const = 0;
		virtual void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) = 0;
		virtual bool surface_get_needs_resize(SurfaceID p_surface) const = 0;
		virtual void surface_destroy(SurfaceID p_surface) = 0;
		virtual bool is_debug_utils_enabled() const = 0;
		virtual void set_platform_surface_extension(Util::SmallVector<const char*> ext) = 0;
		virtual bool init_loader_and_extensions(WindowPlatformData::Platform p_platform, bool force_reload = false) = 0;

		std::string get_driver_and_device_memory_report() const;

		virtual const char* get_tracked_object_name(uint32_t p_type_index) const;
		virtual uint64_t get_tracked_object_type_count() const;

		virtual uint64_t get_driver_total_memory() const;
		virtual uint64_t get_driver_allocation_count() const;
		virtual uint64_t get_driver_memory_by_object_type(uint32_t p_type) const;
		virtual uint64_t get_driver_allocs_by_object_type(uint32_t p_type) const;

		virtual uint64_t get_device_total_memory() const;
		virtual uint64_t get_device_allocation_count() const;
		virtual uint64_t get_device_memory_by_object_type(uint32_t p_type) const;
		virtual uint64_t get_device_allocs_by_object_type(uint32_t p_type) const;
	};
}
