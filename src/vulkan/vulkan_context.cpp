#include "vulkan_context.h"
#include "vulkan/vk_enum_string_helper.h"
#include <windows.h>
#include <mutex>
#include "util/logger.h"
#include "libassert/assert.hpp"
#include "util/error_macros.h"

namespace Vulkan
{
	static std::mutex loader_init_lock;
	static bool loader_init_once;
	static PFN_vkGetInstanceProcAddr instance_proc_addr;

    bool Context::init_loader(PFN_vkGetInstanceProcAddr addr, bool force_reload)
    {
		std::lock_guard<std::mutex> holder(loader_init_lock);

		if (loader_init_once && !force_reload && !addr)
			return true;

		if (!addr)
		{
#ifndef _WIN32
			// TODO: If the user didn't pass in an address (!addr), the code manually searches the operating system for the Vulkan Loader file.
#else
			static HMODULE module;
			if (!module)
			{
				module = LoadLibraryA("vulkan-1.dll");
				if (!module)
					return false;
			}

			// Ugly pointer warning workaround.
			auto ptr = GetProcAddress(module, "vkGetInstanceProcAddr");
			static_assert(sizeof(ptr) == sizeof(addr), "Mismatch pointer type.");
			memcpy(&addr, &ptr, sizeof(ptr));

			if (!addr)
				return false;
#endif
		}
		instance_proc_addr = addr;
		volkInitializeCustom(addr);		// automatic version: volkInitialize();
		loader_init_once = true;
		return true;
    }

	VkApplicationInfo Context::get_promoted_application_info() const
	{
		VkInstanceCreateInfo* inherit_info = nullptr;		// TODO:
		VkApplicationInfo app_info = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Vukan Engine", 0, "Vukan Engine", 0, VK_API_VERSION_1_1,
		};

		VK_ASSERT(!inherit_info || inherit_info->pApplicationInfo);
		uint32_t supported_instance_version = inherit_info ? inherit_info->pApplicationInfo->apiVersion : volkGetInstanceVersion();

		// Target Vulkan 1.4 if available,
		// but the tooling ecosystem isn't quite ready for this yet, so stick to 1.3 for the time being.
		DEBUG_ASSERT(instance_api_version <= supported_instance_version);
		app_info.apiVersion = instance_api_version;

