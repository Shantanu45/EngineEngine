/*****************************************************************//**
 * \file   vulkan_context.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include "vulkan_common.h"
#include "util/error_list.h"
#include <set>
#include <unordered_map>
#include <string>
#include "rendering/rendering_context_driver.h"
#define VULKAN_DEBUG


namespace Vulkan
{
	using namespace ::Rendering;
	class RenderingContextDriverVulkan : public RenderingContextDriver
	{
	public:
		typedef uint64_t SurfaceID;

		RenderingContextDriverVulkan();
		virtual ~RenderingContextDriverVulkan();

		virtual Error initialize() override;
		virtual SurfaceID surface_create(const void* p_platform_data) override;
		virtual const Device& device_get(uint32_t p_device_index) const override;
		virtual uint32_t device_get_count() const override;
		virtual bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const override;
		virtual void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) override;
		virtual void surface_set_vsync_mode(SurfaceID p_surface, DisplayServerEnums::VSyncMode p_vsync_mode) override;
		virtual DisplayServerEnums::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const override;
		virtual void surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) override;
		virtual bool surface_get_hdr_output_enabled(SurfaceID p_surface) const override;
		virtual void surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) override;
		virtual float surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const override;
		virtual void surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) override;
		virtual float surface_get_hdr_output_max_luminance(SurfaceID p_surface) const override;
		virtual void surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) override;
		virtual float surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const;
		virtual float surface_get_hdr_output_max_value(SurfaceID p_surface) const override;
		virtual uint32_t surface_get_width(SurfaceID p_surface) const override;
		virtual uint32_t surface_get_height(SurfaceID p_surface) const override;
		virtual void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) override;
		virtual bool surface_get_needs_resize(SurfaceID p_surface) const override;
		virtual void surface_destroy(SurfaceID p_surface) override;
		virtual bool is_debug_utils_enabled() const override;
		virtual RenderingDeviceDriver* driver_create() override;
		virtual void set_platform_surface_extension(std::vector<const char*> ext) override { surface_extension = ext; }

		virtual void driver_free(RenderingDeviceDriver* p_driver) override;


		RenderingContextDriverVulkan::SurfaceID set_surface(VkSurfaceKHR vk_surface);

		bool init_loader_and_extensions(WindowPlatformData::Platform p_platform, bool force_reload = false) override;

		VkApplicationInfo get_promoted_application_info() const;

		struct Surface {
			VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
			uint32_t width = 0;
			uint32_t height = 0;
			DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
			bool needs_resize = false;

			bool hdr_output = false;
			// BT.2408 recommendation of 203 nits for HDR Reference White, rounded to 200
			// to be a more pleasant player-facing value.
			float hdr_reference_luminance = 200.0f;
			float hdr_max_luminance = 1000.0f;
			float hdr_linear_luminance_scale = 100.0f;
		};

		bool queue_family_supports_present(VkPhysicalDevice p_physical_device, uint32_t p_queue_family_index, SurfaceID p_surface) const;




		bool is_colorspace_supported() const;

		VkInstance instance_get() const;
		VkPhysicalDevice physical_device_get(uint32_t p_device_index) const;
		uint32_t queue_family_get_count(uint32_t p_device_index) const;
		VkQueueFamilyProperties queue_family_get(uint32_t p_device_index, uint32_t p_queue_family_index) const;

	protected:
		virtual Error _create_vulkan_instance(const VkInstanceCreateInfo* p_create_info, VkInstance* r_instance);
		virtual bool _use_validation_layers() const;
		virtual std::vector<const char*> _get_platform_surface_extension() const { return surface_extension; }
	private:
		Error _initialize_instance_extensions();
		void _register_requested_instance_extension(const std::string& p_extension_name, bool p_required);
		Error _initialize_instance();
		Error _initialize_devices();
		Error _initialize_vulkan_version();
		Error _find_validation_layers(std::vector<const char*>& r_layer_names) const;

		static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT p_message_severity, VkDebugUtilsMessageTypeFlagsEXT p_message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data);
		static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_report_callback(VkDebugReportFlagsEXT p_flags, VkDebugReportObjectTypeEXT p_object_type, uint64_t p_object, size_t p_location, int32_t p_message_code, const char* p_layer_prefix, const char* p_message, void* p_user_data);

	private:
		std::vector<const char*> surface_extension;
		struct DeviceQueueFamilies {
			std::vector<VkQueueFamilyProperties> properties;
		};

		std::unordered_map<std::string, bool> requested_instance_extensions;
		std::set<std::string> enabled_instance_extension_names;
		VkInstance instance = VK_NULL_HANDLE;

		uint32_t instance_api_version = VK_API_VERSION_1_0;

		VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
		VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;

		std::vector<Device> driver_devices;
		std::vector<VkPhysicalDevice> physical_devices;
		std::vector<DeviceQueueFamilies> device_queue_families;

	public:

	};

}