		return app_info;
	}

	bool Context::device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const
	{
		DEBUG_ASSERT(p_device_index < physical_devices.size());

		// Check if any of the queues supported by the device supports presenting to the window's surface.
		const VkPhysicalDevice physical_device = physical_devices[p_device_index];
		const DeviceQueueFamilies& queue_families = device_queue_families[p_device_index];
		for (uint32_t i = 0; i < queue_families.properties.size(); i++) {
			if ((queue_families.properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && queue_family_supports_present(physical_device, i, p_surface)) {
				return true;
			}
		}

		return false;
	}

	bool Context::queue_family_supports_present(VkPhysicalDevice p_physical_device, uint32_t p_queue_family_index, SurfaceID p_surface) const
	{
		DEBUG_ASSERT(p_physical_device != VK_NULL_HANDLE);
		DEBUG_ASSERT(p_surface != 0);
		Surface* surface = (Surface*)(p_surface);
		VkBool32 present_supported = false;
		VkResult err = vkGetPhysicalDeviceSurfaceSupportKHR(p_physical_device, p_queue_family_index, surface->vk_surface, &present_supported);
		return err == VK_SUCCESS && present_supported;
	}

	Error Context::_initialize_instance() {

		Error err;
		std::vector<const char*> enabled_extension_names;
		enabled_extension_names.reserve(enabled_instance_extension_names.size());
		for (const std::string& extension_name : enabled_instance_extension_names) {
			enabled_extension_names.push_back(extension_name.data());
		}

		VkApplicationInfo app_info = get_promoted_application_info();
		std::vector<const char*> enabled_layer_names;
		if (_use_validation_layers()) {
			err = _find_validation_layers(enabled_layer_names);
			ERR_FAIL_COND_V(err != OK, err);
		}

		VkInstanceCreateInfo instance_info = {};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

		instance_info.pApplicationInfo = &app_info;
		instance_info.enabledExtensionCount = enabled_extension_names.size();
		instance_info.ppEnabledExtensionNames = enabled_extension_names.data();
		instance_info.enabledLayerCount = enabled_layer_names.size();
		instance_info.ppEnabledLayerNames = enabled_layer_names.data();

		// This is info for a temp callback to use during CreateInstance. After the instance is created, we use the instance-based function to register the final callback.
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
		VkDebugReportCallbackCreateInfoEXT debug_report_callback_create_info = {};
		const bool has_debug_utils_extension = enabled_instance_extension_names.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		const bool has_debug_report_extension = enabled_instance_extension_names.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		if (has_debug_utils_extension) {
			debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debug_messenger_create_info.pNext = nullptr;
			debug_messenger_create_info.flags = 0;
			debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debug_messenger_create_info.pfnUserCallback = _debug_messenger_callback;
			debug_messenger_create_info.pUserData = this;
			instance_info.pNext = &debug_messenger_create_info;
		}
		else if (has_debug_report_extension) {
			debug_report_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debug_report_callback_create_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
			debug_report_callback_create_info.pfnCallback = _debug_report_callback;
			debug_report_callback_create_info.pUserData = this;
			instance_info.pNext = &debug_report_callback_create_info;
		}

		if (instance == VK_NULL_HANDLE)
		{
			if (_create_vulkan_instance(&instance_info, &instance) != OK)
				return FAILED;
		}

		volkLoadInstance(instance);

		if (has_debug_utils_extension) {
			VkResult res = vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger);

			switch (res) {
			case VK_SUCCESS:
				break;
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugUtilsMessengerEXT: out of host memory\nCreateDebugUtilsMessengerEXT Failure");
				break;
			default:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugUtilsMessengerEXT: unknown failure\nCreateDebugUtilsMessengerEXT Failure");
				break;
			}
		}
		else if (has_debug_report_extension) {
			VkResult res = vkCreateDebugReportCallbackEXT(instance, &debug_report_callback_create_info, nullptr, &debug_report);
			switch (res) {
			case VK_SUCCESS:
				break;
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugReportCallbackEXT: out of host memory\nCreateDebugReportCallbackEXT Failure");
				break;
			default:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugReportCallbackEXT: unknown failure\nCreateDebugReportCallbackEXT Failure");
				break;
			}
		}
		return OK;
	}

	Error Context::_initialize_devices() {
		uint32_t physical_device_count = 0;
		VkResult err = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
		ERR_FAIL_COND_V_MSG(physical_device_count == 0, ERR_CANT_CREATE, "vkEnumeratePhysicalDevices reported zero accessible devices.\n\nDo you have a compatible Vulkan installable client driver (ICD) installed?\nvkEnumeratePhysicalDevices Failure.");

		driver_devices.resize(physical_device_count);
		physical_devices.resize(physical_device_count);
		device_queue_families.resize(physical_device_count);
		err = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		// Fill the list of driver devices with the properties from the physical devices.
		for (uint32_t i = 0; i < physical_devices.size(); i++) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(physical_devices[i], &props);

			Device& driver_device = driver_devices[i];
			driver_device.name = std::string(props.deviceName);
			driver_device.vendor = props.vendorID;
			driver_device.type = DeviceType(props.deviceType);
			driver_device.workarounds = Workarounds();

			//TODO:
			//_check_driver_workarounds(props, driver_device);

			uint32_t queue_family_properties_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, nullptr);

			if (queue_family_properties_count > 0) {
				device_queue_families[i].properties.resize(queue_family_properties_count);
				vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, device_queue_families[i].properties.data());
			}
		}

		return OK;
	}

	Error Context::_initialize_vulkan_version() {
		// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkApplicationInfo.html#_description
		// For Vulkan 1.0 vkEnumerateInstanceVersion is not available, including not in the loader we compile against on Android.
		typedef VkResult(VKAPI_PTR* _vkEnumerateInstanceVersion)(uint32_t*);
		_vkEnumerateInstanceVersion func = (_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
		if (func != nullptr) {
			uint32_t api_version;
			VkResult res = func(&api_version);
			if (res == VK_SUCCESS) {
				instance_api_version = api_version;
			}
			else {
				// According to the documentation this shouldn't fail with anything except a memory allocation error
				// in which case we're in deep trouble anyway.
				ERR_FAIL_V(ERR_CANT_CREATE);
			}
		}
		else {
			LOGI("vkEnumerateInstanceVersion not available, assuming Vulkan 1.0.");
			instance_api_version = VK_API_VERSION_1_0;
		}

		return OK;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL Context::_debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT p_message_severity, VkDebugUtilsMessageTypeFlagsEXT p_message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data) {
		// This error needs to be ignored because the AMD allocator will mix up memory types on IGP processors.
		if (strstr(p_callback_data->pMessage, "Mapping an image with layout") != nullptr && strstr(p_callback_data->pMessage, "can result in undefined behavior if this memory is used by the device") != nullptr) {
			return VK_FALSE;
		}
		// This needs to be ignored because Validator is wrong here.
		if (strstr(p_callback_data->pMessage, "Invalid SPIR-V binary version 1.3") != nullptr) {
			return VK_FALSE;
		}
		// This needs to be ignored because Validator is wrong here.
		if (strstr(p_callback_data->pMessage, "Shader requires flag") != nullptr) {
			return VK_FALSE;
		}

		// This needs to be ignored because Validator is wrong here.
		if (strstr(p_callback_data->pMessage, "SPIR-V module not valid: Pointer operand") != nullptr && strstr(p_callback_data->pMessage, "must be a memory object") != nullptr) {
			return VK_FALSE;
		}

		if (p_callback_data->pMessageIdName && strstr(p_callback_data->pMessageIdName, "UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw") != nullptr) {
			return VK_FALSE;
		}

		std::string type_string;
		switch (p_message_type) {
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT):
			type_string = "GENERAL";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT):
			type_string = "VALIDATION";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT):
			type_string = "PERFORMANCE";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT):
			type_string = "VALIDATION|PERFORMANCE";
			break;
		}

		std::string objects_string;
		if (p_callback_data->objectCount > 0) {
			objects_string = "\n\tObjects - " + std::to_string(p_callback_data->objectCount);
			for (uint32_t object = 0; object < p_callback_data->objectCount; ++object) {
				objects_string +=
					"\n\t\tObject[" + std::to_string(object) + "]" +
					" - " + string_VkObjectType(p_callback_data->pObjects[object].objectType) +
					", Handle " + std::to_string(p_callback_data->pObjects[object].objectHandle);

				if (p_callback_data->pObjects[object].pObjectName != nullptr && strlen(p_callback_data->pObjects[object].pObjectName) > 0) {
					objects_string += ", Name \"" + std::string(p_callback_data->pObjects[object].pObjectName) + "\"";
				}
			}
		}

		std::string labels_string;
		if (p_callback_data->cmdBufLabelCount > 0) {
			labels_string = "\n\tCommand Buffer Labels - " + std::to_string(p_callback_data->cmdBufLabelCount);
			for (uint32_t cmd_buf_label = 0; cmd_buf_label < p_callback_data->cmdBufLabelCount; ++cmd_buf_label) {
				labels_string +=
					"\n\t\tLabel[" + std::to_string(cmd_buf_label) + "]" +
					" - " + p_callback_data->pCmdBufLabels[cmd_buf_label].pLabelName +
					"{ ";

				for (int color_idx = 0; color_idx < 4; ++color_idx) {
					labels_string += std::to_string(p_callback_data->pCmdBufLabels[cmd_buf_label].color[color_idx]);
					if (color_idx < 3) {
						labels_string += ", ";
					}
				}

				labels_string += " }";
			}
		}

		std::string error_message(type_string +
			" - Message Id Number: " + std::to_string(p_callback_data->messageIdNumber) +
			" | Message Id Name: " + p_callback_data->pMessageIdName +
			"\n\t" + p_callback_data->pMessage +
			objects_string + labels_string);

		// Convert VK severity to our own log macros.
		switch (p_message_severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			LOGE(error_message.c_str());
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			LOGE(error_message.c_str());
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			LOGW(error_message.c_str());
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			LOGE(error_message.c_str());
			CRASH_COND_MSG(true/*Engine::get_singleton()->is_abort_on_gpu_errors_enabled()*/, "Crashing, because abort on GPU errors is enabled.");
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			break; // Shouldn't happen, only handling to make compilers happy.
		}

		return VK_FALSE;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL Context::_debug_report_callback(VkDebugReportFlagsEXT p_flags, VkDebugReportObjectTypeEXT p_object_type, uint64_t p_object, size_t p_location, int32_t p_message_code, const char* p_layer_prefix, const char* p_message, void* p_user_data)
	{
		std::string debug_message = std::string("Vulkan Debug Report: object - ") + std::to_string(p_object) + "\n" + p_message;

		switch (p_flags) {
		case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
		case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
			LOGI(debug_message.c_str());
			break;
		case VK_DEBUG_REPORT_WARNING_BIT_EXT:
		case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
			LOGW(debug_message.c_str());
			break;
		case VK_DEBUG_REPORT_ERROR_BIT_EXT:
			LOGE(debug_message.c_str());
			break;
		}

		return VK_FALSE;
	}

	Error Context::_find_validation_layers(std::vector<const char*>& r_layer_names) const {
		r_layer_names.clear();

		uint32_t instance_layer_count = 0;
		VkResult err = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
		if (instance_layer_count > 0) {
			std::vector<VkLayerProperties> layer_properties;
			layer_properties.resize(instance_layer_count);
			err = vkEnumerateInstanceLayerProperties(&instance_layer_count, layer_properties.data());
			ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

			// Preferred set of validation layers.
			const std::initializer_list<const char*> preferred = { "VK_LAYER_KHRONOS_validation" };

			// Alternative (deprecated, removed in SDK 1.1.126.0) set of validation layers.
			const std::initializer_list<const char*> lunarg = { "VK_LAYER_LUNARG_standard_validation" };

			// Alternative (deprecated, removed in SDK 1.1.121.1) set of validation layers.
			const std::initializer_list<const char*> google = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation", "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation", "VK_LAYER_GOOGLE_unique_objects" };

			// Verify all the layers of the list are present.
			for (const std::initializer_list<const char*>& list : { preferred, lunarg, google }) {
				bool layers_found = false;
				for (const char* layer_name : list) {
					layers_found = false;

					for (const VkLayerProperties& properties : layer_properties) {
						if (!strcmp(properties.layerName, layer_name)) {
							layers_found = true;
							break;
						}
					}

					if (!layers_found) {
						break;
					}
				}

				if (layers_found) {
					r_layer_names.reserve(list.size());
					for (const char* layer_name : list) {
						r_layer_names.push_back(layer_name);
					}

					break;
				}
			}
		}

		return OK;
	}

	Error Context::_create_vulkan_instance(const VkInstanceCreateInfo* p_create_info, VkInstance* r_instance)
	{

		VkResult err = vkCreateInstance(p_create_info, nullptr, r_instance);
		ERR_FAIL_COND_V_MSG(err == VK_ERROR_INCOMPATIBLE_DRIVER, ERR_CANT_CREATE,
			"Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
			"vkCreateInstance Failure");
		ERR_FAIL_COND_V_MSG(err == VK_ERROR_EXTENSION_NOT_PRESENT, ERR_CANT_CREATE,
			"Cannot find a specified extension library.\n"
			"Make sure your layers path is set appropriately.\n"
			"vkCreateInstance Failure");
		ERR_FAIL_COND_V_MSG(err, ERR_CANT_CREATE,
			"vkCreateInstance failed.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");

		return OK;
	}

	bool Context::_use_validation_layers() const {
		return true;
	}

	void Context::_register_requested_instance_extension(const std::string& p_extension_name, bool p_required)
	{
		requested_instance_extensions[p_extension_name] = p_required;
	}

	Error Context::_initialize_instance_extensions()
	{
		enabled_instance_extension_names.clear();

		// The surface extension and the platform-specific surface extension are core requirements.
		// // TODO:
		_register_requested_instance_extension(VK_KHR_SURFACE_EXTENSION_NAME, true);
		if (_get_platform_surface_extension()) {
			_register_requested_instance_extension(_get_platform_surface_extension(), true);
		}

		if (_use_validation_layers()) {
			_register_requested_instance_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, false);
		}

		// This extension allows us to use the properties2 features to query additional device capabilities.
		_register_requested_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);

		// This extension allows us to use colorspaces other than SRGB.
		_register_requested_instance_extension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, false);

		// Only enable debug utils in verbose mode or DEV_ENABLED.
		// End users would get spammed with messages of varying verbosity due to the
		// mess that thirdparty layers/extensions and drivers seem to leave in their
		// wake, making the Windows registry a bottomless pit of broken layer JSON.
#ifdef VULKAN_DEBUG
		bool want_debug_utils = true;
#else
		bool want_debug_utils = false;
#endif
		if (want_debug_utils) {
			_register_requested_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);
		}

		// Load instance extensions that are available.
		uint32_t instance_extension_count = 0;
		VkResult err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS && err != VK_INCOMPLETE, ERR_CANT_CREATE);
		ERR_FAIL_COND_V_MSG(instance_extension_count == 0, ERR_CANT_CREATE, "No instance extensions were found.");

		std::vector<VkExtensionProperties> instance_extensions;
		instance_extensions.resize(instance_extension_count);
		err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.data());
		if (err != VK_SUCCESS && err != VK_INCOMPLETE) {
			ERR_FAIL_V(ERR_CANT_CREATE);
		}

#ifdef VULKAN_DEBUG
		for (uint32_t i = 0; i < instance_extension_count; i++) {
			print_verbose(String("VULKAN: Found instance extension ") + String::utf8(instance_extensions[i].extensionName) + String("."));
		}
#endif

		// Enable all extensions that are supported and requested.
		for (uint32_t i = 0; i < instance_extension_count; i++) {
			std::string extension_name(instance_extensions[i].extensionName);
			if (requested_instance_extensions.contains(extension_name)) {
				enabled_instance_extension_names.insert(extension_name);
			}
		}

		// Now check our requested extensions.
		for (std::pair<std::string, bool> requested_extension : requested_instance_extensions) {
			if (!enabled_instance_extension_names.contains(requested_extension.first)) {
				if (requested_extension.second) {
					std::string msg = "Required extension " + requested_extension.first + " not found.";
					ERR_FAIL_V_MSG(ERR_BUG, msg.c_str());
				}
				else {
					LOGI("Optional extension %s not found.", requested_extension.first.c_str());
				}
			}
		}

		return OK;
	}

	Context::Context()
	{
	}

	Context::~Context()
	{
		if (debug_messenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
		}

		if (debug_report != VK_NULL_HANDLE) {
			vkDestroyDebugReportCallbackEXT(instance, debug_report, nullptr);
		}

		if (instance != VK_NULL_HANDLE) {
			vkDestroyInstance(instance, nullptr);
		}
	}

	Error Context::initialize()
	{
		Error err;

	#if 0
		if (volkInitialize() != VK_SUCCESS) {
			return FAILED;
		}
	#endif

		err = _initialize_vulkan_version();
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_instance_extensions();
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_instance();
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_devices();
		ERR_FAIL_COND_V(err != OK, err);

		return OK;
	}

	Context::SurfaceID Context::set_surface(VkSurfaceKHR vk_surface) {
		Surface* surface = new Surface;
		surface->vk_surface = vk_surface;
		return SurfaceID(surface);
	}

	Context::SurfaceID Context::surface_create(const void* p_platform_data) {
		const WindowPlatformData* wpd = (const WindowPlatformData*)(p_platform_data);

		VkWin32SurfaceCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		create_info.hinstance = wpd->instance;
		create_info.hwnd = wpd->window;

		VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
		VkResult err = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &vk_surface);
		ERR_FAIL_COND_V(err != VK_SUCCESS, SurfaceID());

		Surface* surface = new Surface;
		surface->vk_surface = vk_surface;
		return SurfaceID(surface);
	}

	void Context::surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) {
		Surface* surface = (Surface*)(p_surface);
		surface->width = p_width;
		surface->height = p_height;
		surface->needs_resize = true;
	}

	void Context::surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) {
		Surface* surface = (Surface*)(p_surface);
		surface->hdr_output = p_enabled;
		surface->needs_resize = true;
	}

	bool Context::surface_get_hdr_output_enabled(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->hdr_output;
	}

	void Context::surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) {
		Surface* surface = (Surface*)(p_surface);
		surface->hdr_reference_luminance = p_reference_luminance;
	}

	float Context::surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->hdr_reference_luminance;
	}

	void Context::surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) {
		Surface* surface = (Surface*)(p_surface);
		surface->hdr_max_luminance = p_max_luminance;
	}

	float Context::surface_get_hdr_output_max_luminance(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->hdr_max_luminance;
	}

	void Context::surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) {
		Surface* surface = (Surface*)(p_surface);
		surface->hdr_linear_luminance_scale = p_linear_luminance_scale;
	}

	float Context::surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->hdr_linear_luminance_scale;
	}

	float Context::surface_get_hdr_output_max_value(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return std::fmax(surface->hdr_max_luminance / std::fmax(surface->hdr_reference_luminance, 1.0f), 1.0f);
	}

	uint32_t Context::surface_get_width(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->width;
	}

	uint32_t Context::surface_get_height(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->height;
	}

	void Context::surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) {
		Surface* surface = (Surface*)(p_surface);
		surface->needs_resize = p_needs_resize;
	}

	bool Context::surface_get_needs_resize(SurfaceID p_surface) const {
		Surface* surface = (Surface*)(p_surface);
		return surface->needs_resize;
	}

	void Context::surface_destroy(SurfaceID p_surface) {
		Surface* surface = (Surface*)(p_surface);
		vkDestroySurfaceKHR(instance, surface->vk_surface, nullptr);
		delete surface;
	}

	const Context::Device& Context::device_get(uint32_t p_device_index) const {
		DEBUG_ASSERT(p_device_index < driver_devices.size());
		return driver_devices[p_device_index];
	}

	uint32_t Context::device_get_count() const {
		return driver_devices.size();
	}

	bool Context::is_debug_utils_enabled() const {
		return enabled_instance_extension_names.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	bool Context::is_colorspace_supported() const {
		return enabled_instance_extension_names.contains(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
	}

	VkInstance Context::instance_get() const {
		return instance;
	}

	VkPhysicalDevice Context::physical_device_get(uint32_t p_device_index) const {
		DEBUG_ASSERT(p_device_index < physical_devices.size());
		return physical_devices[p_device_index];
	}

	uint32_t Context::queue_family_get_count(uint32_t p_device_index) const {
		DEBUG_ASSERT(p_device_index < physical_devices.size());
		return device_queue_families[p_device_index].properties.size();
	}

	VkQueueFamilyProperties Context::queue_family_get(uint32_t p_device_index, uint32_t p_queue_family_index) const {
		DEBUG_ASSERT(p_device_index < physical_devices.size());
		DEBUG_ASSERT(p_queue_family_index < queue_family_get_count(p_device_index));
		return device_queue_families[p_device_index].properties[p_queue_family_index];
	}
}
