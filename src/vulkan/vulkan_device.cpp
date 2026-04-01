/*****************************************************************//**
 * \file   vulkan_device.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "vulkan_device.h"
#include "libassert/assert.hpp"
#include "util/error_macros.h"
#include <array>

// Enable the use of re-spirv for optimizing shaders after applying specialization constants.
#define RESPV_ENABLED 0

// Only enable function inlining for re-spirv when dealing with a shader that uses specialization constants.
#define RESPV_ONLY_INLINE_SHADERS_WITH_SPEC_CONSTANTS 1

#define VULKAN_DEBUG

namespace Vulkan
{
#pragma region Generic
	

	std::string hex_encode_buffer(const uint8_t* buffer, int len) {
		static const char hex[] = "0123456789abcdef";

		std::string ret;
		ret.resize(len * 2);

		char* out = ret.data();

		for (int i = 0; i < len; ++i) {
			*out++ = hex[buffer[i] >> 4];
			*out++ = hex[buffer[i] & 0x0F];
		}

		return ret;
	}

	static VkAccessFlags _rd_to_vk_access_flags(BitField<RenderingDeviceDriverVulkan::BarrierAccessBits> p_access) {
		VkAccessFlags vk_flags = 0;
		if (p_access.has_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_COPY_READ_BIT) || p_access.has_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_RESOLVE_READ_BIT)) {
			vk_flags |= VK_ACCESS_TRANSFER_READ_BIT;
			p_access.clear_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_COPY_READ_BIT);
			p_access.clear_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_RESOLVE_READ_BIT);
		}

		if (p_access.has_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_COPY_WRITE_BIT) || p_access.has_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_RESOLVE_WRITE_BIT)) {
			vk_flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
			p_access.clear_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_COPY_WRITE_BIT);
			p_access.clear_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_RESOLVE_WRITE_BIT);
		}

		if (p_access.has_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_STORAGE_CLEAR_BIT)) {
			// Vulkan should never use this as API_TRAIT_CLEAR_RESOURCES_WITH_VIEWS is not specified.
			// Therefore, storage is never cleared with an explicit command.
			p_access.clear_flag(RenderingDeviceDriverVulkan::BARRIER_ACCESS_STORAGE_CLEAR_BIT);
		}

		// The rest of the flags have compatible numeric values with Vulkan.
		return VkAccessFlags(p_access) | vk_flags;
	}

	static VkImageLayout RD_TO_VK_LAYOUT[RenderingDeviceDriverVulkan::TEXTURE_LAYOUT_MAX] = {
	VK_IMAGE_LAYOUT_UNDEFINED, // TEXTURE_LAYOUT_UNDEFINED
	VK_IMAGE_LAYOUT_GENERAL, // TEXTURE_LAYOUT_GENERAL
	VK_IMAGE_LAYOUT_GENERAL, // TEXTURE_LAYOUT_STORAGE_OPTIMAL
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, // TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // TEXTURE_LAYOUT_COPY_SRC_OPTIMAL
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // TEXTURE_LAYOUT_COPY_DST_OPTIMAL
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL
	VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, // TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL
	VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, // TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL
	};

	static VkPipelineStageFlags _rd_to_vk_pipeline_stages(BitField<RenderingDeviceDriverVulkan::PipelineStageBits> p_stages) {
		VkPipelineStageFlags vk_flags = 0;
		if (p_stages.has_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_COPY_BIT) || p_stages.has_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_RESOLVE_BIT)) {
			// Transfer has been split into copy and resolve bits. Clear them and merge them into one bit.
			vk_flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			p_stages.clear_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_COPY_BIT);
			p_stages.clear_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_RESOLVE_BIT);
		}

		if (p_stages.has_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_CLEAR_STORAGE_BIT)) {
			// Vulkan should never use this as API_TRAIT_CLEAR_RESOURCES_WITH_VIEWS is not specified.
			// Therefore, storage is never cleared with an explicit command.
			p_stages.clear_flag(RenderingDeviceDriverVulkan::PIPELINE_STAGE_CLEAR_STORAGE_BIT);
		}

		// The rest of the flags have compatible numeric values with Vulkan.
		return VkPipelineStageFlags(p_stages) | vk_flags;
	}

	static const uint32_t MAX_DYNAMIC_BUFFERS = 8u;

	// Debug marker extensions.

	VkDebugReportObjectTypeEXT RenderingDeviceDriverVulkan::_convert_to_debug_report_objectType(VkObjectType p_object_type) {
		switch (p_object_type) {
		case VK_OBJECT_TYPE_UNKNOWN:
			return VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;
		case VK_OBJECT_TYPE_INSTANCE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT;
		case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT;
		case VK_OBJECT_TYPE_DEVICE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT;
		case VK_OBJECT_TYPE_QUEUE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT;
		case VK_OBJECT_TYPE_SEMAPHORE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT;
		case VK_OBJECT_TYPE_COMMAND_BUFFER:
			return VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT;
		case VK_OBJECT_TYPE_FENCE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT;
		case VK_OBJECT_TYPE_DEVICE_MEMORY:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT;
		case VK_OBJECT_TYPE_BUFFER:
			return VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
		case VK_OBJECT_TYPE_IMAGE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		case VK_OBJECT_TYPE_EVENT:
			return VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT;
		case VK_OBJECT_TYPE_QUERY_POOL:
			return VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT;
		case VK_OBJECT_TYPE_BUFFER_VIEW:
			return VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT;
		case VK_OBJECT_TYPE_IMAGE_VIEW:
			return VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT;
		case VK_OBJECT_TYPE_SHADER_MODULE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT;
		case VK_OBJECT_TYPE_PIPELINE_CACHE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT;
		case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
			return VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT;
		case VK_OBJECT_TYPE_RENDER_PASS:
			return VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT;
		case VK_OBJECT_TYPE_PIPELINE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT;
		case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT;
		case VK_OBJECT_TYPE_SAMPLER:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT;
		case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT;
		case VK_OBJECT_TYPE_DESCRIPTOR_SET:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT;
		case VK_OBJECT_TYPE_FRAMEBUFFER:
			return VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT;
		case VK_OBJECT_TYPE_COMMAND_POOL:
			return VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT;
		case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT;
		case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT;
		case VK_OBJECT_TYPE_SURFACE_KHR:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT;
		case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
			return VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT;
		case VK_OBJECT_TYPE_DISPLAY_KHR:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT;
		case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT;
		case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
			return VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT;
		case VK_OBJECT_TYPE_CU_MODULE_NVX:
			return VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT;
		case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
			return VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT;
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
			return VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT;
		case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
			return VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT;
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
			return VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT;
		default:
			break;
		}

		return VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;
	}

	void RenderingDeviceDriverVulkan::_set_object_name(VkObjectType p_object_type, uint64_t p_object_handle, std::string p_object_name) {
		
		if (vkSetDebugUtilsObjectNameEXT != nullptr) {		// recommended successor to vkDebugMarkerSetObjectNameEXT
			std::string obj_data = p_object_name;
			VkDebugUtilsObjectNameInfoEXT name_info;
			name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			name_info.pNext = nullptr;
			name_info.objectType = p_object_type;
			name_info.objectHandle = p_object_handle;
			name_info.pObjectName = obj_data.data();
			vkSetDebugUtilsObjectNameEXT(vk_device, &name_info);
		}
		else if (vkDebugMarkerSetObjectNameEXT != nullptr) {
			// Debug marker extensions.
			std::string obj_data = p_object_name;
			VkDebugMarkerObjectNameInfoEXT name_info;
			name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			name_info.pNext = nullptr;
			name_info.objectType = _convert_to_debug_report_objectType(p_object_type);
			name_info.object = p_object_handle;
			name_info.pObjectName = obj_data.data();
			vkDebugMarkerSetObjectNameEXT(vk_device, &name_info);
		}
	}

#pragma endregion

#pragma region Device
	RenderingDeviceDriverVulkan::RenderingDeviceDriverVulkan(RenderingContextDriverVulkan* p_context_driver)
	{
		DEBUG_ASSERT(p_context_driver != nullptr);

		context_driver = p_context_driver;
		//TODO:
		max_descriptor_sets_per_pool = 3;
	}

	Error RenderingDeviceDriverVulkan::initialize(uint32_t p_device_index, uint32_t p_frame_count)
	{
		context_device = context_driver->device_get(p_device_index);
		physical_device = context_driver->physical_device_get(p_device_index);
		vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

		frame_count = p_frame_count;

		uint32_t queue_family_count = context_driver->queue_family_get_count(p_device_index);
		queue_family_properties.resize(queue_family_count);
		for (uint32_t i = 0; i < queue_family_count; i++) {
			queue_family_properties[i] = context_driver->queue_family_get(p_device_index, i);
		}

		Error err = _initialize_device_extensions();
		ERR_FAIL_COND_V(err != OK, err);

		err = _check_device_features();
		ERR_FAIL_COND_V(err != OK, err);

		err = _check_device_capabilities();
		ERR_FAIL_COND_V(err != OK, err);

		std::vector<VkDeviceQueueCreateInfo> queue_create_info;
		err = _add_queue_create_info(queue_create_info);
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_device(queue_create_info);
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_allocator();
		ERR_FAIL_COND_V(err != OK, err);

		err = _initialize_pipeline_cache();
		ERR_FAIL_COND_V(err != OK, err);

	}

	void RenderingDeviceDriverVulkan::finalize()
	{
		//TODO:
	}

	void RenderingDeviceDriverVulkan::_register_requested_device_extension(const std::string& p_extension_name, bool p_required) {
		ERR_FAIL_COND(requested_device_extensions.contains(p_extension_name));
		requested_device_extensions[p_extension_name] = p_required;
	}

	Error RenderingDeviceDriverVulkan::_initialize_device_extensions() {
		enabled_device_extension_names.clear();

		_register_requested_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);
		_register_requested_device_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_16BIT_STORAGE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_MAINTENANCE_2_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME, false);
		_register_requested_device_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, false);

		// We don't actually use this extension, but some runtime components on some platforms
		// can and will fill the validation layers with useless info otherwise if not enabled.
		_register_requested_device_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, false);

		// TODO:
		//if (Engine::get_singleton()->is_generate_spirv_debug_info_enabled()) {
		//	_register_requested_device_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, true);
		//}

		_register_requested_device_extension(VK_EXT_DEVICE_FAULT_EXTENSION_NAME, false);

		{
			// Debug marker extensions.
			// Should be last element in the array.
#ifdef DEV_ENABLED
			bool want_debug_markers = true;
#else
			bool want_debug_markers = false/*OS::get_singleton()->is_stdout_verbose()*/;
#endif
			if (want_debug_markers) {
				_register_requested_device_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false);
			}
		}

		uint32_t device_extension_count = 0;
		VkResult err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
		ERR_FAIL_COND_V_MSG(device_extension_count == 0, ERR_CANT_CREATE, "vkEnumerateDeviceExtensionProperties failed to find any extensions\n\nDo you have a compatible Vulkan installable client driver (ICD) installed?");

		std::vector<VkExtensionProperties> device_extensions;
		device_extensions.resize(device_extension_count);
		err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, device_extensions.data());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);


#ifdef VULKAN_DEBUG
		for (uint32_t i = 0; i < device_extension_count; i++) {
			LOGI(std::format("VULKAN: Found device extension {}", std::string(device_extensions[i].extensionName)).c_str());
		}
#endif

		// Enable all extensions that are supported and requested.
		for (uint32_t i = 0; i < device_extension_count; i++) {
			std::string extension_name(device_extensions[i].extensionName);
			if (requested_device_extensions.contains(extension_name)) {
				enabled_device_extension_names.insert(extension_name);
			}
		}

		// Now check our requested extensions.
		for (std::pair<std::string, bool> requested_extension : requested_device_extensions) {
			if (!enabled_device_extension_names.contains(requested_extension.first)) {
				if (requested_extension.second) {
					std::string msg = "Required extension " + requested_extension.first + "not found.";
					ERR_FAIL_V_MSG(ERR_BUG, msg.c_str());
				}
				else {
					LOGI("Optional extension %s not found.", requested_extension.first.c_str());
				}
			}
		}

		return OK;
	}

	Error RenderingDeviceDriverVulkan::_check_device_features() {
		vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

		// Check for required features.
		if (!physical_device_features.imageCubeArray || !physical_device_features.independentBlend) {
			std::string error_string = std::format("Your GPU ({}) does not support the following features which are required to use Vulkan-based renderers in Godot:\n\n", context_device.name);
			if (!physical_device_features.imageCubeArray) {
				error_string += "- No support for image cube arrays.\n";
			}
			if (!physical_device_features.independentBlend) {
				error_string += "- No support for independentBlend.\n";
			}
			error_string += "\nThis is usually a hardware limitation, so updating graphics drivers won't help in most cases.";


			LOGE("%s\n", error_string);

			return ERR_CANT_CREATE;
		}

		// Opt-in to the features we actually need/use. These can be changed in the future.
		// We do this for multiple reasons:
		//
		//	1. Certain features (like sparse* stuff) cause unnecessary internal driver allocations.
		//	2. Others like shaderStorageImageMultisample are a huge red flag
		//	   (MSAA + Storage is rarely needed).
		//	3. Most features when turned off aren't actually off (we just promise the driver not to use them)
		//	   and it is validation what will complain. This allows us to target a minimum baseline.
		//
		// TODO: Allow the user to override these settings (i.e. turn off more stuff) using profiles
		// so they can target a broad range of HW. For example Mali HW does not have
		// shaderClipDistance/shaderCullDistance; thus validation would complain if such feature is used;
		// allowing them to fix the problem without even owning Mali HW to test on.
		//
		// The excluded features are:
		// - robustBufferAccess (can hamper performance on some hardware)
		// - occlusionQueryPrecise
		// - pipelineStatisticsQuery
		// - shaderStorageImageMultisample (unsupported by Intel Arc, prevents from using MSAA storage accidentally)
		// - shaderResourceResidency
		// - sparseBinding (we don't use sparse features and enabling them cause extra internal allocations inside the Vulkan driver we don't need)
		// - sparseResidencyBuffer
		// - sparseResidencyImage2D
		// - sparseResidencyImage3D
		// - sparseResidency2Samples
		// - sparseResidency4Samples
		// - sparseResidency8Samples
		// - sparseResidency16Samples
		// - sparseResidencyAliased
		// - inheritedQueries

#define VK_DEVICEFEATURE_ENABLE_IF(x) \
	if (physical_device_features.x) { \
		requested_device_features.x = physical_device_features.x; \
	} else \
		((void)0)

		requested_device_features = {};
		VK_DEVICEFEATURE_ENABLE_IF(fullDrawIndexUint32);
		VK_DEVICEFEATURE_ENABLE_IF(imageCubeArray);
		VK_DEVICEFEATURE_ENABLE_IF(independentBlend);
		VK_DEVICEFEATURE_ENABLE_IF(geometryShader);
		VK_DEVICEFEATURE_ENABLE_IF(tessellationShader);
		VK_DEVICEFEATURE_ENABLE_IF(sampleRateShading);
		VK_DEVICEFEATURE_ENABLE_IF(dualSrcBlend);
		VK_DEVICEFEATURE_ENABLE_IF(logicOp);
		VK_DEVICEFEATURE_ENABLE_IF(multiDrawIndirect);
		VK_DEVICEFEATURE_ENABLE_IF(drawIndirectFirstInstance);
		VK_DEVICEFEATURE_ENABLE_IF(depthClamp);
		VK_DEVICEFEATURE_ENABLE_IF(depthBiasClamp);
		VK_DEVICEFEATURE_ENABLE_IF(fillModeNonSolid);
		VK_DEVICEFEATURE_ENABLE_IF(depthBounds);
		VK_DEVICEFEATURE_ENABLE_IF(wideLines);
		VK_DEVICEFEATURE_ENABLE_IF(largePoints);
		VK_DEVICEFEATURE_ENABLE_IF(alphaToOne);
		VK_DEVICEFEATURE_ENABLE_IF(multiViewport);
		VK_DEVICEFEATURE_ENABLE_IF(samplerAnisotropy);
		VK_DEVICEFEATURE_ENABLE_IF(textureCompressionETC2);
		VK_DEVICEFEATURE_ENABLE_IF(textureCompressionASTC_LDR);
		VK_DEVICEFEATURE_ENABLE_IF(textureCompressionBC);
		VK_DEVICEFEATURE_ENABLE_IF(vertexPipelineStoresAndAtomics);
		VK_DEVICEFEATURE_ENABLE_IF(fragmentStoresAndAtomics);
		VK_DEVICEFEATURE_ENABLE_IF(shaderTessellationAndGeometryPointSize);
		VK_DEVICEFEATURE_ENABLE_IF(shaderImageGatherExtended);
		VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageExtendedFormats);
		VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageReadWithoutFormat);
		VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageWriteWithoutFormat);
		VK_DEVICEFEATURE_ENABLE_IF(shaderUniformBufferArrayDynamicIndexing);
		VK_DEVICEFEATURE_ENABLE_IF(shaderSampledImageArrayDynamicIndexing);
		VK_DEVICEFEATURE_ENABLE_IF(shaderStorageBufferArrayDynamicIndexing);
		VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageArrayDynamicIndexing);
		VK_DEVICEFEATURE_ENABLE_IF(shaderClipDistance);
		VK_DEVICEFEATURE_ENABLE_IF(shaderCullDistance);
		VK_DEVICEFEATURE_ENABLE_IF(shaderFloat64);
		VK_DEVICEFEATURE_ENABLE_IF(shaderInt64);
		VK_DEVICEFEATURE_ENABLE_IF(shaderInt16);
		VK_DEVICEFEATURE_ENABLE_IF(shaderResourceMinLod);
		VK_DEVICEFEATURE_ENABLE_IF(variableMultisampleRate);

		return OK;
	}

	Error RenderingDeviceDriverVulkan::_check_device_capabilities() {
		// Fill device family and version.
		device_capabilities.device_family = DEVICE_VULKAN;
		device_capabilities.version_major = VK_API_VERSION_MAJOR(physical_device_properties.apiVersion);
		device_capabilities.version_minor = VK_API_VERSION_MINOR(physical_device_properties.apiVersion);

		// Cache extension availability we query often.
		framebuffer_depth_resolve = enabled_device_extension_names.contains(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);

		bool use_fdm_offsets = false;
		//if (VulkanHooks::get_singleton() != nullptr) {
		//	use_fdm_offsets = VulkanHooks::get_singleton()->use_fragment_density_offsets();
		//}

		// References:
		// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_KHR_multiview.html
		// https://www.khronos.org/blog/vulkan-subgroup-tutorial
		if (vkGetPhysicalDeviceFeatures2 != nullptr)
		{
			// We must check that the corresponding extension is present before assuming a feature as enabled.
			// See also: https://github.com/godotengine/godot/issues/65409

			void* next_features = nullptr;
			VkPhysicalDeviceVulkan12Features device_features_vk_1_2 = {};
			VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_features = {};
			VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features = {};
			VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model_features = {};
			VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsr_features = {};
			VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdm_features = {};
			VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fdmo_features_qcom = {};
			VkPhysicalDevice16BitStorageFeaturesKHR storage_feature = {};
			VkPhysicalDeviceMultiviewFeatures multiview_features = {};
			VkPhysicalDevicePipelineCreationCacheControlFeatures pipeline_cache_control_features = {};
			VkPhysicalDeviceVulkanMemoryModelFeatures memory_model_features = {};
			VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_pipeline_features = {};
			VkPhysicalDeviceSynchronization2FeaturesKHR sync_2_features = {};
			VkPhysicalDeviceRayTracingValidationFeaturesNV raytracing_validation_features = {};

			const bool use_1_2_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_2;
			if (use_1_2_features) {
				device_features_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
				device_features_vk_1_2.pNext = next_features;
				next_features = &device_features_vk_1_2;
			}
			else {
				if (enabled_device_extension_names.contains(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
					shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
					shader_features.pNext = next_features;
					next_features = &shader_features;
				}
				if (enabled_device_extension_names.contains(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
					buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
					buffer_device_address_features.pNext = next_features;
					next_features = &buffer_device_address_features;
				}
				if (enabled_device_extension_names.contains(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
					vulkan_memory_model_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
					vulkan_memory_model_features.pNext = next_features;
					next_features = &vulkan_memory_model_features;
				}
			}

			if (enabled_device_extension_names.contains(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) {
				fsr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
				fsr_features.pNext = next_features;
				next_features = &fsr_features;
			}

			if (enabled_device_extension_names.contains(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
				fdm_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
				fdm_features.pNext = next_features;
				next_features = &fdm_features;
			}

			if (use_fdm_offsets && enabled_device_extension_names.contains(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
				fdmo_features_qcom.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM;
				fdmo_features_qcom.pNext = next_features;
				next_features = &fdmo_features_qcom;
			}

			if (enabled_device_extension_names.contains(VK_KHR_16BIT_STORAGE_EXTENSION_NAME)) {
				storage_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
				storage_feature.pNext = next_features;
				next_features = &storage_feature;
			}

			if (enabled_device_extension_names.contains(VK_KHR_MULTIVIEW_EXTENSION_NAME)) {
				multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
				multiview_features.pNext = next_features;
				next_features = &multiview_features;
			}

			if (enabled_device_extension_names.contains(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
				pipeline_cache_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES;
				pipeline_cache_control_features.pNext = next_features;
				next_features = &pipeline_cache_control_features;
			}

			if (enabled_device_extension_names.contains(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
				memory_model_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
				memory_model_features.pNext = next_features;
				next_features = &memory_model_features;
			}

			if (enabled_device_extension_names.contains(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) {
				acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
				acceleration_structure_features.pNext = next_features;
				next_features = &acceleration_structure_features;
			}

			if (enabled_device_extension_names.contains(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) {
				raytracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
				raytracing_pipeline_features.pNext = next_features;
				next_features = &raytracing_pipeline_features;
			}

			if (enabled_device_extension_names.contains(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME)) {
				raytracing_validation_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV;
				raytracing_validation_features.pNext = next_features;
				next_features = &raytracing_validation_features;
			}

			if (enabled_device_extension_names.contains(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
				sync_2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
				sync_2_features.pNext = next_features;
				next_features = &sync_2_features;
			}

			VkPhysicalDeviceFeatures2 device_features_2 = {};
			device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			device_features_2.pNext = next_features;
			vkGetPhysicalDeviceFeatures2(physical_device, &device_features_2);

			if (use_1_2_features) {
#ifdef MACOS_ENABLED
				ERR_FAIL_COND_V_MSG(!device_features_vk_1_2.shaderSampledImageArrayNonUniformIndexing, ERR_CANT_CREATE, "Your GPU doesn't support shaderSampledImageArrayNonUniformIndexing which is required to use the Vulkan-based renderers in Godot.");
#endif
				if (enabled_device_extension_names.contains(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
					shader_capabilities.shader_float16_is_supported = device_features_vk_1_2.shaderFloat16;
					shader_capabilities.shader_int8_is_supported = device_features_vk_1_2.shaderInt8;
				}
				if (enabled_device_extension_names.contains(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
					buffer_device_address_support = device_features_vk_1_2.bufferDeviceAddress;
				}
				if (enabled_device_extension_names.contains(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
					vulkan_memory_model_support = device_features_vk_1_2.vulkanMemoryModel;
					vulkan_memory_model_device_scope_support = device_features_vk_1_2.vulkanMemoryModelDeviceScope;
				}
			}
			else {
				if (enabled_device_extension_names.contains(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
					shader_capabilities.shader_float16_is_supported = shader_features.shaderFloat16;
					shader_capabilities.shader_int8_is_supported = shader_features.shaderInt8;
				}
				if (enabled_device_extension_names.contains(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
					buffer_device_address_support = buffer_device_address_features.bufferDeviceAddress;
				}
				if (enabled_device_extension_names.contains(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
					vulkan_memory_model_support = vulkan_memory_model_features.vulkanMemoryModel;
					vulkan_memory_model_device_scope_support = vulkan_memory_model_features.vulkanMemoryModelDeviceScope;
				}
			}

			if (enabled_device_extension_names.contains(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
				pipeline_cache_control_support = pipeline_cache_control_features.pipelineCreationCacheControl;
			}

			if (enabled_device_extension_names.contains(VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
				device_fault_support = true;
			}
#if defined(VK_TRACK_DEVICE_MEMORY)
			if (enabled_device_extension_names.has(VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME)) {
				device_memory_report_support = true;
			}
#endif
		}

		if (vkGetPhysicalDeviceProperties2 != nullptr) {
			void* next_properties = nullptr;
			VkPhysicalDeviceProperties2 physical_device_properties_2 = {};


			physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			physical_device_properties_2.pNext = next_properties;
			vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties_2);


			// TODO: FSR, FDM, Subgroups, Raytracing support
		}

		return OK;
	}

	Error RenderingDeviceDriverVulkan::_add_queue_create_info(std::vector<VkDeviceQueueCreateInfo>& r_queue_create_info) {
		uint32_t queue_family_count = queue_family_properties.size();
		queue_families.resize(queue_family_count);

		VkQueueFlags queue_flags_mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		const uint32_t max_queue_count_per_family = 1;
		static const float queue_priorities[max_queue_count_per_family] = {};
		for (uint32_t i = 0; i < queue_family_count; i++) {
			if ((queue_family_properties[i].queueFlags & queue_flags_mask) == 0) {
				// We ignore creating queues in families that don't support any of the operations we require.
				continue;
			}

			VkDeviceQueueCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			create_info.queueFamilyIndex = i;
			create_info.queueCount = MIN(queue_family_properties[i].queueCount, max_queue_count_per_family);
			create_info.pQueuePriorities = queue_priorities;
			r_queue_create_info.push_back(create_info);

			// Prepare the vectors where the queues will be filled out.
			queue_families[i].resize(create_info.queueCount);
		}

		return OK;
	}

	Error RenderingDeviceDriverVulkan::_initialize_device(const std::vector<VkDeviceQueueCreateInfo>& p_queue_create_info) {
		std::vector<const char*> enabled_extension_names;
		enabled_extension_names.reserve(enabled_device_extension_names.size());
		for (const std::string& extension_name : enabled_device_extension_names) {
			enabled_extension_names.push_back(extension_name.data());
		}

		void* create_info_next = nullptr;
		VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_features = {};
		shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
		shader_features.pNext = create_info_next;
		shader_features.shaderFloat16 = shader_capabilities.shader_float16_is_supported;
		shader_features.shaderInt8 = shader_capabilities.shader_int8_is_supported;
		create_info_next = &shader_features;

		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features = {};
		if (buffer_device_address_support) {
			buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
			buffer_device_address_features.pNext = create_info_next;
			buffer_device_address_features.bufferDeviceAddress = buffer_device_address_support;
			create_info_next = &buffer_device_address_features;
		}

		VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model_features = {};
		if (vulkan_memory_model_support && vulkan_memory_model_device_scope_support) {
			vulkan_memory_model_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
			vulkan_memory_model_features.pNext = create_info_next;
			vulkan_memory_model_features.vulkanMemoryModel = vulkan_memory_model_support;
			vulkan_memory_model_features.vulkanMemoryModelDeviceScope = vulkan_memory_model_device_scope_support;
			create_info_next = &vulkan_memory_model_features;
		}

		VkPhysicalDevicePipelineCreationCacheControlFeatures pipeline_cache_control_features = {};
		if (pipeline_cache_control_support) {
			pipeline_cache_control_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES;
			pipeline_cache_control_features.pNext = create_info_next;
			pipeline_cache_control_features.pipelineCreationCacheControl = pipeline_cache_control_support;
			create_info_next = &pipeline_cache_control_features;
		}

		VkPhysicalDeviceFaultFeaturesEXT device_fault_features = {};
		if (device_fault_support) {
			device_fault_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
			device_fault_features.pNext = create_info_next;
			create_info_next = &device_fault_features;
		}

		VkPhysicalDeviceVulkan11Features vulkan_1_1_features = {};
		VkPhysicalDevice16BitStorageFeaturesKHR storage_features = {};
		VkPhysicalDeviceMultiviewFeatures multiview_features = {};
		const bool enable_1_2_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_2;
		if (enable_1_2_features) {
			// In Vulkan 1.2 and newer we use a newer struct to enable various features.
			vulkan_1_1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			vulkan_1_1_features.pNext = create_info_next;
			vulkan_1_1_features.storageBuffer16BitAccess = false/*storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported*/;
			vulkan_1_1_features.uniformAndStorageBuffer16BitAccess = false/*storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported*/;
			vulkan_1_1_features.storagePushConstant16 = false/*storage_buffer_capabilities.storage_push_constant_16_is_supported*/;
			vulkan_1_1_features.storageInputOutput16 = false/*storage_buffer_capabilities.storage_input_output_16*/;
			vulkan_1_1_features.multiview = false/*multiview_capabilities.is_supported*/;
			vulkan_1_1_features.multiviewGeometryShader = false/*multiview_capabilities.geometry_shader_is_supported*/;
			vulkan_1_1_features.multiviewTessellationShader = false/*multiview_capabilities.tessellation_shader_is_supported*/;
			vulkan_1_1_features.variablePointersStorageBuffer = 0;
			vulkan_1_1_features.variablePointers = 0;
			vulkan_1_1_features.protectedMemory = 0;
			vulkan_1_1_features.samplerYcbcrConversion = 0;
			vulkan_1_1_features.shaderDrawParameters = 0;
			create_info_next = &vulkan_1_1_features;
		}
		else {
			// On Vulkan 1.0 and 1.1 we use our older structs to initialize these features.
			storage_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
			storage_features.pNext = create_info_next;
			storage_features.storageBuffer16BitAccess = false/*storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported*/;
			storage_features.uniformAndStorageBuffer16BitAccess = false/*storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported*/;
			storage_features.storagePushConstant16 = false/* storage_buffer_capabilities.storage_push_constant_16_is_supported*/;
			storage_features.storageInputOutput16 = false/*storage_buffer_capabilities.storage_input_output_16*/;
			create_info_next = &storage_features;

			const bool enable_1_1_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_1;
			if (enable_1_1_features) {
				multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
				multiview_features.pNext = create_info_next;
				multiview_features.multiview = false/*multiview_capabilities.is_supported*/;
				multiview_features.multiviewGeometryShader = false/*multiview_capabilities.geometry_shader_is_supported*/;
				multiview_features.multiviewTessellationShader = false/*multiview_capabilities.tessellation_shader_is_supported*/;
				create_info_next = &multiview_features;
			}
		}

		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pNext = create_info_next;
		create_info.queueCreateInfoCount = p_queue_create_info.size();
		create_info.pQueueCreateInfos = p_queue_create_info.data();
		create_info.enabledExtensionCount = enabled_extension_names.size();
		create_info.ppEnabledExtensionNames = enabled_extension_names.data();
		create_info.pEnabledFeatures = &requested_device_features;

		/*		if (VulkanHooks::get_singleton() != nullptr) {
					bool device_created = VulkanHooks::get_singleton()->create_vulkan_device(&create_info, &vk_device);
					ERR_FAIL_COND_V(!device_created, ERR_CANT_CREATE);
				}
				else */
		{
			VkResult err = vkCreateDevice(physical_device, &create_info, nullptr, &vk_device);
			ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
		}

		volkLoadDevice(vk_device);  // loads all device functions for this VkDevice

		for (uint32_t i = 0; i < queue_families.size(); i++) {
			for (uint32_t j = 0; j < queue_families[i].size(); j++) {
				vkGetDeviceQueue(vk_device, i, j, &queue_families[i][j].queue);
			}
		}
		return OK;
	}

	Error RenderingDeviceDriverVulkan::_initialize_allocator() {
		VmaAllocatorCreateInfo allocator_info = {};
		allocator_info.physicalDevice = physical_device;
		allocator_info.device = vk_device;
		allocator_info.instance = context_driver->instance_get();
		const bool use_1_3_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_3;
		if (use_1_3_features) {
			allocator_info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;
		}
		if (buffer_device_address_support) {
			allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		}

		VmaVulkanFunctions vulkanFunctions{};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
		allocator_info.pVulkanFunctions = &vulkanFunctions;

		VkResult err = vmaCreateAllocator(&allocator_info, &allocator);
		std::string msg = "vmaCreateAllocator failed with error " + std::to_string(err) + ".";
		ERR_FAIL_COND_V_MSG(err, ERR_CANT_CREATE, msg);

		return OK;
	}

	static void _convert_subpass_attachments(const VkAttachmentReference2* p_attachment_references_2, uint32_t p_attachment_references_count, std::vector<VkAttachmentReference>& r_attachment_references) {
		r_attachment_references.resize(p_attachment_references_count);
		for (uint32_t i = 0; i < p_attachment_references_count; i++) {
			// Ignore sType, pNext and aspectMask (which is currently unused).
			r_attachment_references[i].attachment = p_attachment_references_2[i].attachment;
			r_attachment_references[i].layout = p_attachment_references_2[i].layout;
		}
	}

	Error RenderingDeviceDriverVulkan::_initialize_pipeline_cache() {
		pipelines_cache.buffer.resize(sizeof(PipelineCacheHeader));
		PipelineCacheHeader* header = (PipelineCacheHeader*)(pipelines_cache.buffer.data());
		*header = {};
		header->magic = 868 + VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
		header->device_id = physical_device_properties.deviceID;
		header->vendor_id = physical_device_properties.vendorID;
		header->driver_version = physical_device_properties.driverVersion;
		memcpy(header->uuid, physical_device_properties.pipelineCacheUUID, VK_UUID_SIZE);
		header->driver_abi = sizeof(void*);

		pipeline_cache_id = hex_encode_buffer(physical_device_properties.pipelineCacheUUID, VK_UUID_SIZE);
		pipeline_cache_id += "-driver-" + std::to_string(physical_device_properties.driverVersion);

		return OK;
	}

	VkResult RenderingDeviceDriverVulkan::_create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2* p_create_info, const VkAllocationCallbacks* p_allocator, VkRenderPass* p_render_pass) {
		if (vkCreateRenderPass2KHR != nullptr) {
			return vkCreateRenderPass2KHR(p_device, p_create_info, p_allocator, p_render_pass);
		}
		else {
			// Compatibility fallback with regular create render pass but by converting the inputs from the newer version to the older one.
			std::vector<VkAttachmentDescription> attachments;
			attachments.resize(p_create_info->attachmentCount);
			for (uint32_t i = 0; i < p_create_info->attachmentCount; i++) {
				// Ignores sType and pNext from the attachment.
				const VkAttachmentDescription2& src = p_create_info->pAttachments[i];
				VkAttachmentDescription& dst = attachments[i];
				dst.flags = src.flags;
				dst.format = src.format;
				dst.samples = src.samples;
				dst.loadOp = src.loadOp;
				dst.storeOp = src.storeOp;
				dst.stencilLoadOp = src.stencilLoadOp;
				dst.stencilStoreOp = src.stencilStoreOp;
				dst.initialLayout = src.initialLayout;
				dst.finalLayout = src.finalLayout;
			}

			const uint32_t attachment_vectors_per_subpass = 4;
			std::vector<std::vector<VkAttachmentReference>> subpasses_attachments;
			std::vector<VkSubpassDescription> subpasses;
			subpasses_attachments.resize(p_create_info->subpassCount * attachment_vectors_per_subpass);
			subpasses.resize(p_create_info->subpassCount);

			for (uint32_t i = 0; i < p_create_info->subpassCount; i++) {
				const uint32_t vector_base_index = i * attachment_vectors_per_subpass;
				const uint32_t input_attachments_index = vector_base_index + 0;
				const uint32_t color_attachments_index = vector_base_index + 1;
				const uint32_t resolve_attachments_index = vector_base_index + 2;
				const uint32_t depth_attachment_index = vector_base_index + 3;
				_convert_subpass_attachments(p_create_info->pSubpasses[i].pInputAttachments, p_create_info->pSubpasses[i].inputAttachmentCount, subpasses_attachments[input_attachments_index]);
				_convert_subpass_attachments(p_create_info->pSubpasses[i].pColorAttachments, p_create_info->pSubpasses[i].colorAttachmentCount, subpasses_attachments[color_attachments_index]);
				_convert_subpass_attachments(p_create_info->pSubpasses[i].pResolveAttachments, (p_create_info->pSubpasses[i].pResolveAttachments != nullptr) ? p_create_info->pSubpasses[i].colorAttachmentCount : 0, subpasses_attachments[resolve_attachments_index]);
				_convert_subpass_attachments(p_create_info->pSubpasses[i].pDepthStencilAttachment, (p_create_info->pSubpasses[i].pDepthStencilAttachment != nullptr) ? 1 : 0, subpasses_attachments[depth_attachment_index]);

				// Ignores sType and pNext from the subpass.
				const VkSubpassDescription2& src_subpass = p_create_info->pSubpasses[i];
				VkSubpassDescription& dst_subpass = subpasses[i];
				dst_subpass.flags = src_subpass.flags;
				dst_subpass.pipelineBindPoint = src_subpass.pipelineBindPoint;
				dst_subpass.inputAttachmentCount = src_subpass.inputAttachmentCount;
				dst_subpass.pInputAttachments = subpasses_attachments[input_attachments_index].data();
				dst_subpass.colorAttachmentCount = src_subpass.colorAttachmentCount;
				dst_subpass.pColorAttachments = subpasses_attachments[color_attachments_index].data();
				dst_subpass.pResolveAttachments = subpasses_attachments[resolve_attachments_index].data();
				dst_subpass.pDepthStencilAttachment = subpasses_attachments[depth_attachment_index].data();
				dst_subpass.preserveAttachmentCount = src_subpass.preserveAttachmentCount;
				dst_subpass.pPreserveAttachments = src_subpass.pPreserveAttachments;
			}

			std::vector<VkSubpassDependency> dependencies;
			dependencies.resize(p_create_info->dependencyCount);

			for (uint32_t i = 0; i < p_create_info->dependencyCount; i++) {
				// Ignores sType and pNext from the dependency, and viewMask which is currently unused.
				const VkSubpassDependency2& src_dependency = p_create_info->pDependencies[i];
				VkSubpassDependency& dst_dependency = dependencies[i];
				dst_dependency.srcSubpass = src_dependency.srcSubpass;
				dst_dependency.dstSubpass = src_dependency.dstSubpass;
				dst_dependency.srcStageMask = src_dependency.srcStageMask;
				dst_dependency.dstStageMask = src_dependency.dstStageMask;
				dst_dependency.srcAccessMask = src_dependency.srcAccessMask;
				dst_dependency.dstAccessMask = src_dependency.dstAccessMask;
				dst_dependency.dependencyFlags = src_dependency.dependencyFlags;
			}

			VkRenderPassCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			create_info.pNext = p_create_info->pNext;
			create_info.flags = p_create_info->flags;
			create_info.attachmentCount = attachments.size();
			create_info.pAttachments = attachments.data();
			create_info.subpassCount = subpasses.size();
			create_info.pSubpasses = subpasses.data();
			create_info.dependencyCount = dependencies.size();
			create_info.pDependencies = dependencies.data();
			return vkCreateRenderPass(vk_device, &create_info, p_allocator, p_render_pass);
		}
	}

	bool RenderingDeviceDriverVulkan::_release_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain) {
		SwapChain* swap_chain = p_command_queue->image_semaphores_swap_chains[p_semaphore_index];
		if (swap_chain != nullptr) {
			// Clear the swap chain from the command queue's vector.
			p_command_queue->image_semaphores_swap_chains[p_semaphore_index] = nullptr;

			if (p_release_on_swap_chain) {
				// Remove the acquired semaphore from the swap chain's vectors.
				for (uint32_t i = 0; i < swap_chain->command_queues_acquired.size(); i++) {
					if (swap_chain->command_queues_acquired[i] == p_command_queue && swap_chain->command_queues_acquired_semaphores[i] == p_semaphore_index) {
						swap_chain->command_queues_acquired.erase(swap_chain->command_queues_acquired.begin() + i);
						swap_chain->command_queues_acquired_semaphores.erase(swap_chain->command_queues_acquired_semaphores.begin() + i);
						break;
					}
				}
			}

			return true;
		}

		return false;
	}

	bool RenderingDeviceDriverVulkan::_recreate_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain) {
		_release_image_semaphore(p_command_queue, p_semaphore_index, p_release_on_swap_chain);

		VkSemaphore semaphore;
		VkSemaphoreCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkResult err = vkCreateSemaphore(vk_device, &create_info, nullptr, &semaphore);
		ERR_FAIL_COND_V(err != VK_SUCCESS, false);

		// Indicate the semaphore is free again and destroy the previous one before storing the new one.
		vkDestroySemaphore(vk_device, p_command_queue->image_semaphores[p_semaphore_index], nullptr);

		p_command_queue->image_semaphores[p_semaphore_index] = semaphore;
		p_command_queue->free_image_semaphores.push_back(p_semaphore_index);

		return true;
	}


#pragma endregion

#pragma region Memory

	static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

	VmaPool RenderingDeviceDriverVulkan::_find_or_create_small_allocs_pool(uint32_t p_mem_type_index) {
		if (small_allocs_pools.contains(p_mem_type_index)) {
			return small_allocs_pools[p_mem_type_index];
		}

		LOGI("Creating VMA small objects pool for memory type index %d", p_mem_type_index);

		VmaPoolCreateInfo pci = {};
		pci.memoryTypeIndex = p_mem_type_index;
		pci.flags = 0;
		pci.blockSize = 0;
		pci.minBlockCount = 0;
		pci.maxBlockCount = SIZE_MAX;
		pci.priority = 0.5f;
		pci.minAllocationAlignment = 0;
		pci.pMemoryAllocateNext = nullptr;
		VmaPool pool = VK_NULL_HANDLE;
		VkResult res = vmaCreatePool(allocator, &pci, &pool);
		small_allocs_pools[p_mem_type_index] = pool; // Don't try to create it again if failed the first time.
		std::string msg = "vmaCreatePool failed with error " + std::to_string(res) + ".";
		ERR_FAIL_COND_V_MSG(res, pool, msg);

		return pool;
	}

#pragma endregion

#pragma region Buffer

	RenderingDeviceDriverVulkan::BufferID RenderingDeviceDriverVulkan::buffer_create(uint64_t p_size, BitField<RenderingDeviceDriverVulkan::BufferUsageBits> p_usage, 
		RenderingDeviceDriverVulkan::MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn) {

		uint32_t alignment = 16u; // 16 bytes is reasonable.
		if (p_usage.has_flag(BUFFER_USAGE_UNIFORM_BIT)) {
			// Some GPUs (e.g. NVIDIA) have absurdly high alignments, like 256 bytes.
			alignment = MAX(alignment, physical_device_properties.limits.minUniformBufferOffsetAlignment);
		}
		if (p_usage.has_flag(BUFFER_USAGE_STORAGE_BIT)) {
			// This shouldn't be a problem since it's often <= 16 bytes. But do it just in case.
			alignment = MAX(alignment, physical_device_properties.limits.minStorageBufferOffsetAlignment);
		}
		// Align the size. This is specially important for BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT buffers.
		// For the rest, it should work thanks to VMA taking care of the details. But still align just in case.
		p_size = STEPIFY(p_size, alignment);

		const size_t original_size = p_size;
		if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			p_size = p_size * frame_count;
		}
		VkBufferCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		create_info.size = p_size;
		create_info.usage = p_usage & ~BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaMemoryUsage vma_usage = VMA_MEMORY_USAGE_UNKNOWN;
		uint32_t vma_flags_to_remove = 0;

		VmaAllocationCreateInfo alloc_create_info = {};
		switch (p_allocation_type) {
		case MEMORY_ALLOCATION_TYPE_CPU: {
			bool is_src = p_usage.has_flag(BUFFER_USAGE_TRANSFER_FROM_BIT);
			bool is_dst = p_usage.has_flag(BUFFER_USAGE_TRANSFER_TO_BIT);
			if (is_src && !is_dst) {
				// Looks like a staging buffer: CPU maps, writes sequentially, then GPU copies to VRAM.
				alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
				alloc_create_info.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				vma_flags_to_remove |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
			if (is_dst && !is_src) {
				// Looks like a readback buffer: GPU copies from VRAM, then CPU maps and reads.
				alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
				alloc_create_info.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
				vma_flags_to_remove |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			}
			vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			alloc_create_info.requiredFlags = (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		} break;
		case MEMORY_ALLOCATION_TYPE_GPU: {
			vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			//if (!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()) {
			//	// We must set it right now or else vmaFindMemoryTypeIndexForBufferInfo will use wrong parameters.
			//	alloc_create_info.usage = vma_usage;
			//}
			if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
				alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			}
			alloc_create_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			if (p_size <= SMALL_ALLOCATION_MAX_SIZE) {
				uint32_t mem_type_index = 0;
				vmaFindMemoryTypeIndexForBufferInfo(allocator, &create_info, &alloc_create_info, &mem_type_index);
				alloc_create_info.pool = _find_or_create_small_allocs_pool(mem_type_index);
			}
		} break;
		}

		VkBuffer vk_buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		VmaAllocationInfo alloc_info = {};

		if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
			alloc_create_info.preferredFlags &= ~vma_flags_to_remove;
			alloc_create_info.usage = vma_usage;

			VkResult err = vmaCreateBuffer(allocator, &create_info, &alloc_create_info, &vk_buffer, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format( "Can't create buffer of size: {} , error {}", std::to_string(p_size), std::to_string(err)));
		}
		else {
			/*static uint32_t num = 1;
			LOGI("buffer alloc: %d", num++);*/
			VkResult err = vkCreateBuffer(vk_device, &create_info, nullptr, &vk_buffer);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't create buffer of size: {} , error {}", std::to_string(p_size), std::to_string(err)));
			err = vmaAllocateMemoryForBuffer(allocator, vk_buffer, &alloc_create_info, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't allocate memory for buffer of size: {}, error {} .", std::to_string(p_size), std::to_string(err)));
			err = vmaBindBufferMemory2(allocator, allocation, 0, vk_buffer, nullptr);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't bind memory to buffer of size: {}, error {} .", std::to_string(p_size), std::to_string(err)));
		}

		// Bookkeep.
		BufferInfo* buf_info;
		if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			void* persistent_ptr = nullptr;
			VkResult err = vmaMapMemory(allocator, allocation, &persistent_ptr);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("vmaMapMemory failed with error {} .", std::to_string(err)));

			BufferDynamicInfo* dyn_buffer = new BufferDynamicInfo;		// TODO: dealloc
			buf_info = dyn_buffer;
#ifdef DEBUG_ENABLED
			dyn_buffer->last_frame_mapped = p_frames_drawn - 1ul;
#endif
			dyn_buffer->frame_idx = 0u;
			dyn_buffer->persistent_ptr = (uint8_t*)persistent_ptr;
		}
		else {
			buf_info = new BufferInfo; //TODO: dealloc? // VersatileResource::allocate<BufferInfo>(resources_allocator);
		}
		buf_info->vk_buffer = vk_buffer;
		buf_info->allocation.handle = allocation;
		buf_info->allocation.size = alloc_info.size;
		buf_info->size = original_size;

		return BufferID(buf_info);
	}

	void RenderingDeviceDriverVulkan::buffer_free(BufferID p_buffer) {
		BufferInfo* buf_info = (BufferInfo*)p_buffer.id;
		if (buf_info->vk_view) {
			vkDestroyBufferView(vk_device, buf_info->vk_view, nullptr);
		}

		if (buf_info->is_dynamic()) {
			vmaUnmapMemory(allocator, buf_info->allocation.handle);
		}

		if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
			vmaDestroyBuffer(allocator, buf_info->vk_buffer, buf_info->allocation.handle);
		}
		else {
			/*static uint32_t de_num = 1;
			LOGI("buffer de alloc: %d", de_num++);*/
			vkDestroyBuffer(vk_device, buf_info->vk_buffer, nullptr);
			vmaFreeMemory(allocator, buf_info->allocation.handle);
		}

		if (buf_info->is_dynamic()) {
			delete (BufferDynamicInfo*)buf_info;
		}
		else {
			delete buf_info;
		}
	}

	bool RenderingDeviceDriverVulkan::buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) {
		BufferInfo* buf_info = (BufferInfo*)p_buffer.id;

		DEV_ASSERT(!buf_info->vk_view);

		VkBufferViewCreateInfo view_create_info = {};
		view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view_create_info.buffer = buf_info->vk_buffer;
		view_create_info.format = RD_TO_VK_FORMAT[p_format];
		view_create_info.range = buf_info->allocation.size;

		VkResult res = vkCreateBufferView(vk_device, &view_create_info, nullptr, &buf_info->vk_view);
		ERR_FAIL_COND_V_MSG(res, false, std::format("Unable to create buffer view, error {} .", std::to_string(res)));

		return true;
	}

	uint64_t RenderingDeviceDriverVulkan::buffer_get_allocation_size(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		return buf_info->allocation.size;
	}

	uint8_t* RenderingDeviceDriverVulkan::buffer_map(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		ERR_FAIL_COND_V_MSG(buf_info->is_dynamic(), nullptr, "Buffer must NOT have BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT. Use buffer_persistent_map_advance() instead.");
		void* data_ptr = nullptr;
		VkResult err = vmaMapMemory(allocator, buf_info->allocation.handle, &data_ptr);
		ERR_FAIL_COND_V_MSG(err, nullptr, std::format("vmaMapMemory failed with error {} .", std::to_string(err)));
		return (uint8_t*)data_ptr;
	}

	void RenderingDeviceDriverVulkan::buffer_unmap(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		vmaUnmapMemory(allocator, buf_info->allocation.handle);
	}

	uint8_t* RenderingDeviceDriverVulkan::buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) {
		BufferDynamicInfo* buf_info = (BufferDynamicInfo*)p_buffer.id;
		ERR_FAIL_COND_V_MSG(!buf_info->is_dynamic(), nullptr, "Buffer must have BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT. Use buffer_map() instead.");
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(buf_info->last_frame_mapped == p_frames_drawn, nullptr, "Buffers with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT must only be mapped once per frame. Otherwise there could be race conditions with the GPU. Amalgamate all data uploading into one map(), use an extra buffer or remove the bit.");
		buf_info->last_frame_mapped = p_frames_drawn;
#endif
		buf_info->frame_idx = (buf_info->frame_idx + 1u) % frame_count;
		return buf_info->persistent_ptr + buf_info->frame_idx * buf_info->size;
	}

	uint64_t RenderingDeviceDriverVulkan::buffer_get_dynamic_offsets(std::span<BufferID> p_buffers) {
		uint64_t mask = 0u;
		uint64_t shift = 0u;

		for (const BufferID& buf : p_buffers) {
			const BufferInfo* buf_info = (const BufferInfo*)buf.id;
			if (!buf_info->is_dynamic()) {
				continue;
			}
			mask |= buf_info->frame_idx << shift;
			// We can encode the frame index in 2 bits since frame_count won't be > 4.
			shift += 2UL;
		}

		return mask;
	}

	void RenderingDeviceDriverVulkan::buffer_flush(BufferID p_buffer) {
		BufferDynamicInfo* buf_info = (BufferDynamicInfo*)p_buffer.id;

		VkMemoryPropertyFlags mem_props_flags;
		vmaGetAllocationMemoryProperties(allocator, buf_info->allocation.handle, &mem_props_flags);

		const bool needs_flushing = !(mem_props_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (needs_flushing) {
			if (buf_info->is_dynamic()) {
				pending_flushes.allocations.push_back(buf_info->allocation.handle);
				pending_flushes.offsets.push_back(buf_info->frame_idx * buf_info->size);
				pending_flushes.sizes.push_back(buf_info->size);
			}
			else {
				pending_flushes.allocations.push_back(buf_info->allocation.handle);
				pending_flushes.offsets.push_back(0u);
				pending_flushes.sizes.push_back(VK_WHOLE_SIZE);
			}
		}
	}

	uint64_t RenderingDeviceDriverVulkan::buffer_get_device_address(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		VkBufferDeviceAddressInfo address_info = {};
		address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		address_info.pNext = nullptr;
		address_info.buffer = buf_info->vk_buffer;
		return vkGetBufferDeviceAddress(vk_device, &address_info);
	}

#pragma endregion

#pragma region Texture

	static const VkImageType RD_TEX_TYPE_TO_VK_IMG_TYPE[RenderingDeviceCommons::TEXTURE_TYPE_MAX] = {
	VK_IMAGE_TYPE_1D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_3D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_1D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_2D,
	};

	static const VkSampleCountFlagBits RD_TO_VK_SAMPLE_COUNT[RenderingDeviceCommons::TEXTURE_SAMPLES_MAX] = {
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT,
	};

	VkSampleCountFlagBits RenderingDeviceDriverVulkan::_ensure_supported_sample_count(TextureSamples p_requested_sample_count) {
		VkSampleCountFlags sample_count_flags = (physical_device_properties.limits.framebufferColorSampleCounts & physical_device_properties.limits.framebufferDepthSampleCounts);

		if ((sample_count_flags & RD_TO_VK_SAMPLE_COUNT[p_requested_sample_count])) {
			// The requested sample count is supported.
			return RD_TO_VK_SAMPLE_COUNT[p_requested_sample_count];
		}
		else {
			// Find the closest lower supported sample count.
			VkSampleCountFlagBits sample_count = RD_TO_VK_SAMPLE_COUNT[p_requested_sample_count];
			while (sample_count > VK_SAMPLE_COUNT_1_BIT) {
				if (sample_count_flags & sample_count) {
					return sample_count;
				}
				sample_count = (VkSampleCountFlagBits)(sample_count >> 1);
			}
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	RenderingDeviceDriverVulkan::TextureID RenderingDeviceDriverVulkan::texture_create(const TextureFormat& p_format, const TextureView& p_view) {
		VkImageCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

		if (p_format.shareable_formats.size()) {
			create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

			if (enabled_device_extension_names.contains(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME)) {
				std::vector<VkFormat> vk_allowed_formats_array(p_format.shareable_formats.size());
				VkFormat* vk_allowed_formats = vk_allowed_formats_array.data();
				for (int i = 0; i < p_format.shareable_formats.size(); i++) {
					vk_allowed_formats[i] = RD_TO_VK_FORMAT[p_format.shareable_formats[i]];
				}

				VkImageFormatListCreateInfoKHR* format_list_create_info = new VkImageFormatListCreateInfoKHR;
				*format_list_create_info = {};
				format_list_create_info->sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
				format_list_create_info->viewFormatCount = p_format.shareable_formats.size();
				format_list_create_info->pViewFormats = vk_allowed_formats;

				create_info.pNext = format_list_create_info;
			}
		}

		if (p_format.texture_type == TEXTURE_TYPE_CUBE || p_format.texture_type == TEXTURE_TYPE_CUBE_ARRAY) {
			create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}
		/*if (p_format.texture_type == TEXTURE_TYPE_2D || p_format.texture_type == TEXTURE_TYPE_2D_ARRAY) {
			create_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
		}*/

		//if (fdm_capabilities.offset_supported && (p_format.usage_bits & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_VRS_ATTACHMENT_BIT))) {
		//	create_info.flags |= VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM;
		//}

		create_info.imageType = RD_TEX_TYPE_TO_VK_IMG_TYPE[p_format.texture_type];

		create_info.format = RD_TO_VK_FORMAT[p_format.format];

		create_info.extent.width = p_format.width;
		create_info.extent.height = p_format.height;
		create_info.extent.depth = p_format.depth;

		create_info.mipLevels = p_format.mipmaps;
		create_info.arrayLayers = p_format.array_layers;

		create_info.samples = _ensure_supported_sample_count(p_format.samples);
		create_info.tiling = (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

		// Usage.
		if ((p_format.usage_bits & TEXTURE_USAGE_SAMPLING_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_STORAGE_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if ((p_format.usage_bits & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT))) {
			create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && (p_format.usage_bits & TEXTURE_USAGE_VRS_FRAGMENT_SHADING_RATE_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && (p_format.usage_bits & TEXTURE_USAGE_VRS_FRAGMENT_DENSITY_MAP_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_CAN_UPDATE_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_CAN_COPY_FROM_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if ((p_format.usage_bits & TEXTURE_USAGE_CAN_COPY_TO_BIT)) {
			create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// Allocate memory.

		uint32_t width = 0, height = 0;
		uint32_t image_size = get_image_format_required_size(p_format.format, p_format.width, p_format.height, p_format.depth, p_format.mipmaps, &width, &height);

		VmaAllocationCreateInfo alloc_create_info = {};
		alloc_create_info.flags = (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0;

		if (p_format.usage_bits & TEXTURE_USAGE_TRANSIENT_BIT) {
			uint32_t memory_type_index = 0;
			VmaAllocationCreateInfo lazy_memory_requirements = alloc_create_info;
			lazy_memory_requirements.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
			VkResult result = vmaFindMemoryTypeIndex(allocator, UINT32_MAX, &lazy_memory_requirements, &memory_type_index);
			if (VK_SUCCESS == result) {
				alloc_create_info = lazy_memory_requirements;
				create_info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
				// VUID-VkImageCreateInfo-usage-00963 :
				// If usage includes VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
				// then bits other than VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				// and VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT must not be set.
				create_info.usage &= (VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
			}
			else {
				alloc_create_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
		}
		else if (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) {
			alloc_create_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}
		else {
			alloc_create_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}

		if (image_size <= SMALL_ALLOCATION_MAX_SIZE) {
			uint32_t mem_type_index = 0;
			vmaFindMemoryTypeIndexForImageInfo(allocator, &create_info, &alloc_create_info, &mem_type_index);
			alloc_create_info.pool = _find_or_create_small_allocs_pool(mem_type_index);
		}

		// Create.

		VkImage vk_image = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		VmaAllocationInfo alloc_info = {};

		if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
			alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			VkResult err = vmaCreateImage(allocator, &create_info, &alloc_create_info, &vk_image, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vmaCreateImage failed with error {} .", std::to_string(err)));
		}
		else {
			//static uint32_t num = 1;
			//LOGI("buffer alloc: %d", num++);
			VkResult err = vkCreateImage(vk_device, &create_info, nullptr, &vk_image);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImage failed with error {}", std::to_string(err)));
			err = vmaAllocateMemoryForImage(allocator, vk_image, &alloc_create_info, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("Can't allocate memory for image, error: {}", std::to_string(err)));
			err = vmaBindImageMemory2(allocator, allocation, 0, vk_image, nullptr);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("Can't bind memory to image, error: {}", std::to_string(err)));
		}

		// Create view.

		VkImageViewCreateInfo image_view_create_info = {};
		image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.image = vk_image;
		image_view_create_info.viewType = (VkImageViewType)p_format.texture_type;
		image_view_create_info.format = RD_TO_VK_FORMAT[p_view.format];
		image_view_create_info.components.r = (VkComponentSwizzle)p_view.swizzle_r;
		image_view_create_info.components.g = (VkComponentSwizzle)p_view.swizzle_g;
		image_view_create_info.components.b = (VkComponentSwizzle)p_view.swizzle_b;
		image_view_create_info.components.a = (VkComponentSwizzle)p_view.swizzle_a;
		image_view_create_info.subresourceRange.levelCount = create_info.mipLevels;
		image_view_create_info.subresourceRange.layerCount = create_info.arrayLayers;
		if ((p_format.usage_bits & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT))) {
			image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else {
			image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		VkImageViewASTCDecodeModeEXT decode_mode;
		if (enabled_device_extension_names.contains(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME)) {
			if (image_view_create_info.format >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK && image_view_create_info.format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK) {
				decode_mode.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT;
				decode_mode.pNext = nullptr;
				decode_mode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
				image_view_create_info.pNext = &decode_mode;
			}
		}

		VkImageView vk_image_view = VK_NULL_HANDLE;
		VkResult err = vkCreateImageView(vk_device, &image_view_create_info, nullptr, &vk_image_view);
		if (err) {
			if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
				vmaDestroyImage(allocator, vk_image, allocation);
			}
			else {
				//static uint32_t de_num = 1;
				//LOGI("de_num buffer alloc: %d", de_num++);
				vkDestroyImage(vk_device, vk_image, nullptr);
				vmaFreeMemory(allocator, allocation);
			}

			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error {} .", std::to_string(err)));
		}

		// Bookkeep.

		TextureInfo* tex_info = new TextureInfo;
		tex_info->vk_image = vk_image;
		tex_info->vk_view = vk_image_view;
		tex_info->rd_format = p_format.format;
		tex_info->vk_create_info = create_info;
		tex_info->vk_view_create_info = image_view_create_info;
		tex_info->allocation.handle = allocation;
#ifdef DEBUG_ENABLED
		tex_info->transient = (p_format.usage_bits & TEXTURE_USAGE_TRANSIENT_BIT) != 0;
#endif
		vmaGetAllocationInfo(allocator, tex_info->allocation.handle, &tex_info->allocation.info);

#if PRINT_NATIVE_COMMANDS
		LOGI(vformat("vkCreateImageView: 0x%uX for 0x%uX", uint64_t(vk_image_view), uint64_t(vk_image)));
#endif

		return TextureID(tex_info);
	}

	RenderingDeviceDriverVulkan::TextureID RenderingDeviceDriverVulkan::texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps) {
		VkImage vk_image = (VkImage)p_native_texture;

		// We only need to create a view into the already existing natively-provided texture.

		VkImageViewCreateInfo image_view_create_info = {};
		image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.image = vk_image;
		image_view_create_info.viewType = (VkImageViewType)p_type;
		image_view_create_info.format = RD_TO_VK_FORMAT[p_format];
		image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
		image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
		image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
		image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
		image_view_create_info.subresourceRange.baseMipLevel = 0;
		image_view_create_info.subresourceRange.levelCount = p_mipmaps;
		image_view_create_info.subresourceRange.layerCount = p_array_layers;
		image_view_create_info.subresourceRange.aspectMask = p_depth_stencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageView vk_image_view = VK_NULL_HANDLE;
		VkResult err = vkCreateImageView(vk_device, &image_view_create_info, nullptr, &vk_image_view);
		if (err) {
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error {} .",std::to_string(err)));
		}

		// Bookkeep.

		TextureInfo* tex_info = new TextureInfo;
		tex_info->vk_view = vk_image_view;
		tex_info->rd_format = p_format;
		tex_info->vk_view_create_info = image_view_create_info;
#ifdef DEBUG_ENABLED
		tex_info->created_from_extension = true;
#endif
		return TextureID(tex_info);
	}

	RenderingDeviceDriverVulkan::TextureID RenderingDeviceDriverVulkan::texture_create_shared(TextureID p_original_texture, const TextureView& p_view) {
		const TextureInfo* owner_tex_info = (const TextureInfo*)p_original_texture.id;
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(!owner_tex_info->allocation.handle && !owner_tex_info->created_from_extension, TextureID());
#endif
		VkImageViewCreateInfo image_view_create_info = owner_tex_info->vk_view_create_info;
		image_view_create_info.format = RD_TO_VK_FORMAT[p_view.format];
		image_view_create_info.components.r = (VkComponentSwizzle)p_view.swizzle_r;
		image_view_create_info.components.g = (VkComponentSwizzle)p_view.swizzle_g;
		image_view_create_info.components.b = (VkComponentSwizzle)p_view.swizzle_b;
		image_view_create_info.components.a = (VkComponentSwizzle)p_view.swizzle_a;

		if (enabled_device_extension_names.contains(VK_KHR_MAINTENANCE_2_EXTENSION_NAME)) {
			// May need to make VK_KHR_maintenance2 mandatory and thus has Vulkan 1.1 be our minimum supported version
			// if we require setting this information. Vulkan 1.0 may simply not care.
			if (image_view_create_info.format != owner_tex_info->vk_view_create_info.format) {
				VkImageViewUsageCreateInfo* usage_info = new VkImageViewUsageCreateInfo;
				*usage_info = {};
				usage_info->sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
				usage_info->usage = owner_tex_info->vk_create_info.usage;

				// Certain features may not be available for the format of the view.
				{
					VkFormatProperties properties = {};
					vkGetPhysicalDeviceFormatProperties(physical_device, RD_TO_VK_FORMAT[p_view.format], &properties);
					const VkFormatFeatureFlags& supported_flags = owner_tex_info->vk_create_info.tiling == VK_IMAGE_TILING_LINEAR ? properties.linearTilingFeatures : properties.optimalTilingFeatures;
					if ((usage_info->usage & VK_IMAGE_USAGE_STORAGE_BIT) && !(supported_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
						usage_info->usage &= ~uint32_t(VK_IMAGE_USAGE_STORAGE_BIT);
					}
					if ((usage_info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) && !(supported_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
						usage_info->usage &= ~uint32_t(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
					}
				}

				image_view_create_info.pNext = usage_info;
			}
		}

		VkImageView new_vk_image_view = VK_NULL_HANDLE;
		VkResult err = vkCreateImageView(vk_device, &image_view_create_info, nullptr, &new_vk_image_view);
		ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error {} .", std::to_string(err)));

		// Bookkeep.

		TextureInfo* tex_info = new TextureInfo;
		*tex_info = *owner_tex_info;
		tex_info->vk_view = new_vk_image_view;
		tex_info->vk_view_create_info = image_view_create_info;
		tex_info->allocation = {};

#if PRINT_NATIVE_COMMANDS
		LOGI(std::format("vkCreateImageView: 0x%uX for 0x%uX", uint64_t(new_vk_image_view), uint64_t(owner_tex_info->vk_view_create_info.image)));
#endif

		return TextureID(tex_info);
	}

	RenderingDeviceDriverVulkan::TextureID RenderingDeviceDriverVulkan::texture_create_shared_from_slice(TextureID p_original_texture, const TextureView& p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
		const TextureInfo* owner_tex_info = (const TextureInfo*)p_original_texture.id;
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(!owner_tex_info->allocation.handle && !owner_tex_info->created_from_extension, TextureID());
#endif

		VkImageViewCreateInfo image_view_create_info = owner_tex_info->vk_view_create_info;
		switch (p_slice_type) {
		case TEXTURE_SLICE_2D: {
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		} break;
		case TEXTURE_SLICE_3D: {
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
		} break;
		case TEXTURE_SLICE_CUBEMAP: {
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		} break;
		case TEXTURE_SLICE_2D_ARRAY: {
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		} break;
		default: {
			return TextureID(nullptr);
		}
		}
		image_view_create_info.format = RD_TO_VK_FORMAT[p_view.format];
		image_view_create_info.components.r = (VkComponentSwizzle)p_view.swizzle_r;
		image_view_create_info.components.g = (VkComponentSwizzle)p_view.swizzle_g;
		image_view_create_info.components.b = (VkComponentSwizzle)p_view.swizzle_b;
		image_view_create_info.components.a = (VkComponentSwizzle)p_view.swizzle_a;
		image_view_create_info.subresourceRange.baseMipLevel = p_mipmap;
		image_view_create_info.subresourceRange.levelCount = p_mipmaps;
		image_view_create_info.subresourceRange.baseArrayLayer = p_layer;
		image_view_create_info.subresourceRange.layerCount = p_layers;

		VkImageView new_vk_image_view = VK_NULL_HANDLE;
		VkResult err = vkCreateImageView(vk_device, &image_view_create_info, nullptr, &new_vk_image_view);
		ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error {} .", std::to_string(err)));

		// Bookkeep.

		TextureInfo* tex_info = new TextureInfo;
		*tex_info = *owner_tex_info;
		tex_info->vk_view = new_vk_image_view;
		tex_info->vk_view_create_info = image_view_create_info;
		tex_info->allocation = {};

#if PRINT_NATIVE_COMMANDS
		LOGI(std::format("vkCreateImageView: {} for {} ({} {} {} {})", uint64_t(new_vk_image_view), uint64_t(owner_tex_info->vk_view_create_info.image), p_mipmap, p_mipmaps, p_layer, p_layers));
#endif

		return TextureID(tex_info);
	}

	void RenderingDeviceDriverVulkan::texture_free(TextureID p_texture) {
		TextureInfo* tex_info = (TextureInfo*)p_texture.id;
		vkDestroyImageView(vk_device, tex_info->vk_view, nullptr);
		if (tex_info->allocation.handle) {
			if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
				vmaDestroyImage(allocator, tex_info->vk_view_create_info.image, tex_info->allocation.handle);
			}
			else {
				//static uint32_t de_num_tex = 1;
				//LOGI("de_num_tex buffer alloc: %d", de_num_tex++);
				vkDestroyImage(vk_device, tex_info->vk_image, nullptr);
				vmaFreeMemory(allocator, tex_info->allocation.handle);
			}
		}
		delete tex_info;
	}

	uint64_t RenderingDeviceDriverVulkan::texture_get_allocation_size(TextureID p_texture) {
		const TextureInfo* tex_info = (const TextureInfo*)p_texture.id;
		return tex_info->allocation.info.size;
	}

	void RenderingDeviceDriverVulkan::texture_get_copyable_layout(TextureID p_texture, const TextureSubresource& p_subresource, TextureCopyableLayout* r_layout) {
		const TextureInfo* tex_info = (const TextureInfo*)p_texture.id;

		uint32_t w = MAX(1u, tex_info->vk_create_info.extent.width >> p_subresource.mipmap);
		uint32_t h = MAX(1u, tex_info->vk_create_info.extent.height >> p_subresource.mipmap);
		uint32_t d = MAX(1u, tex_info->vk_create_info.extent.depth >> p_subresource.mipmap);

		uint32_t bw = 0, bh = 0;
		get_compressed_image_format_block_dimensions(tex_info->rd_format, bw, bh);

		uint32_t sbw = 0, sbh = 0;
		*r_layout = {};
		r_layout->size = get_image_format_required_size(tex_info->rd_format, w, h, d, 1, &sbw, &sbh);
		r_layout->row_pitch = r_layout->size / ((sbh / bh) * d);
	}

	std::vector<uint8_t> RenderingDeviceDriverVulkan::texture_get_data(TextureID p_texture, uint32_t p_layer) {
		const TextureInfo* tex = (const TextureInfo*)p_texture.id;

		DataFormat tex_format = tex->rd_format;
		uint32_t tex_width = tex->vk_create_info.extent.width;
		uint32_t tex_height = tex->vk_create_info.extent.height;
		uint32_t tex_depth = tex->vk_create_info.extent.depth;
		uint32_t tex_mipmaps = tex->vk_create_info.mipLevels;

		uint32_t width, height, depth;
		uint32_t tight_mip_size = get_image_format_required_size(tex_format, tex_width, tex_height, tex_depth, tex_mipmaps, &width, &height, &depth);

		std::vector<uint8_t> image_data;
		image_data.resize(tight_mip_size);

		uint32_t blockw, blockh;
		get_compressed_image_format_block_dimensions(tex_format, blockw, blockh);
		uint32_t block_size = get_compressed_image_format_block_byte_size(tex_format);
		uint32_t pixel_size = get_image_format_pixel_size(tex_format);

		void* data_ptr = nullptr;
		VkResult err = vmaMapMemory(allocator, tex->allocation.handle, &data_ptr);
		ERR_FAIL_COND_V_MSG(err, std::vector<uint8_t>(), std::format("vmaMapMemory failed with error {} .", std::to_string(err)));

		{
			uint8_t* w = image_data.data();

			uint32_t mipmap_offset = 0;
			for (uint32_t mm_i = 0; mm_i < tex_mipmaps; mm_i++) {
				uint32_t image_total = get_image_format_required_size(tex_format, tex_width, tex_height, tex_depth, mm_i + 1, &width, &height, &depth);

				uint8_t* write_ptr_mipmap = w + mipmap_offset;
				tight_mip_size = image_total - mipmap_offset;

				VkImageSubresource vk_subres = {};
				vk_subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vk_subres.arrayLayer = p_layer;
				vk_subres.mipLevel = mm_i;

				VkSubresourceLayout vk_layout = {};
				vkGetImageSubresourceLayout(vk_device, tex->vk_view_create_info.image, &vk_subres, &vk_layout);

				for (uint32_t z = 0; z < depth; z++) {
					uint8_t* write_ptr = write_ptr_mipmap + z * tight_mip_size / depth;
					const uint8_t* slice_read_ptr = (uint8_t*)data_ptr + vk_layout.offset + z * vk_layout.depthPitch;

					if (block_size > 1) {
						// Compressed.
						uint32_t line_width = (block_size * (width / blockw));
						for (uint32_t y = 0; y < height / blockh; y++) {
							const uint8_t* rptr = slice_read_ptr + y * vk_layout.rowPitch;
							uint8_t* wptr = write_ptr + y * line_width;

							memcpy(wptr, rptr, line_width);
						}
					}
					else {
						// Uncompressed.
						for (uint32_t y = 0; y < height; y++) {
							const uint8_t* rptr = slice_read_ptr + y * vk_layout.rowPitch;
							uint8_t* wptr = write_ptr + y * pixel_size * width;
							memcpy(wptr, rptr, (uint64_t)pixel_size * width);
						}
					}
				}

				mipmap_offset = image_total;
			}
		}

		vmaUnmapMemory(allocator, tex->allocation.handle);

		return image_data;
	}

	BitField<RenderingDeviceCommons::TextureUsageBits> RenderingDeviceDriverVulkan::texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) {
		if (p_format >= DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK && p_format <= DATA_FORMAT_ASTC_12x12_SFLOAT_BLOCK && !enabled_device_extension_names.contains(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME)) {
			// Formats that were introduced later with extensions must not reach vkGetPhysicalDeviceFormatProperties if the extension isn't available. This means it's not supported.
			return 0;
		}
		VkFormatProperties properties = {};
		vkGetPhysicalDeviceFormatProperties(physical_device, RD_TO_VK_FORMAT[p_format], &properties);

		const VkFormatFeatureFlags& flags = p_cpu_readable ? properties.linearTilingFeatures : properties.optimalTilingFeatures;

		// Everything supported by default makes an all-or-nothing check easier for the caller.
		BitField<TextureUsageBits> supported = INT64_MAX;

		if (!(flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
			supported.clear_flag(TEXTURE_USAGE_SAMPLING_BIT);
		}
		if (!(flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
			supported.clear_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
		}
		if (!(flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
			supported.clear_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
			supported.clear_flag(TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT);
		}
		if (!(flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
			supported.clear_flag(TEXTURE_USAGE_STORAGE_BIT);
		}
		if (!(flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)) {
			supported.clear_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
		}
		if (p_format != DATA_FORMAT_R8_UINT && p_format != DATA_FORMAT_R8G8_UNORM) {
			supported.clear_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT);
		}

		return supported;
	}

	bool RenderingDeviceDriverVulkan::texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool& r_raw_reinterpretation) {
		r_raw_reinterpretation = false;
		return true;
	}

#pragma endregion

#pragma region Sampler

	RenderingDeviceDriverVulkan::SamplerID RenderingDeviceDriverVulkan::sampler_create(const SamplerState& p_state) {
		VkSamplerCreateInfo sampler_create_info = {};
		sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_create_info.pNext = nullptr;
		sampler_create_info.flags = 0;
		sampler_create_info.magFilter = p_state.mag_filter == SAMPLER_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		sampler_create_info.minFilter = p_state.min_filter == SAMPLER_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		sampler_create_info.mipmapMode = p_state.mip_filter == SAMPLER_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_create_info.addressModeU = (VkSamplerAddressMode)p_state.repeat_u;
		sampler_create_info.addressModeV = (VkSamplerAddressMode)p_state.repeat_v;
		sampler_create_info.addressModeW = (VkSamplerAddressMode)p_state.repeat_w;
		sampler_create_info.mipLodBias = p_state.lod_bias;
		sampler_create_info.anisotropyEnable = p_state.use_anisotropy && (physical_device_features.samplerAnisotropy == VK_TRUE);
		sampler_create_info.maxAnisotropy = p_state.anisotropy_max;
		sampler_create_info.compareEnable = p_state.enable_compare;
		sampler_create_info.compareOp = (VkCompareOp)p_state.compare_op;
		sampler_create_info.minLod = p_state.min_lod;
		sampler_create_info.maxLod = p_state.max_lod;
		sampler_create_info.borderColor = (VkBorderColor)p_state.border_color;
		sampler_create_info.unnormalizedCoordinates = p_state.unnormalized_uvw;

		VkSampler vk_sampler = VK_NULL_HANDLE;
		VkResult res = vkCreateSampler(vk_device, &sampler_create_info, nullptr, &vk_sampler);
		ERR_FAIL_COND_V_MSG(res, SamplerID(), std::format("vkCreateSampler failed with error {} .", std::to_string(res)));

		return SamplerID(vk_sampler);
	}

	void RenderingDeviceDriverVulkan::sampler_free(SamplerID p_sampler) {
		vkDestroySampler(vk_device, (VkSampler)p_sampler.id, nullptr);
	}

	bool RenderingDeviceDriverVulkan::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) {
		switch (p_filter) {
		case SAMPLER_FILTER_NEAREST: {
			return true;
		}
		case SAMPLER_FILTER_LINEAR: {
			VkFormatProperties properties = {};
			vkGetPhysicalDeviceFormatProperties(physical_device, RD_TO_VK_FORMAT[p_format], &properties);
			return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
		}
		}
		return false;
	}

#pragma endregion

#pragma region Vertex Arrays

	RenderingDeviceDriverVulkan::VertexFormatID RenderingDeviceDriverVulkan::vertex_format_create(std::span<VertexAttribute> p_vertex_attribs, const VertexAttributeBindingsMap& p_vertex_bindings) {
		// Pre-bookkeep.
		VertexFormatInfo* vf_info = new VertexFormatInfo;

		vf_info->vk_bindings.reserve(p_vertex_bindings.size());
		for (const auto& E : p_vertex_bindings) {
			const VertexAttributeBinding& binding = E.second;
			VkVertexInputBindingDescription vk_binding = {};
			vk_binding.binding = E.first;
			vk_binding.stride = binding.stride;
			vk_binding.inputRate = binding.frequency == VERTEX_FREQUENCY_INSTANCE ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
			vf_info->vk_bindings.push_back(vk_binding);
		}
		vf_info->vk_attributes.resize(p_vertex_attribs.size());
		for (uint32_t i = 0; i < p_vertex_attribs.size(); i++) {
			vf_info->vk_attributes[i] = {};
			vf_info->vk_attributes[i].binding = p_vertex_attribs[i].binding;
			vf_info->vk_attributes[i].location = p_vertex_attribs[i].location;
			vf_info->vk_attributes[i].format = RD_TO_VK_FORMAT[p_vertex_attribs[i].format];
			vf_info->vk_attributes[i].offset = p_vertex_attribs[i].offset;
		}

		vf_info->vk_create_info = {};
		vf_info->vk_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vf_info->vk_create_info.vertexBindingDescriptionCount = vf_info->vk_bindings.size();
		vf_info->vk_create_info.pVertexBindingDescriptions = vf_info->vk_bindings.data();
		vf_info->vk_create_info.vertexAttributeDescriptionCount = vf_info->vk_attributes.size();
		vf_info->vk_create_info.pVertexAttributeDescriptions = vf_info->vk_attributes.data();

		return VertexFormatID(vf_info);
	}

	void RenderingDeviceDriverVulkan::vertex_format_free(VertexFormatID p_vertex_format) {
		VertexFormatInfo* vf_info = (VertexFormatInfo*)p_vertex_format.id;
		delete vf_info;
	}

#pragma endregion

#pragma region Barriers

	void RenderingDeviceDriverVulkan::command_pipeline_barrier(
		CommandBufferID p_cmd_buffer,
		BitField<PipelineStageBits> p_src_stages,
		BitField<PipelineStageBits> p_dst_stages,
		std::span<MemoryAccessBarrier> p_memory_barriers,
		std::span<BufferBarrier> p_buffer_barriers,
		std::span<TextureBarrier> p_texture_barriers,
		std::span<AccelerationStructureBarrier> p_acceleration_structure_barriers) {

		std::vector<VkMemoryBarrier> vk_mem_barrier_vec(p_memory_barriers.size());
		VkMemoryBarrier* vk_memory_barriers = vk_mem_barrier_vec.data();
		for (uint32_t i = 0; i < p_memory_barriers.size(); i++) {
			vk_memory_barriers[i] = {};
			vk_memory_barriers[i].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			vk_memory_barriers[i].srcAccessMask = _rd_to_vk_access_flags(p_memory_barriers[i].src_access) & ~VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vk_memory_barriers[i].dstAccessMask = _rd_to_vk_access_flags(p_memory_barriers[i].dst_access) & ~VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		}
		std::vector<VkBufferMemoryBarrier> vk_buffer_vec(p_buffer_barriers.size());
		VkBufferMemoryBarrier* vk_buffer_barriers = vk_buffer_vec.data();
		for (uint32_t i = 0; i < p_buffer_barriers.size(); i++) {
			vk_buffer_barriers[i] = {};
			vk_buffer_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			vk_buffer_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vk_buffer_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vk_buffer_barriers[i].srcAccessMask = _rd_to_vk_access_flags(p_buffer_barriers[i].src_access) & ~VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vk_buffer_barriers[i].dstAccessMask = _rd_to_vk_access_flags(p_buffer_barriers[i].dst_access) & ~VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vk_buffer_barriers[i].buffer = ((const BufferInfo*)p_buffer_barriers[i].buffer.id)->vk_buffer;
			vk_buffer_barriers[i].offset = p_buffer_barriers[i].offset;
			vk_buffer_barriers[i].size = p_buffer_barriers[i].size;
		}

		std::vector<VkImageMemoryBarrier> vk_image_barrier_vec(p_texture_barriers.size());

		VkImageMemoryBarrier* vk_image_barriers = vk_image_barrier_vec.data();
		for (uint32_t i = 0; i < p_texture_barriers.size(); i++) {
			const TextureInfo* tex_info = (const TextureInfo*)p_texture_barriers[i].texture.id;
			vk_image_barriers[i] = {};
			vk_image_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			vk_image_barriers[i].srcAccessMask = _rd_to_vk_access_flags(p_texture_barriers[i].src_access);
			vk_image_barriers[i].dstAccessMask = _rd_to_vk_access_flags(p_texture_barriers[i].dst_access);
			vk_image_barriers[i].oldLayout = RD_TO_VK_LAYOUT[p_texture_barriers[i].prev_layout];
			vk_image_barriers[i].newLayout = RD_TO_VK_LAYOUT[p_texture_barriers[i].next_layout];
			vk_image_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vk_image_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vk_image_barriers[i].image = tex_info->vk_view_create_info.image;
			vk_image_barriers[i].subresourceRange.aspectMask = (VkImageAspectFlags)p_texture_barriers[i].subresources.aspect;
			vk_image_barriers[i].subresourceRange.baseMipLevel = p_texture_barriers[i].subresources.base_mipmap;
			vk_image_barriers[i].subresourceRange.levelCount = p_texture_barriers[i].subresources.mipmap_count;
			vk_image_barriers[i].subresourceRange.baseArrayLayer = p_texture_barriers[i].subresources.base_layer;
			vk_image_barriers[i].subresourceRange.layerCount = p_texture_barriers[i].subresources.layer_count;
		}

		VkPipelineStageFlags src_stage_flags = _rd_to_vk_pipeline_stages(p_src_stages);
		VkPipelineStageFlags dst_stage_flags = _rd_to_vk_pipeline_stages(p_dst_stages);
		VkPipelineStageFlags accel_src_stages = src_stage_flags;
		VkPipelineStageFlags accel_dst_stages = dst_stage_flags;

		// TODO: acceleration barriers
		//std::vector<VkBufferMemoryBarrier> vk_accel_barrier_vec(p_acceleration_structure_barriers.size());
		//VkBufferMemoryBarrier* vk_accel_barriers = vk_accel_barrier_vec.data();
		//for (uint32_t i = 0; i < p_acceleration_structure_barriers.size(); i++) {
		//	// If the rayQuery feature is not enabled and a memory barrier srcAccessMask includes
		//	// VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, srcStageMask must not include any of the
		//	// VK_PIPELINE_STAGE_*_SHADER_BIT stages except VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
		//	VkAccessFlags src_access = _rd_to_vk_access_flags(p_acceleration_structure_barriers[i].src_access);
		//	if ((src_access & VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR) != 0) {
		//		accel_src_stages &= ~(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		//	}

		//	// If the rayQuery feature is not enabled and a memory barrier dstAccessMask includes
		//	// VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, dstStageMask must not include any of the
		//	// VK_PIPELINE_STAGE_*_SHADER_BIT stages except VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
		//	VkAccessFlags dst_access = _rd_to_vk_access_flags(p_acceleration_structure_barriers[i].dst_access);
		//	if ((dst_access & VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR) != 0) {
		//		accel_dst_stages &= ~(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		//	}

		//	const AccelerationStructureInfo* accel_info = (const AccelerationStructureInfo*)p_acceleration_structure_barriers[i].acceleration_structure.id;
		//	vk_accel_barriers[i] = {};
		//	vk_accel_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		//	vk_accel_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//	vk_accel_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//	vk_accel_barriers[i].srcAccessMask = src_access;
		//	vk_accel_barriers[i].dstAccessMask = dst_access;
		//	vk_accel_barriers[i].buffer = ((const BufferInfo*)accel_info->buffer.id)->vk_buffer;
		//	vk_accel_barriers[i].offset = p_acceleration_structure_barriers[i].offset;
		//	vk_accel_barriers[i].size = p_acceleration_structure_barriers[i].size;
		//}

#if PRINT_NATIVE_COMMANDS
		LOGI(std::format("vkCmdPipelineBarrier MEMORY {} BUFFER {} TEXTURE {} ACCELERATION STRUCTURE {}", p_memory_barriers.size(), p_buffer_barriers.size(), p_texture_barriers.size(), p_acceleration_structure_barriers.size()));
		for (uint32_t i = 0; i < p_memory_barriers.size(); i++) {
			LOGI(std::format("  VkMemoryBarrier #{} src {} dst {}", i, vk_memory_barriers[i].srcAccessMask, vk_memory_barriers[i].dstAccessMask));
		}

		for (uint32_t i = 0; i < p_buffer_barriers.size(); i++) {
			LOGI(std::format("  VkBufferMemoryBarrier #{} src 0x%uX dst {} buffer {}", i, vk_buffer_barriers[i].srcAccessMask, vk_buffer_barriers[i].dstAccessMask, uint64_t(vk_buffer_barriers[i].buffer)));
		}

		for (uint32_t i = 0; i < p_texture_barriers.size(); i++) {
			LOGI(std::format("  VkImageMemoryBarrier #{} src {} dst {}X image {} old {} new {} ({} {} {} {})", i, vk_image_barriers[i].srcAccessMask, vk_image_barriers[i].dstAccessMask,
				uint64_t(vk_image_barriers[i].image), vk_image_barriers[i].oldLayout, vk_image_barriers[i].newLayout, vk_image_barriers[i].subresourceRange.baseMipLevel, vk_image_barriers[i].subresourceRange.levelCount,
				vk_image_barriers[i].subresourceRange.baseArrayLayer, vk_image_barriers[i].subresourceRange.layerCount));
		}

		for (uint32_t i = 0; i < p_acceleration_structure_barriers.size(); i++) {
			LOGI(std::format("  VkBufferMemoryBarrier #{} src 0x%uX dst {} acceleration structure buffer {}", i, vk_accel_barriers[i].srcAccessMask, vk_accel_barriers[i].dstAccessMask, uint64_t(vk_accel_barriers[i].buffer)));
		}
#endif

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdPipelineBarrier(
			command_buffer->vk_command_buffer,
			src_stage_flags,
			dst_stage_flags,
			0,
			p_memory_barriers.size(), vk_memory_barriers,
			p_buffer_barriers.size(), vk_buffer_barriers,
			p_texture_barriers.size(), vk_image_barriers);

		/*if (p_acceleration_structure_barriers.size() > 0) {
			vkCmdPipelineBarrier(
				command_buffer->vk_command_buffer,
				accel_src_stages,
				accel_dst_stages,
				0,
				0, nullptr,
				p_acceleration_structure_barriers.size(), vk_accel_barriers,
				0, nullptr);
		}*/
	}

#pragma endregion

#pragma region Fences

	RenderingDeviceDriverVulkan::FenceID RenderingDeviceDriverVulkan::fence_create() {
		VkFence vk_fence = VK_NULL_HANDLE;
		VkFenceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkResult err = vkCreateFence(vk_device, &create_info, nullptr, &vk_fence);
		ERR_FAIL_COND_V(err != VK_SUCCESS, FenceID());

		Fence* fence = new Fence;
		fence->vk_fence = vk_fence;
		fence->queue_signaled_from = nullptr;
		return FenceID(fence);
	}

	Error RenderingDeviceDriverVulkan::fence_wait(FenceID p_fence) {
		Fence* fence = (Fence*)(p_fence.id);
		VkResult fence_status = vkGetFenceStatus(vk_device, fence->vk_fence);
		if (fence_status == VK_NOT_READY) {		// fence is unsignaled (GPU still working)
			VkResult err = vkWaitForFences(vk_device, 1, &fence->vk_fence, VK_TRUE, UINT64_MAX);
			ERR_FAIL_COND_V(err != VK_SUCCESS, FAILED);
		}
		// when signaled
		VkResult err = vkResetFences(vk_device, 1, &fence->vk_fence); // set to unsignaled again
		ERR_FAIL_COND_V(err != VK_SUCCESS, FAILED);

		if (fence->queue_signaled_from != nullptr) {
			// Release all semaphores that the command queue associated to the fence waited on the last time it was submitted.
			std::vector<std::pair<Fence*, uint32_t>>& pairs = fence->queue_signaled_from->image_semaphores_for_fences;
			uint32_t i = 0;
			while (i < pairs.size()) {
				if (pairs[i].first == fence) {
					_release_image_semaphore(fence->queue_signaled_from, pairs[i].second, true);
					fence->queue_signaled_from->free_image_semaphores.push_back(pairs[i].second);
					pairs.erase(pairs.begin() + i);
				}
				else {
					i++;
				}
			}

			fence->queue_signaled_from = nullptr;
		}

		return OK;
	}

	void RenderingDeviceDriverVulkan::fence_free(FenceID p_fence) {
		Fence* fence = (Fence*)(p_fence.id);
		vkDestroyFence(vk_device, fence->vk_fence, nullptr);
		delete fence;
	}

#pragma endregion

#pragma region Semaphores

	RenderingDeviceDriverVulkan::SemaphoreID RenderingDeviceDriverVulkan::semaphore_create() {
		VkSemaphore semaphore = VK_NULL_HANDLE;
		VkSemaphoreCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkResult err = vkCreateSemaphore(vk_device, &create_info, nullptr, &semaphore);
		ERR_FAIL_COND_V(err != VK_SUCCESS, SemaphoreID());

		return SemaphoreID(semaphore);
	}

	void RenderingDeviceDriverVulkan::semaphore_free(SemaphoreID p_semaphore) {
		vkDestroySemaphore(vk_device, VkSemaphore(p_semaphore.id), nullptr);
	}

#pragma endregion

#pragma region Command

	// ----- QUEUE FAMILY -----
	RenderingDeviceDriverVulkan::CommandQueueFamilyID RenderingDeviceDriverVulkan::command_queue_family_get(BitField<RenderingDeviceDriverVulkan::CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriverVulkan::SurfaceID p_surface) {
		// Pick the queue with the least amount of bits that can fulfill the requirements.
		VkQueueFlags picked_queue_flags = VK_QUEUE_FLAG_BITS_MAX_ENUM;
		uint32_t picked_family_index = UINT_MAX;
		for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
			if (queue_families[i].empty()) {
				// Ignore empty queue families.
				continue;
			}

			if (p_surface != 0 && !context_driver->queue_family_supports_present(physical_device, i, p_surface)) {
				// Present is not an actual bit but something that must be queried manually.
				continue;
			}

			// Preferring a queue with less bits will get us closer to getting a queue that performs better for our requirements.
			// For example, dedicated compute and transfer queues are usually indicated as such.
			const VkQueueFlags option_queue_flags = queue_family_properties[i].queueFlags;
			const bool includes_all_bits = p_cmd_queue_family_bits.get_shared(option_queue_flags) == p_cmd_queue_family_bits;
			const bool prefer_less_bits = option_queue_flags < picked_queue_flags;
			if (includes_all_bits && prefer_less_bits) {
				picked_family_index = i;
				picked_queue_flags = option_queue_flags;
			}
		}

		if (picked_family_index >= queue_family_properties.size()) {
			return CommandQueueFamilyID();
		}

		// Since 0 is a valid index and we use 0 as the error case, we make the index start from 1 instead.
		return CommandQueueFamilyID(picked_family_index + 1);
	}

	// ----- QUEUE -----

	RenderingDeviceDriverVulkan::CommandQueueID RenderingDeviceDriverVulkan::command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue) {
		DEV_ASSERT(p_cmd_queue_family.id != 0);

		// Make a virtual queue on top of a real queue. Use the queue from the family with the least amount of virtual queues created.
		uint32_t family_index = p_cmd_queue_family.id - 1;
		std::vector<Queue>& queue_family = queue_families[family_index];
		uint32_t picked_queue_index = UINT_MAX;
		uint32_t picked_virtual_count = UINT_MAX;
		for (uint32_t i = 0; i < queue_family.size(); i++) {
			if (queue_family[i].virtual_count < picked_virtual_count) {
				picked_queue_index = i;
				picked_virtual_count = queue_family[i].virtual_count;
			}
		}

		ERR_FAIL_COND_V_MSG(picked_queue_index >= queue_family.size(), CommandQueueID(), "A queue in the picked family could not be found.");

		// Create the virtual queue.
		CommandQueue* command_queue = new CommandQueue;
		command_queue->queue_family = family_index;
		command_queue->queue_index = picked_queue_index;
		queue_family[picked_queue_index].virtual_count++;

		// If is was identified as the main queue and a hook is active, indicate it as such to the hook.
		//if (p_identify_as_main_queue && (VulkanHooks::get_singleton() != nullptr)) {
		//	VulkanHooks::get_singleton()->set_direct_queue_family_and_index(family_index, picked_queue_index);
		//}

		return CommandQueueID(command_queue);
	}

	Error RenderingDeviceDriverVulkan::command_queue_execute_and_present(CommandQueueID p_cmd_queue, 
								std::span<SemaphoreID> p_wait_semaphores, 
								std::span<CommandBufferID> p_cmd_buffers, 
								std::span<SemaphoreID> p_cmd_semaphores, 
								FenceID p_cmd_fence, 
								std::span<SwapChainID> p_swap_chains) {
		DEV_ASSERT(p_cmd_queue.id != 0);

		VkResult err;
		CommandQueue* command_queue = (CommandQueue*)(p_cmd_queue.id);
		Queue& device_queue = queue_families[command_queue->queue_family][command_queue->queue_index];
		Fence* fence = (Fence*)(p_cmd_fence.id);
		VkFence vk_fence = (fence != nullptr) ? fence->vk_fence : VK_NULL_HANDLE;

		thread_local std::vector<VkSemaphore> wait_semaphores;
		thread_local std::vector<VkPipelineStageFlags> wait_semaphores_stages;
		wait_semaphores.clear();
		wait_semaphores_stages.clear();

		if (!command_queue->pending_semaphores_for_execute.empty()) {
			for (uint32_t i = 0; i < command_queue->pending_semaphores_for_execute.size(); i++) {
				VkSemaphore wait_semaphore = command_queue->image_semaphores[command_queue->pending_semaphores_for_execute[i]];
				wait_semaphores.push_back(wait_semaphore);
				wait_semaphores_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			}

			command_queue->pending_semaphores_for_execute.clear();
		}

		for (uint32_t i = 0; i < p_wait_semaphores.size(); i++) {
			// FIXME: Allow specifying the stage mask in more detail.
			wait_semaphores.push_back(VkSemaphore(p_wait_semaphores[i].id));
			wait_semaphores_stages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}

		if (!pending_flushes.allocations.empty()) {
			// We must do this now, even if p_cmd_buffers is empty; because afterwards pending_flushes.allocations
			// could become dangling. We cannot delay this call for the next frame(s).
			err = vmaFlushAllocations(allocator, pending_flushes.allocations.size(),
				pending_flushes.allocations.data(), pending_flushes.offsets.data(),
				pending_flushes.sizes.data());
			pending_flushes.allocations.clear();
			pending_flushes.offsets.clear();
			pending_flushes.sizes.clear();
			ERR_FAIL_COND_V(err != VK_SUCCESS, FAILED);
		}

		if (p_cmd_buffers.size() > 0) {
			thread_local std::vector<VkCommandBuffer> command_buffers;
			thread_local std::vector<VkSemaphore> present_semaphores;
			thread_local std::vector<VkSemaphore> signal_semaphores;
			command_buffers.clear();
			present_semaphores.clear();
			signal_semaphores.clear();

			for (uint32_t i = 0; i < p_cmd_buffers.size(); i++) {
				const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)(p_cmd_buffers[i].id);
				command_buffers.push_back(command_buffer->vk_command_buffer);
			}

			for (uint32_t i = 0; i < p_cmd_semaphores.size(); i++) {
				signal_semaphores.push_back(VkSemaphore(p_cmd_semaphores[i].id));
			}

			for (uint32_t i = 0; i < p_swap_chains.size(); i++) {
				const SwapChain* swap_chain = (const SwapChain*)(p_swap_chains[i].id);
				VkSemaphore semaphore = swap_chain->present_semaphores[swap_chain->image_index];
				present_semaphores.push_back(semaphore);
				signal_semaphores.push_back(semaphore);
			}

			VkSubmitInfo submit_info = {};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.waitSemaphoreCount = wait_semaphores.size();
			submit_info.pWaitSemaphores = wait_semaphores.data();
			submit_info.pWaitDstStageMask = wait_semaphores_stages.data();		// wait for these simaphores
			submit_info.commandBufferCount = command_buffers.size();
			submit_info.pCommandBuffers = command_buffers.data();
			submit_info.signalSemaphoreCount = signal_semaphores.size();		// signal these when done
			submit_info.pSignalSemaphores = signal_semaphores.data();

			device_queue.submit_mutex.lock();
			err = vkQueueSubmit(device_queue.queue, 1, &submit_info, vk_fence);
			device_queue.submit_mutex.unlock();

			if (err == VK_ERROR_DEVICE_LOST) {
				print_lost_device_info();
				CRASH_NOW_MSG("Vulkan device was lost.");
			}
			ERR_FAIL_COND_V(err != VK_SUCCESS, FAILED);

			if (fence != nullptr && !command_queue->pending_semaphores_for_fence.empty()) {
				fence->queue_signaled_from = command_queue;

				// Indicate to the fence that it should release the semaphores that were waited on this submission the next time the fence is waited on.
				for (uint32_t i = 0; i < command_queue->pending_semaphores_for_fence.size(); i++) {
					command_queue->image_semaphores_for_fences.push_back({ fence, command_queue->pending_semaphores_for_fence[i] });
				}

				command_queue->pending_semaphores_for_fence.clear();
			}

			if (!present_semaphores.empty()) {
				// If command buffers were executed, swap chains must wait on the present semaphore used by the command queue.
				wait_semaphores = present_semaphores;
			}
		}

		if (p_swap_chains.size() > 0) {
			thread_local std::vector<VkSwapchainKHR> swapchains;
			thread_local std::vector<uint32_t> image_indices;
			thread_local std::vector<VkResult> results;
			swapchains.clear();
			image_indices.clear();

			for (uint32_t i = 0; i < p_swap_chains.size(); i++) {
				SwapChain* swap_chain = (SwapChain*)(p_swap_chains[i].id);
				swapchains.push_back(swap_chain->vk_swapchain);
				DEV_ASSERT(swap_chain->image_index < swap_chain->images.size());
				image_indices.push_back(swap_chain->image_index);
			}

			results.resize(swapchains.size());

			VkPresentInfoKHR present_info = {};
			present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present_info.waitSemaphoreCount = wait_semaphores.size();
			present_info.pWaitSemaphores = wait_semaphores.data();
			present_info.swapchainCount = swapchains.size();
			present_info.pSwapchains = swapchains.data();
			present_info.pImageIndices = image_indices.data();
			present_info.pResults = results.data();

			device_queue.submit_mutex.lock();

			err = vkQueuePresentKHR(device_queue.queue, &present_info);

			device_queue.submit_mutex.unlock();

			// Set the index to an invalid value. If any of the swap chains returned out of date, indicate it should be resized the next time it's acquired.
			bool any_result_is_out_of_date = false;
			for (uint32_t i = 0; i < p_swap_chains.size(); i++) {
				SwapChain* swap_chain = (SwapChain*)(p_swap_chains[i].id);
				swap_chain->image_index = UINT_MAX;
				if (results[i] == VK_ERROR_OUT_OF_DATE_KHR) {
					context_driver->surface_set_needs_resize(swap_chain->surface, true);
					any_result_is_out_of_date = true;
				}
			}

			if (any_result_is_out_of_date || err == VK_ERROR_OUT_OF_DATE_KHR) {
				// It is possible for presentation to fail with out of date while acquire might've succeeded previously. This case
				// will be considered a silent failure as it can be triggered easily by resizing a window in the OS natively.
				return FAILED;
			}

			// Handling VK_SUBOPTIMAL_KHR the same as VK_SUCCESS is completely intentional.
			//
			// Godot does not currently support native rotation in Android when creating the swap chain. It intentionally uses
			// VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR instead of the current transform bits available in the surface capabilities.
			// Choosing the transform that leads to optimal presentation leads to distortion that makes the application unusable,
			// as the rotation of all the content is not handled at the moment.
			//
			// VK_SUBOPTIMAL_KHR is accepted as a successful case even if it's not the most efficient solution to work around this
			// problem. This behavior should not be changed unless the swap chain recreation uses the current transform bits, as
			// it'll lead to very low performance in Android by entering an endless loop where it'll always resize the swap chain
			// every frame.

			ERR_FAIL_COND_V_MSG(
				err != VK_SUCCESS && err != VK_SUBOPTIMAL_KHR,
				FAILED,
				std::format("QueuePresentKHR failed with error: {} .", get_vulkan_result(err)));
		}

		return OK;
	}

	void RenderingDeviceDriverVulkan::command_queue_free(CommandQueueID p_cmd_queue) {
		DEV_ASSERT(p_cmd_queue);

		CommandQueue* command_queue = (CommandQueue*)(p_cmd_queue.id);

		// Erase all the semaphores used for image acquisition.
		for (VkSemaphore semaphore : command_queue->image_semaphores) {
			vkDestroySemaphore(vk_device, semaphore, nullptr);
		}

		// Retrieve the queue family corresponding to the virtual queue.
		DEV_ASSERT(command_queue->queue_family < queue_families.size());
		std::vector<Queue>& queue_family = queue_families[command_queue->queue_family];

		// Decrease the virtual queue count.
		DEV_ASSERT(command_queue->queue_index < queue_family.size());
		DEV_ASSERT(queue_family[command_queue->queue_index].virtual_count > 0);
		queue_family[command_queue->queue_index].virtual_count--;

		// Destroy the virtual queue structure.
		delete command_queue;
	}

	// ----- POOL -----

	RenderingDeviceDriverVulkan::CommandPoolID RenderingDeviceDriverVulkan::command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) {
		DEV_ASSERT(p_cmd_queue_family.id != 0);

		uint32_t family_index = p_cmd_queue_family.id - 1;
		VkCommandPoolCreateInfo cmd_pool_info = {};
		cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_info.queueFamilyIndex = family_index;

		if (!command_pool_reset_enabled) {
			cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		}

		VkCommandPool vk_command_pool = VK_NULL_HANDLE;
		VkResult res = vkCreateCommandPool(vk_device, &cmd_pool_info, nullptr, &vk_command_pool);
		ERR_FAIL_COND_V_MSG(res, CommandPoolID(), std::format("vkCreateCommandPool failed with error {} .",std::to_string(res)));

		CommandPool* command_pool = new CommandPool;
		command_pool->vk_command_pool = vk_command_pool;
		command_pool->buffer_type = p_cmd_buffer_type;
		return CommandPoolID(command_pool);
	}

	bool RenderingDeviceDriverVulkan::command_pool_reset(CommandPoolID p_cmd_pool) {
		DEV_ASSERT(p_cmd_pool);

		CommandPool* command_pool = (CommandPool*)(p_cmd_pool.id);
		VkResult err = vkResetCommandPool(vk_device, command_pool->vk_command_pool, 0);
		ERR_FAIL_COND_V_MSG(err, false, std::format("vkResetCommandPool failed with error {} .", std::to_string(err)));

		return true;
	}

	void RenderingDeviceDriverVulkan::command_pool_free(CommandPoolID p_cmd_pool) {
		DEV_ASSERT(p_cmd_pool);

		CommandPool* command_pool = (CommandPool*)(p_cmd_pool.id);
		for (CommandBufferInfo* command_buffer : command_pool->command_buffers_created) {
			delete command_buffer;
		}

		vkDestroyCommandPool(vk_device, command_pool->vk_command_pool, nullptr);
		delete command_pool;
	}

	// ----- BUFFER -----

	RenderingDeviceDriverVulkan::CommandBufferID RenderingDeviceDriverVulkan::command_buffer_create(CommandPoolID p_cmd_pool) {
		DEV_ASSERT(p_cmd_pool);

		CommandPool* command_pool = (CommandPool*)(p_cmd_pool.id);
		VkCommandBufferAllocateInfo cmd_buf_info = {};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_buf_info.commandPool = command_pool->vk_command_pool;
		cmd_buf_info.commandBufferCount = 1;

		if (command_pool->buffer_type == COMMAND_BUFFER_TYPE_SECONDARY) {
			cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		}
		else {
			cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		}

		VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;
		VkResult err = vkAllocateCommandBuffers(vk_device, &cmd_buf_info, &vk_command_buffer);
		ERR_FAIL_COND_V_MSG(err, CommandBufferID(), std::format("vkAllocateCommandBuffers failed with error {} .", std::to_string(err)));

		CommandBufferInfo* command_buffer = new CommandBufferInfo;
		command_buffer->vk_command_buffer = vk_command_buffer;
		command_pool->command_buffers_created.push_back(command_buffer);
		return CommandBufferID(command_buffer);
	}

	bool RenderingDeviceDriverVulkan::command_buffer_begin(CommandBufferID p_cmd_buffer) {
		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);

		VkCommandBufferBeginInfo cmd_buf_begin_info = {};
		cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkResult err = vkBeginCommandBuffer(command_buffer->vk_command_buffer, &cmd_buf_begin_info);
		ERR_FAIL_COND_V_MSG(err, false, std::format("vkBeginCommandBuffer failed with error {} .", std::to_string(err)));

		return true;
	}

	bool RenderingDeviceDriverVulkan::command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) {
		Framebuffer* framebuffer = (Framebuffer*)(p_framebuffer.id);
		RenderPassInfo* render_pass = (RenderPassInfo*)(p_render_pass.id);
		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);

		VkCommandBufferInheritanceInfo inheritance_info = {};
		inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance_info.renderPass = render_pass->vk_render_pass;
		inheritance_info.subpass = p_subpass;
		inheritance_info.framebuffer = framebuffer->vk_framebuffer;

		VkCommandBufferBeginInfo cmd_buf_begin_info = {};
		cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		cmd_buf_begin_info.pInheritanceInfo = &inheritance_info;

		VkResult err = vkBeginCommandBuffer(command_buffer->vk_command_buffer, &cmd_buf_begin_info);
		ERR_FAIL_COND_V_MSG(err, false, std::format("vkBeginCommandBuffer failed with error {} .", std::to_string(err)));

		return true;
	}

	void RenderingDeviceDriverVulkan::command_buffer_end(CommandBufferID p_cmd_buffer) {
		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);
		vkEndCommandBuffer(command_buffer->vk_command_buffer);
	}

	void RenderingDeviceDriverVulkan::command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, std::span<CommandBufferID> p_secondary_cmd_buffers) {
		thread_local std::vector<VkCommandBuffer> secondary_command_buffers;
		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);
		secondary_command_buffers.resize(p_secondary_cmd_buffers.size());
		for (uint32_t i = 0; i < p_secondary_cmd_buffers.size(); i++) {
			CommandBufferInfo* secondary_command_buffer = (CommandBufferInfo*)(p_secondary_cmd_buffers[i].id);
			secondary_command_buffers[i] = secondary_command_buffer->vk_command_buffer;
		}

		vkCmdExecuteCommands(command_buffer->vk_command_buffer, p_secondary_cmd_buffers.size(), secondary_command_buffers.data());
	}

#pragma endregion

#pragma region Swapchain

	struct FormatCandidate {
		VkFormat format;
		VkColorSpaceKHR colorspace;
	};

	bool RenderingDeviceDriverVulkan::_determine_swap_chain_format(RenderingContextDriverVulkan::SurfaceID p_surface, VkFormat& r_format, VkColorSpaceKHR& r_color_space) {
		DEV_ASSERT(p_surface != 0);

		RenderingContextDriverVulkan::Surface* surface = (RenderingContextDriverVulkan::Surface*)(p_surface);

		// Retrieve the formats supported by the surface.
		uint32_t format_count = 0;
		VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface->vk_surface, &format_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, false);

		std::vector<VkSurfaceFormatKHR> formats;
		formats.resize(format_count);
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface->vk_surface, &format_count, formats.data());
		ERR_FAIL_COND_V(err != VK_SUCCESS, false);

		// If the format list includes just one entry of VK_FORMAT_UNDEFINED, the surface has no preferred format.
		if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
			r_format = VK_FORMAT_B8G8R8A8_UNORM;
			r_color_space = formats[0].colorSpace;
			return true;
		}

		bool colorspace_supported = context_driver->is_colorspace_supported();
		bool hdr_output_requested = context_driver->surface_get_hdr_output_enabled(p_surface);

		// Determine which formats to prefer based on the requested capabilities.
		std::vector<FormatCandidate> preferred_formats;

		// If the surface requests HDR output, try to get an HDR format.
		if (hdr_output_requested && colorspace_supported) {
			// This format is preferred for HDR output.
			preferred_formats.push_back({ VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT });
		}

		// These formats are always considered for SDR.
		preferred_formats.push_back({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
		preferred_formats.push_back({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });

		bool found = false;
		for (const FormatCandidate& candidate : preferred_formats) {
			for (uint32_t i = 0; i < format_count; i++) {
				if (formats[i].format == candidate.format && formats[i].colorSpace == candidate.colorspace) {
					r_format = formats[i].format;
					r_color_space = formats[i].colorSpace;
					found = true;
					break;
				}
			}

			if (found) {
				break;
			}
		}

		// Warnings for when HDR capabilities are requested but not found.
		if (hdr_output_requested) {
			if (!colorspace_supported) {
				WARN_PRINT("HDR output requested but the vulkan driver does not support VK_EXT_swapchain_colorspace, falling back to SDR.");
			}

			if (r_color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				WARN_PRINT("HDR output requested but no HDR compatible format was found, falling back to SDR.");
			}
		}

		return found;
	}

	void RenderingDeviceDriverVulkan::_swap_chain_release(SwapChain* swap_chain) {
		// Destroy views and framebuffers associated to the swapchain's images.
		for (FramebufferID framebuffer : swap_chain->framebuffers) {
			framebuffer_free(framebuffer);
		}

		for (VkImageView view : swap_chain->image_views) {
			vkDestroyImageView(vk_device, view, nullptr);
		}

		swap_chain->image_index = UINT_MAX;
		swap_chain->images.clear();
		swap_chain->image_views.clear();
		swap_chain->framebuffers.clear();

		if (swap_chain->vk_swapchain != VK_NULL_HANDLE) 
		{
			vkDestroySwapchainKHR(vk_device, swap_chain->vk_swapchain, nullptr);
			swap_chain->vk_swapchain = VK_NULL_HANDLE;
		}

		if (swap_chain->render_pass.id != 0) {
			render_pass_free(swap_chain->render_pass);
			swap_chain->render_pass = RenderPassID();
		}

		for (uint32_t i = 0; i < swap_chain->command_queues_acquired.size(); i++) {
			_recreate_image_semaphore(swap_chain->command_queues_acquired[i], swap_chain->command_queues_acquired_semaphores[i], false);
		}

		swap_chain->command_queues_acquired.clear();
		swap_chain->command_queues_acquired_semaphores.clear();

		for (VkSemaphore semaphore : swap_chain->present_semaphores) {
			vkDestroySemaphore(vk_device, semaphore, nullptr);
		}

		swap_chain->present_semaphores.clear();
	}

	RenderingDeviceDriverVulkan::SwapChainID RenderingDeviceDriverVulkan::swap_chain_create(RenderingContextDriverVulkan::SurfaceID p_surface) {
		DEV_ASSERT(p_surface != 0);

		// Create an empty swap chain until it is resized.
		SwapChain* swap_chain = new SwapChain;
		swap_chain->surface = p_surface;
		return SwapChainID(swap_chain);
	}

	Error RenderingDeviceDriverVulkan::swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) {
		DEV_ASSERT(p_cmd_queue.id != 0);
		DEV_ASSERT(p_swap_chain.id != 0);

		CommandQueue* command_queue = (CommandQueue*)(p_cmd_queue.id);
		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);

		// Release all current contents of the swap chain.
		_swap_chain_release(swap_chain);

		// Validate if the command queue being used supports creating the swap chain for this surface.
		if (!context_driver->queue_family_supports_present(physical_device, command_queue->queue_family, swap_chain->surface)) {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Surface is not supported by device. Did the GPU go offline? Was the window created on another monitor? Check"
				"previous errors & try launching with --gpu-validation.");
		}

		// Retrieve the surface's capabilities.
		RenderingContextDriverVulkan::Surface* surface = (RenderingContextDriverVulkan::Surface*)(swap_chain->surface);
		VkSurfaceCapabilitiesKHR surface_capabilities = {};
		VkResult err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface->vk_surface, &surface_capabilities);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		// No swapchain yet, this is the first time we're creating it.
		if (!swap_chain->vk_swapchain) {
			if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
				// The current extent is currently undefined, so the current surface width and height will be clamped to the surface's capabilities.
				// We make sure to overwrite surface_capabilities.currentExtent.width so that the same check further below
				// does not set extent.width = CLAMP( surface->width, ... ) on the first run of this function, because
				// that'd be potentially unswapped.
				surface_capabilities.currentExtent.width = CLAMP(surface->width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
				surface_capabilities.currentExtent.height = CLAMP(surface->height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
			}

			// We must SWAP() only once otherwise we'll keep ping-ponging between
			// the right and wrong resolutions after multiple calls to swap_chain_resize().
			if (surface_capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
				surface_capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
				// Swap to get identity width and height.
				SWAP(surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height);
			}
		}

		VkExtent2D extent;
		if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
			// The current extent is currently undefined, so the current surface width and height will be clamped to the surface's capabilities.
			// We can only be here on the second call to swap_chain_resize(), by which time surface->width & surface->height should already be swapped if needed.
			extent.width = CLAMP(surface->width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
			extent.height = CLAMP(surface->height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
		}
		else {
			// Grab the dimensions from the current extent.
			extent = surface_capabilities.currentExtent;
			surface->width = extent.width;
			surface->height = extent.height;
		}

		if (surface->width == 0 || surface->height == 0) {
			// The surface doesn't have valid dimensions, so we can't create a swap chain.
			return ERR_SKIP;
		}

		// Find what present modes are supported.
		std::vector<VkPresentModeKHR> present_modes;
		uint32_t present_modes_count = 0;
		err = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface->vk_surface, &present_modes_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		present_modes.resize(present_modes_count);
		err = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface->vk_surface, &present_modes_count, present_modes.data());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		// Choose the present mode based on the display server setting.
		VkPresentModeKHR present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
		std::string present_mode_name = "Enabled";
		switch (surface->vsync_mode) {
		case DisplayServerEnums::VSyncMode::VSYNC_MAILBOX:
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			present_mode_name = "Mailbox";
			break;
		case DisplayServerEnums::VSyncMode::VSYNC_ADAPTIVE:
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			present_mode_name = "Adaptive";
			break;
		case DisplayServerEnums::VSyncMode::VSYNC_ENABLED:
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			present_mode_name = "Enabled";
			break;
		case DisplayServerEnums::VSyncMode::VSYNC_DISABLED:
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			present_mode_name = "Disabled";
			break;
		}

		bool present_mode_available = (std::find(present_modes.begin(), present_modes.end(), present_mode) != present_modes.end());
		if (!present_mode_available) {
			// Present mode is not available, fall back to FIFO which is guaranteed to be supported.
			WARN_PRINT(std::format("The requested V-Sync mode {} is not available. Falling back to V-Sync mode Enabled.", present_mode_name));
			surface->vsync_mode = DisplayServerEnums::VSyncMode::VSYNC_ENABLED;
			present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
		}

		// Clamp the desired image count to the surface's capabilities.
		uint32_t desired_swapchain_images = MAX(p_desired_framebuffer_count, surface_capabilities.minImageCount);
		if (surface_capabilities.maxImageCount > 0) {
			// Only clamp to the max image count if it's defined. A max image count of 0 means there's no upper limit to the amount of images.
			desired_swapchain_images = MIN(desired_swapchain_images, surface_capabilities.maxImageCount);
		}

		// Refer to the comment in command_queue_present() for more details.
		VkSurfaceTransformFlagBitsKHR surface_transform_bits = surface_capabilities.currentTransform;

		VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		if (true/*OS::get_singleton()->is_layered_allowed()*/ || !(surface_capabilities.supportedCompositeAlpha & composite_alpha)) {
			// Find a supported composite alpha mode - one of these is guaranteed to be set.
			VkCompositeAlphaFlagBitsKHR composite_alpha_flags[4] = {
				VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
				VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
			};

			for (uint32_t i = 0; i < std::size(composite_alpha_flags); i++) {
				if (surface_capabilities.supportedCompositeAlpha & composite_alpha_flags[i]) {
					composite_alpha = composite_alpha_flags[i];
					break;
				}
			}
			has_comp_alpha[(uint64_t)p_cmd_queue.id] = (composite_alpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
		}

		// Determine the format and color space for the swap chain.
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		if (!_determine_swap_chain_format(swap_chain->surface, format, color_space)) {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Surface did not return any valid formats.");
		}
		else {
			swap_chain->format = format;
			swap_chain->color_space = color_space;
		}

		VkSwapchainCreateInfoKHR swap_create_info = {};
		swap_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_create_info.surface = surface->vk_surface;
		swap_create_info.minImageCount = desired_swapchain_images;
		swap_create_info.imageFormat = swap_chain->format;
		swap_create_info.imageColorSpace = swap_chain->color_space;
		swap_create_info.imageExtent = extent;
		swap_create_info.imageArrayLayers = 1;
		swap_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swap_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swap_create_info.preTransform = surface_transform_bits;
		switch (swap_create_info.preTransform) {
		case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
			swap_chain->pre_transform_rotation_degrees = 0;
			break;
		case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
			swap_chain->pre_transform_rotation_degrees = 90;
			break;
		case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
			swap_chain->pre_transform_rotation_degrees = 180;
			break;
		case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
			swap_chain->pre_transform_rotation_degrees = 270;
			break;
		default:
			WARN_PRINT("Unexpected swap_create_info.preTransform = ", std::to_string(swap_create_info.preTransform), ".");
			swap_chain->pre_transform_rotation_degrees = 0;
			break;
		}
		swap_create_info.compositeAlpha = composite_alpha;
		swap_create_info.presentMode = present_mode;
		swap_create_info.clipped = true;
		err = vkCreateSwapchainKHR(vk_device, &swap_create_info, nullptr, &swap_chain->vk_swapchain);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		uint32_t image_count = 0;
		err = vkGetSwapchainImagesKHR(vk_device, swap_chain->vk_swapchain, &image_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		swap_chain->images.resize(image_count);
		err = vkGetSwapchainImagesKHR(vk_device, swap_chain->vk_swapchain, &image_count, swap_chain->images.data());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		VkImageViewCreateInfo view_create_info = {};
		view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_create_info.format = swap_chain->format;
		view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
		view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_create_info.subresourceRange.levelCount = 1;
		view_create_info.subresourceRange.layerCount = 1;

		swap_chain->image_views.reserve(image_count);

		VkImageView image_view;
		for (uint32_t i = 0; i < image_count; i++) {
			view_create_info.image = swap_chain->images[i];
			err = vkCreateImageView(vk_device, &view_create_info, nullptr, &image_view);
			ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

			swap_chain->image_views.push_back(image_view);
		}

		swap_chain->framebuffers.reserve(image_count);

		// Create the render pass for the chosen format.
		VkAttachmentDescription2KHR attachment = {};
		attachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
		attachment.format = format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference2KHR color_reference = {};
		color_reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription2KHR subpass = {};
		subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;

		VkRenderPassCreateInfo2KHR pass_info = {};
		pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
		pass_info.attachmentCount = 1;
		pass_info.pAttachments = &attachment;
		pass_info.subpassCount = 1;
		pass_info.pSubpasses = &subpass;

		VkRenderPass vk_render_pass = VK_NULL_HANDLE;
		err = _create_render_pass(vk_device, &pass_info, nullptr, &vk_render_pass);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		RenderPassInfo* render_pass_info = new RenderPassInfo;
		render_pass_info->vk_render_pass = vk_render_pass;

		DEV_ASSERT(swap_chain->render_pass.id == 0);
		swap_chain->render_pass = RenderPassID(render_pass_info);

		VkFramebufferCreateInfo fb_create_info = {};
		fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_create_info.renderPass = vk_render_pass;
		fb_create_info.attachmentCount = 1;
		fb_create_info.width = surface->width;
		fb_create_info.height = surface->height;
		fb_create_info.layers = 1;

		VkFramebuffer vk_framebuffer;
		for (uint32_t i = 0; i < image_count; i++) {
			fb_create_info.pAttachments = &swap_chain->image_views[i];
			err = vkCreateFramebuffer(vk_device, &fb_create_info, nullptr, &vk_framebuffer);
			ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

			Framebuffer* framebuffer = new Framebuffer;
			framebuffer->vk_framebuffer = vk_framebuffer;
			framebuffer->swap_chain_image = swap_chain->images[i];
			framebuffer->swap_chain_image_subresource_range = view_create_info.subresourceRange;
			swap_chain->framebuffers.push_back(RenderingDeviceDriverVulkan::FramebufferID(framebuffer));
		}

		VkSemaphore vk_semaphore = VK_NULL_HANDLE;
		for (uint32_t i = 0; i < image_count; i++) {
			VkSemaphoreCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			err = vkCreateSemaphore(vk_device, &create_info, nullptr, &vk_semaphore);
			ERR_FAIL_COND_V(err != VK_SUCCESS, FAILED);

			swap_chain->present_semaphores.push_back(vk_semaphore);
		}

		// Once everything's been created correctly, indicate the surface no longer needs to be resized.
		context_driver->surface_set_needs_resize(swap_chain->surface, false);

		return OK;
	}

	RenderingDeviceDriverVulkan::FramebufferID RenderingDeviceDriverVulkan::swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool& r_resize_required) {
		DEV_ASSERT(p_cmd_queue);
		DEV_ASSERT(p_swap_chain);

		CommandQueue* command_queue = (CommandQueue*)(p_cmd_queue.id);
		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		if ((swap_chain->vk_swapchain == VK_NULL_HANDLE) || context_driver->surface_get_needs_resize(swap_chain->surface)) {
			// The surface does not have a valid swap chain or it indicates it requires a resize.
			r_resize_required = true;
			return FramebufferID();
		}

		VkResult err;
		VkSemaphore semaphore = VK_NULL_HANDLE;
		uint32_t semaphore_index = 0;
		if (command_queue->free_image_semaphores.empty()) {
			// Add a new semaphore if none are free.
			VkSemaphoreCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			err = vkCreateSemaphore(vk_device, &create_info, nullptr, &semaphore);
			ERR_FAIL_COND_V(err != VK_SUCCESS, FramebufferID());

			semaphore_index = command_queue->image_semaphores.size();
			command_queue->image_semaphores.push_back(semaphore);
			command_queue->image_semaphores_swap_chains.push_back(swap_chain);
		}
		else {
			// Pick a free semaphore.
			uint32_t free_index = command_queue->free_image_semaphores.size() - 1;
			semaphore_index = command_queue->free_image_semaphores[free_index];
			command_queue->image_semaphores_swap_chains[semaphore_index] = swap_chain;
			command_queue->free_image_semaphores.erase(command_queue->free_image_semaphores.begin() + free_index);
			semaphore = command_queue->image_semaphores[semaphore_index];
		}

		// Store in the swap chain the acquired semaphore.
		swap_chain->command_queues_acquired.push_back(command_queue);
		swap_chain->command_queues_acquired_semaphores.push_back(semaphore_index);

		err = vkAcquireNextImageKHR(vk_device, swap_chain->vk_swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &swap_chain->image_index);
		if (err == VK_ERROR_OUT_OF_DATE_KHR) {
			// Out of date leaves the semaphore in a signaled state that will never finish, so it's necessary to recreate it.
			bool semaphore_recreated = _recreate_image_semaphore(command_queue, semaphore_index, true);
			ERR_FAIL_COND_V(!semaphore_recreated, FramebufferID());

			// Swap chain is out of date and must be recreated.
			r_resize_required = true;
			return FramebufferID();
		}
		else if (err != VK_SUCCESS && err != VK_SUBOPTIMAL_KHR) {
			// Swap chain failed to present but the reason is unknown.
			// Refer to the comment in command_queue_present() as to why VK_SUBOPTIMAL_KHR is handled the same as VK_SUCCESS.
			return FramebufferID();
		}

		// Indicate the command queue should wait on these semaphores on the next submission and that it should
		// indicate they're free again on the next fence.
		command_queue->pending_semaphores_for_execute.push_back(semaphore_index);
		command_queue->pending_semaphores_for_fence.push_back(semaphore_index);

		// Return the corresponding framebuffer to the new current image.
		FramebufferID framebuffer_id = swap_chain->framebuffers[swap_chain->image_index];
		Framebuffer* framebuffer = (Framebuffer*)(framebuffer_id.id);
		framebuffer->swap_chain_acquired = true;
		return framebuffer_id;
	}

	RenderingDeviceDriverVulkan::RenderPassID RenderingDeviceDriverVulkan::swap_chain_get_render_pass(SwapChainID p_swap_chain) {
		DEV_ASSERT(p_swap_chain.id != 0);

		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		return swap_chain->render_pass;
	}

	int RenderingDeviceDriverVulkan::swap_chain_get_pre_rotation_degrees(SwapChainID p_swap_chain) {
		DEV_ASSERT(p_swap_chain.id != 0);

		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		return swap_chain->pre_transform_rotation_degrees;
	}

	RenderingDeviceCommons::DataFormat RenderingDeviceDriverVulkan::swap_chain_get_format(SwapChainID p_swap_chain) {
		DEV_ASSERT(p_swap_chain.id != 0);

		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		switch (swap_chain->format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
			return DATA_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_R8G8B8A8_UNORM:
			return DATA_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			return DATA_FORMAT_A2B10G10R10_UNORM_PACK32;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
			return DATA_FORMAT_A2R10G10B10_UNORM_PACK32;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return DATA_FORMAT_R16G16B16A16_SFLOAT;
		default:
			DEV_ASSERT(false && "Unknown swap chain format.");
			return DATA_FORMAT_MAX;
		}
	}

	RenderingDeviceCommons::ColorSpace RenderingDeviceDriverVulkan::swap_chain_get_color_space(SwapChainID p_swap_chain) {
		DEV_ASSERT(p_swap_chain.id != 0);

		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		switch (swap_chain->color_space) {
		case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
			return COLOR_SPACE_REC709_NONLINEAR_SRGB;
		case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
			return COLOR_SPACE_REC709_LINEAR;
		default:
			DEV_ASSERT(false && "Unknown swap chain color space.");
			return COLOR_SPACE_MAX;
		}
	}

	void RenderingDeviceDriverVulkan::swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) {
		//TODO: SWAPPY_FRAME_PACING_ENABLED
		DEV_ASSERT(p_swap_chain.id != 0);
	}

	void RenderingDeviceDriverVulkan::swap_chain_free(SwapChainID p_swap_chain) {
		DEV_ASSERT(p_swap_chain.id != 0);

		SwapChain* swap_chain = (SwapChain*)(p_swap_chain.id);
		_swap_chain_release(swap_chain);

		delete swap_chain;
	}
#pragma endregion

#pragma region Framebuffer

	RenderingDeviceDriverVulkan::FramebufferID RenderingDeviceDriverVulkan::framebuffer_create(RenderPassID p_render_pass, std::span<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) {
		RenderPassInfo* render_pass = (RenderPassInfo*)(p_render_pass.id);

		uint32_t fragment_density_map_offsets_layers = 0;
		std::vector< VkImageView> vk_img_vec(p_attachments.size());
		VkImageView* vk_img_views = vk_img_vec.data();
		for (uint32_t i = 0; i < p_attachments.size(); i++) {
			const TextureInfo* texture = (const TextureInfo*)p_attachments[i].id;
			vk_img_views[i] = texture->vk_view;
		}

		VkFramebufferCreateInfo framebuffer_create_info = {};
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass->vk_render_pass;
		framebuffer_create_info.attachmentCount = p_attachments.size();
		framebuffer_create_info.pAttachments = vk_img_views;
		framebuffer_create_info.width = p_width;
		framebuffer_create_info.height = p_height;
		framebuffer_create_info.layers = 1;

		VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;
		VkResult err = vkCreateFramebuffer(vk_device, &framebuffer_create_info, nullptr, &vk_framebuffer);
		ERR_FAIL_COND_V_MSG(err, FramebufferID(), std::format("vkCreateFramebuffer failed with error {} .", std::to_string(err)));

#if PRINT_NATIVE_COMMANDS
		LOGI(std::format("vkCreateFramebuffer {} with {} attachments", uint64_t(vk_framebuffer), p_attachments.size()));
		for (uint32_t i = 0; i < p_attachments.size(); i++) {
			const TextureInfo* attachment_info = (const TextureInfo*)p_attachments[i].id;
			LOGI(std::format("  Attachment #{}: IMAGE {} VIEW {}", i, uint64_t(attachment_info->vk_view_create_info.image), uint64_t(attachment_info->vk_view)));
		}
#endif

		Framebuffer* framebuffer = new Framebuffer;
		framebuffer->vk_framebuffer = vk_framebuffer;
		framebuffer->fragment_density_map_offsets_layers = fragment_density_map_offsets_layers;
		return FramebufferID(framebuffer);
	}

	void RenderingDeviceDriverVulkan::framebuffer_free(FramebufferID p_framebuffer) {
		Framebuffer* framebuffer = (Framebuffer*)(p_framebuffer.id);
		vkDestroyFramebuffer(vk_device, framebuffer->vk_framebuffer, nullptr);
		delete framebuffer;
	}

#pragma endregion

#pragma region Shader

	static VkShaderStageFlagBits RD_STAGE_TO_VK_SHADER_STAGE_BITS[RenderingDeviceCommons::SHADER_STAGE_MAX] = {
	VK_SHADER_STAGE_VERTEX_BIT,
	VK_SHADER_STAGE_FRAGMENT_BIT,
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	VK_SHADER_STAGE_COMPUTE_BIT,
	VK_SHADER_STAGE_RAYGEN_BIT_KHR,
	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
	VK_SHADER_STAGE_MISS_BIT_KHR,
	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
	};

	RenderingDeviceDriverVulkan::ShaderID RenderingDeviceDriverVulkan::shader_create_from_container(const RenderingShaderContainer* p_shader_container, const std::vector<ImmutableSampler>& p_immutable_samplers) {
		ShaderReflection shader_refl = p_shader_container->get_shader_reflection();
		ShaderInfo shader_info;
		shader_info.name = p_shader_container->shader_name;

		for (uint32_t i = 0; i < SHADER_STAGE_MAX; i++) {
			if (shader_refl.push_constant_stages.has_flag((ShaderStage)(1 << i))) {
				shader_info.vk_push_constant_stages |= RD_STAGE_TO_VK_SHADER_STAGE_BITS[i];
			}
		}

		// Set bindings.
		std::vector<std::vector<VkDescriptorSetLayoutBinding>> vk_set_bindings;
		vk_set_bindings.resize(shader_refl.uniform_sets.size());
		for (uint32_t i = 0; i < shader_refl.uniform_sets.size(); i++) {
			for (uint32_t j = 0; j < shader_refl.uniform_sets[i].size(); j++) {
				const ShaderUniform& uniform = shader_refl.uniform_sets[i][j];
				VkDescriptorSetLayoutBinding layout_binding = {};
				layout_binding.binding = uniform.binding;
				layout_binding.descriptorCount = 1;
				for (uint32_t k = 0; k < SHADER_STAGE_MAX; k++) {
					if ((uniform.stages.has_flag(ShaderStage(1U << k)))) {
						layout_binding.stageFlags |= RD_STAGE_TO_VK_SHADER_STAGE_BITS[k];
					}
				}

				switch (uniform.type) {
				case UNIFORM_TYPE_SAMPLER: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
					layout_binding.descriptorCount = uniform.length;
					// Immutable samplers: here they get set in the layoutbinding, given that they will not be changed later.
					int immutable_bind_index = -1;
					if (immutable_samplers_enabled && p_immutable_samplers.size() > 0) {
						for (int k = 0; k < p_immutable_samplers.size(); k++) {
							if (p_immutable_samplers[k].binding == layout_binding.binding) {
								immutable_bind_index = k;
								break;
							}
						}
						if (immutable_bind_index >= 0) {
							layout_binding.pImmutableSamplers = (VkSampler*)&p_immutable_samplers[immutable_bind_index].ids[0].id;
						}
					}
				} break;
				case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					layout_binding.descriptorCount = uniform.length;
				} break;
				case UNIFORM_TYPE_TEXTURE: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					layout_binding.descriptorCount = uniform.length;
				} break;
				case UNIFORM_TYPE_IMAGE: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					layout_binding.descriptorCount = uniform.length;
				} break;
				case UNIFORM_TYPE_TEXTURE_BUFFER: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
					layout_binding.descriptorCount = uniform.length;
				} break;
				case UNIFORM_TYPE_IMAGE_BUFFER: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				} break;
				case UNIFORM_TYPE_UNIFORM_BUFFER: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				} break;
				case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				} break;
				case UNIFORM_TYPE_STORAGE_BUFFER: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				} break;
				case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				} break;
				case UNIFORM_TYPE_INPUT_ATTACHMENT: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				} break;
				case UNIFORM_TYPE_ACCELERATION_STRUCTURE: {
					layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				} break;
				default: {
					DEV_ASSERT(false);
				}
				}

				vk_set_bindings[i].push_back(layout_binding);
			}
		}

		// Modules.
		VkResult res;
		std::string error_text;
		std::vector<uint8_t> decompressed_code;
		VkShaderModule vk_module;
		PackedByteArray decoded_spirv;
		const bool use_respv = RESPV_ENABLED && !shader_container_format.get_debug_info_enabled();
		const bool store_respv = use_respv && !shader_refl.specialization_constants.empty();
		const int64_t stage_count = shader_refl.stages_vector.size();
		shader_info.vk_stages_create_info.reserve(stage_count);
		shader_info.original_stage_size.reserve(stage_count);

#if RECORD_PIPELINE_STATISTICS
		shader_info.spirv_stage_bytes.reserve(stage_count);
#endif

		if (store_respv) {
			shader_info.respv_stage_shaders.reserve(stage_count);
		}

		// AnyHit and ClosestHit go in the same group.
		uint32_t hit_group_index = UINT32_MAX;

		for (int i = 0; i < stage_count; i++) {
			const RenderingShaderContainer::Shader& shader = p_shader_container->shaders[i];
			bool requires_decompression = false;// (shader.code_decompressed_size > 0);
			// TODO: shader compression

			//if (requires_decompression) {
			//	decompressed_code.resize(shader.code_decompressed_size);
			//	bool decompressed = p_shader_container->decompress_code(shader.code_compressed_bytes.data(), shader.code_compressed_bytes.size(), shader.code_compression_flags, decompressed_code.data(), decompressed_code.size());
			//	if (!decompressed) {
			//		error_text = std::format("Failed to decompress code on shader stage %s.", std::string(SHADER_STAGE_NAMES[shader_refl.stages_vector[i]]));
			//		break;
			//	}
			//}

			const uint8_t* smolv_input = requires_decompression ? decompressed_code.data() : shader.code_compressed_bytes.data();
			uint32_t smolv_input_size = requires_decompression ? decompressed_code.size() : shader.code_compressed_bytes.size();
			//if (shader.code_compression_flags & RenderingShaderContainerVulkan::COMPRESSION_FLAG_SMOLV) {
			//	decoded_spirv.resize(smolv::GetDecodedBufferSize(smolv_input, smolv_input_size));
			//	if (decoded_spirv.is_empty()) {
			//		error_text = vformat("Malformed smolv input on shader stage %s.", String(SHADER_STAGE_NAMES[shader_refl.stages_vector[i]]));
			//		break;
			//	}

			//	if (!smolv::Decode(smolv_input, smolv_input_size, decoded_spirv.ptrw(), decoded_spirv.size())) {
			//		error_text = vformat("Malformed smolv input on shader stage %s.", String(SHADER_STAGE_NAMES[shader_refl.stages_vector[i]]));
			//		break;
			//	}
			//}
			//else {
				decoded_spirv.resize(smolv_input_size);
				memcpy(decoded_spirv.data(), smolv_input, decoded_spirv.size());
			//}

			shader_info.original_stage_size.push_back(decoded_spirv.size());

			if (use_respv) {
				const bool inline_data = store_respv || !RESPV_ONLY_INLINE_SHADERS_WITH_SPEC_CONSTANTS;
				respv::Shader respv_shader(decoded_spirv.data(), decoded_spirv.size(), inline_data);
				if (respv_shader.empty()) {
#if RESPV_VERBOSE
					print_line("re-spirv failed to parse the shader, skipping optimization.");
#endif
					if (store_respv) {
						shader_info.respv_stage_shaders.push_back(respv::Shader());
					}
				}
				else if (store_respv) {
					shader_info.respv_stage_shaders.push_back(respv_shader);
				}
				else {
					std::vector<uint8_t> respv_optimized_data;
					if (respv::Optimizer::run(respv_shader, nullptr, 0, respv_optimized_data)) {
#if RESPV_VERBOSE
						print_line(vformat("re-spirv transformed the shader from %d bytes to %d bytes.", decoded_spirv.size(), respv_optimized_data.size()));
#endif
						decoded_spirv.resize(respv_optimized_data.size());
						memcpy(decoded_spirv.data(), respv_optimized_data.data(), respv_optimized_data.size());
					}
					else {
#if RESPV_VERBOSE
						print_line("re-spirv failed to optimize the shader.");
#endif
					}
				}
			}

#if RECORD_PIPELINE_STATISTICS
			shader_info.spirv_stage_bytes.push_back(decoded_spirv);
#endif

			VkShaderModuleCreateInfo shader_module_create_info = {};
			shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shader_module_create_info.codeSize = decoded_spirv.size();
			shader_module_create_info.pCode = (const uint32_t*)(decoded_spirv.data());

			res = vkCreateShaderModule(vk_device, &shader_module_create_info, nullptr, &vk_module);
			if (res != VK_SUCCESS) {
				error_text = std::format("Error ({}) creating module for shader stage {}.", std::to_string(res), std::string(SHADER_STAGE_NAMES[shader_refl.stages_vector[i]]));
				break;
			}

			VkPipelineShaderStageCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			create_info.stage = RD_STAGE_TO_VK_SHADER_STAGE_BITS[shader_refl.stages_vector[i]];
			create_info.module = vk_module;
			create_info.pName = "main";
			shader_info.vk_stages_create_info.push_back(create_info);

			ShaderStage stage = shader_refl.stages_vector[i];

			if (stage == ShaderStage::SHADER_STAGE_RAYGEN || stage == ShaderStage::SHADER_STAGE_MISS) {
				VkRayTracingShaderGroupCreateInfoKHR group_info = {};
				group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
				group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
				group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
				group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
				group_info.generalShader = i;

				shader_info.vk_groups_create_info.push_back(group_info);
			}
			if (stage == ShaderStage::SHADER_STAGE_ANY_HIT || stage == ShaderStage::SHADER_STAGE_CLOSEST_HIT) {
				if (hit_group_index == UINT32_MAX) {
					VkRayTracingShaderGroupCreateInfoKHR group_info = {};
					group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
					group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
					group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
					group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
					group_info.intersectionShader = VK_SHADER_UNUSED_KHR;
					group_info.generalShader = VK_SHADER_UNUSED_KHR;

					hit_group_index = shader_info.vk_groups_create_info.size();
					shader_info.vk_groups_create_info.push_back(group_info);
				}

				VkRayTracingShaderGroupCreateInfoKHR& group_info = shader_info.vk_groups_create_info[hit_group_index];
				if (stage == ShaderStage::SHADER_STAGE_ANY_HIT) {
					group_info.anyHitShader = i;
				}
				else if (stage == ShaderStage::SHADER_STAGE_CLOSEST_HIT) {
					group_info.closestHitShader = i;
				}
			}
			if (stage == ShaderStage::SHADER_STAGE_INTERSECTION) {
				VkRayTracingShaderGroupCreateInfoKHR group_info = {};
				group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
				group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
				group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
				group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
				group_info.intersectionShader = i;
				group_info.generalShader = VK_SHADER_UNUSED_KHR;

				shader_info.vk_groups_create_info.push_back(group_info);
			}
		}

		// Descriptor sets.
		if (error_text.empty()) {
			// For Adreno 5XX driver bug.
			VkDescriptorSetLayoutBinding placeholder_binding = {};
			placeholder_binding.binding = 0;
			placeholder_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			placeholder_binding.descriptorCount = 1;
			placeholder_binding.stageFlags = VK_SHADER_STAGE_ALL;

			for (uint32_t i = 0; i < shader_refl.uniform_sets.size(); i++) {
				// Empty ones are fine if they were not used according to spec (binding count will be 0).
				VkDescriptorSetLayoutCreateInfo layout_create_info = {};
				layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layout_create_info.bindingCount = vk_set_bindings[i].size();
				layout_create_info.pBindings = vk_set_bindings[i].data();

				// ...not so fine on Adreno 5XX.
				//if (adreno_5xx_empty_descriptor_set_layout_workaround && layout_create_info.bindingCount == 0) {
				//	layout_create_info.bindingCount = 1;
				//	layout_create_info.pBindings = &placeholder_binding;
				//}

				VkDescriptorSetLayout layout = VK_NULL_HANDLE;
				res = vkCreateDescriptorSetLayout(vk_device, &layout_create_info, nullptr, &layout);
				if (res) {
					error_text = std::format("Error ({}) creating descriptor set layout for set {}.", std::to_string(res), i);
					break;
				}

				shader_info.vk_descriptor_set_layouts.push_back(layout);
			}
		}

		if (error_text.empty()) {
			// Pipeline layout.
			VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
			pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipeline_layout_create_info.setLayoutCount = shader_info.vk_descriptor_set_layouts.size();
			pipeline_layout_create_info.pSetLayouts = shader_info.vk_descriptor_set_layouts.data();

			if (shader_refl.push_constant_size > 0) {
				std::vector<VkPushConstantRange> push_constant_range_vec;
				VkPushConstantRange* push_constant_range = push_constant_range_vec.data();
				*push_constant_range = {};
				push_constant_range->stageFlags = shader_info.vk_push_constant_stages;
				push_constant_range->size = shader_refl.push_constant_size;
				pipeline_layout_create_info.pushConstantRangeCount = 1;
				pipeline_layout_create_info.pPushConstantRanges = push_constant_range;
			}

			res = vkCreatePipelineLayout(vk_device, &pipeline_layout_create_info, nullptr, &shader_info.vk_pipeline_layout);
			if (res != VK_SUCCESS) {
				error_text = std::format("Error ({}) creating pipeline layout.", std::to_string(res));
			}
		}

		if (!error_text.empty()) {
			// Clean up if failed.
			for (uint32_t i = 0; i < shader_info.vk_stages_create_info.size(); i++) {
				vkDestroyShaderModule(vk_device, shader_info.vk_stages_create_info[i].module, nullptr);
			}
			for (uint32_t i = 0; i < shader_info.vk_descriptor_set_layouts.size(); i++) {
				vkDestroyDescriptorSetLayout(vk_device, shader_info.vk_descriptor_set_layouts[i], nullptr);
			}

			ERR_FAIL_V_MSG(ShaderID(), error_text);
		}

		// TODO: Raytracing
		//if (shader_refl.pipeline_type == PIPELINE_TYPE_RAYTRACING) {
		//	// Regions

		//	for (ShaderStage stage : shader_refl.stages_vector) {
		//		switch (stage) {
		//		case ShaderStage::SHADER_STAGE_RAYGEN:
		//			shader_info.region_count.raygen_count += 1;
		//			break;
		//		case ShaderStage::SHADER_STAGE_ANY_HIT:
		//		case ShaderStage::SHADER_STAGE_CLOSEST_HIT:
		//			shader_info.region_count.hit_count += 1;
		//			break;
		//		case ShaderStage::SHADER_STAGE_MISS:
		//			shader_info.region_count.miss_count += 1;
		//			break;
		//		default:
		//			// nothing
		//			break;
		//		}
		//	}

		//	shader_info.region_count.group_count = shader_info.region_count.raygen_count + shader_info.region_count.hit_count + shader_info.region_count.miss_count;
		//}

		// Bookkeep.
		ShaderInfo* shader_info_ptr = new ShaderInfo;
		*shader_info_ptr = shader_info;
		return ShaderID(shader_info_ptr);
	}

	void RenderingDeviceDriverVulkan::shader_free(ShaderID p_shader) {
		ShaderInfo* shader_info = (ShaderInfo*)p_shader.id;

		for (uint32_t i = 0; i < shader_info->vk_descriptor_set_layouts.size(); i++) {
			vkDestroyDescriptorSetLayout(vk_device, shader_info->vk_descriptor_set_layouts[i], nullptr);
		}

		vkDestroyPipelineLayout(vk_device, shader_info->vk_pipeline_layout, nullptr);

		shader_destroy_modules(p_shader);

		delete shader_info;
	}

	void RenderingDeviceDriverVulkan::shader_destroy_modules(ShaderID p_shader) {
		ShaderInfo* si = (ShaderInfo*)p_shader.id;

		for (uint32_t i = 0; i < si->vk_stages_create_info.size(); i++) {
			if (si->vk_stages_create_info[i].module) {
				vkDestroyShaderModule(vk_device, si->vk_stages_create_info[i].module, nullptr);
				si->vk_stages_create_info[i].module = VK_NULL_HANDLE;
			}
		}
		si->vk_stages_create_info.clear();
	}

	bool RenderingDeviceDriverVulkan::has_feature(Features p_feature)
	{
		switch (p_feature) {
		case SUPPORTS_HALF_FLOAT:
			return false;// TODO: shader_capabilities.shader_float16_is_supported&& physical_device_features.shaderInt16&& storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported;
		case SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS:
			return true;
		case SUPPORTS_BUFFER_DEVICE_ADDRESS:
			return buffer_device_address_support;
		case SUPPORTS_IMAGE_ATOMIC_32_BIT:
			return true;
		case SUPPORTS_VULKAN_MEMORY_MODEL:
			return vulkan_memory_model_support && vulkan_memory_model_device_scope_support;
		case SUPPORTS_FRAMEBUFFER_DEPTH_RESOLVE:
			return framebuffer_depth_resolve;
		case SUPPORTS_POINT_SIZE:
			return true;
		//case SUPPORTS_RAY_QUERY:
		//	return acceleration_structure_capabilities.acceleration_structure_support && ray_query_support;
		//case SUPPORTS_RAYTRACING_PIPELINE:
		//	return acceleration_structure_capabilities.acceleration_structure_support && raytracing_capabilities.raytracing_pipeline_support;
		case SUPPORTS_HDR_OUTPUT:
#if defined(WINDOWS_ENABLED)
			// When using a Vulkan swapchain on Windows, some configurations
			// involving integrated GPU hardware do not function correctly
			// with HDR output.
			return false;
#else
			return context_driver->is_colorspace_supported();
#endif // defined(WINDOWS_ENABLED)
		default:
			return false;
		}
	}

	uint64_t RenderingDeviceDriverVulkan::limit_get(Limit p_limit)
	{
		const VkPhysicalDeviceLimits& limits = physical_device_properties.limits;
		uint64_t safe_unbounded = ((uint64_t)1 << 30);
		switch (p_limit)
		{
		case LIMIT_MAX_BOUND_UNIFORM_SETS:
			return limits.maxBoundDescriptorSets;
		case LIMIT_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
			return limits.maxColorAttachments;
		case LIMIT_MAX_TEXTURES_PER_UNIFORM_SET:
			return limits.maxDescriptorSetSampledImages;
		case LIMIT_MAX_SAMPLERS_PER_UNIFORM_SET:
			return limits.maxDescriptorSetSamplers;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET:
			return limits.maxDescriptorSetStorageBuffers;
		case LIMIT_MAX_STORAGE_IMAGES_PER_UNIFORM_SET:
			return limits.maxDescriptorSetStorageImages;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_UNIFORM_SET:
			return limits.maxDescriptorSetUniformBuffers;
		case LIMIT_MAX_DRAW_INDEXED_INDEX:
			return limits.maxDrawIndexedIndexValue;
		case LIMIT_MAX_FRAMEBUFFER_HEIGHT:
			return limits.maxFramebufferHeight;
		case LIMIT_MAX_FRAMEBUFFER_WIDTH:
			return limits.maxFramebufferWidth;
		case LIMIT_MAX_TEXTURE_ARRAY_LAYERS:
			return limits.maxImageArrayLayers;
		case LIMIT_MAX_TEXTURE_SIZE_1D:
			return limits.maxImageDimension1D;
		case LIMIT_MAX_TEXTURE_SIZE_2D:
			return limits.maxImageDimension2D;
		case LIMIT_MAX_TEXTURE_SIZE_3D:
			return limits.maxImageDimension3D;
		case LIMIT_MAX_TEXTURE_SIZE_CUBE:
			return limits.maxImageDimensionCube;
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return limits.maxPerStageDescriptorSampledImages;
		case LIMIT_MAX_SAMPLERS_PER_SHADER_STAGE:
			return limits.maxPerStageDescriptorSamplers;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE:
			return limits.maxPerStageDescriptorStorageBuffers;
		case LIMIT_MAX_STORAGE_IMAGES_PER_SHADER_STAGE:
			return limits.maxPerStageDescriptorStorageImages;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE:
			return limits.maxPerStageDescriptorUniformBuffers;
		case LIMIT_MAX_PUSH_CONSTANT_SIZE:
			return limits.maxPushConstantsSize;
		case LIMIT_MAX_UNIFORM_BUFFER_SIZE:
			return limits.maxUniformBufferRange;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTE_OFFSET:
			return limits.maxVertexInputAttributeOffset;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTES:
			return limits.maxVertexInputAttributes;
		case LIMIT_MAX_VERTEX_INPUT_BINDINGS:
			return limits.maxVertexInputBindings;
		case LIMIT_MAX_VERTEX_INPUT_BINDING_STRIDE:
			return limits.maxVertexInputBindingStride;
		case LIMIT_MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
			return limits.minUniformBufferOffsetAlignment;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X:
			return limits.maxComputeWorkGroupCount[0];
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y:
			return limits.maxComputeWorkGroupCount[1];
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z:
			return limits.maxComputeWorkGroupCount[2];
		case LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS:
			return limits.maxComputeWorkGroupInvocations;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X:
			return limits.maxComputeWorkGroupSize[0];
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y:
			return limits.maxComputeWorkGroupSize[1];
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z:
			return limits.maxComputeWorkGroupSize[2];
		case LIMIT_MAX_COMPUTE_SHARED_MEMORY_SIZE:
			return limits.maxComputeSharedMemorySize;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_X:
			return limits.maxViewportDimensions[0];
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_Y:
			return limits.maxViewportDimensions[1];
		case LIMIT_SUBGROUP_SIZE:
			//return subgroup_capabilities.size;
		case LIMIT_SUBGROUP_MIN_SIZE:
			//return subgroup_capabilities.min_size;
		case LIMIT_SUBGROUP_MAX_SIZE:
			//return subgroup_capabilities.max_size;
		case LIMIT_SUBGROUP_IN_SHADERS:
			//return subgroup_capabilities.supported_stages_flags_rd();
		case LIMIT_SUBGROUP_OPERATIONS:
			//return subgroup_capabilities.supported_operations_flags_rd();
		case LIMIT_MAX_SHADER_VARYINGS:
			// The Vulkan spec states that built in varyings like gl_FragCoord should count against this, but in
			// practice, that doesn't seem to be the case. The validation layers don't even complain.
			return MIN(limits.maxVertexOutputComponents / 4, limits.maxFragmentInputComponents / 4);
		default: {
#ifdef DEV_ENABLED
			WARN_PRINT("Returning maximum value for unknown limit " + itos(p_limit) + ".");
#endif
			return safe_unbounded;
		}
		}
	}

#pragma endregion

#pragma region Uniform Set

	VkDescriptorPool RenderingDeviceDriverVulkan::_descriptor_set_pool_create(const DescriptorSetPoolKey& p_key, bool p_linear_pool) {
		// Here comes more vulkan API strangeness.
		std::vector< VkDescriptorPoolSize> vk_sizes_vec(UNIFORM_TYPE_MAX);
		VkDescriptorPoolSize* vk_sizes = vk_sizes_vec.data();
		uint32_t vk_sizes_count = 0;
		{
			VkDescriptorPoolSize* curr_vk_size = vk_sizes;
			if (p_key.uniform_type[UNIFORM_TYPE_SAMPLER]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_SAMPLER;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_SAMPLER] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_SAMPLER_WITH_TEXTURE]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_SAMPLER_WITH_TEXTURE] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_TEXTURE]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_TEXTURE] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_IMAGE]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_IMAGE] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_TEXTURE_BUFFER] || p_key.uniform_type[UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				curr_vk_size->descriptorCount = (p_key.uniform_type[UNIFORM_TYPE_TEXTURE_BUFFER] + p_key.uniform_type[UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER]) * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_IMAGE_BUFFER]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_IMAGE_BUFFER] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_UNIFORM_BUFFER]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_UNIFORM_BUFFER] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_STORAGE_BUFFER]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_STORAGE_BUFFER] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_INPUT_ATTACHMENT]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_INPUT_ATTACHMENT] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			if (p_key.uniform_type[UNIFORM_TYPE_ACCELERATION_STRUCTURE]) {
				*curr_vk_size = {};
				curr_vk_size->type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				curr_vk_size->descriptorCount = p_key.uniform_type[UNIFORM_TYPE_ACCELERATION_STRUCTURE] * max_descriptor_sets_per_pool;
				curr_vk_size++;
				vk_sizes_count++;
			}
			DEV_ASSERT(vk_sizes_count <= UNIFORM_TYPE_MAX);
		}

		VkDescriptorPoolCreateInfo descriptor_set_pool_create_info = {};
		descriptor_set_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		if (linear_descriptor_pools_enabled && p_linear_pool) {
			descriptor_set_pool_create_info.flags = 0;
		}
		else {
			descriptor_set_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Can't think how somebody may NOT need this flag.
		}
		descriptor_set_pool_create_info.maxSets = max_descriptor_sets_per_pool;
		descriptor_set_pool_create_info.poolSizeCount = vk_sizes_count;
		descriptor_set_pool_create_info.pPoolSizes = vk_sizes;

		VkDescriptorPool vk_pool = VK_NULL_HANDLE;
		VkResult res = vkCreateDescriptorPool(vk_device, &descriptor_set_pool_create_info, nullptr, &vk_pool);
		if (res) {
			ERR_FAIL_COND_V_MSG(res, VK_NULL_HANDLE, std::format("vkCreateDescriptorPool failed with error {} .", std::to_string(res)));
		}

		return vk_pool;
	}

	void RenderingDeviceDriverVulkan::_descriptor_set_pool_unreference(DescriptorSetPools::iterator p_pool_sets_it, VkDescriptorPool p_vk_descriptor_pool, int p_linear_pool_index) {
		std::unordered_map<VkDescriptorPool, uint32_t>::iterator pool_rcs_it = p_pool_sets_it->second.find(p_vk_descriptor_pool);
		pool_rcs_it->second--;
		if (pool_rcs_it->second == 0) {
			vkDestroyDescriptorPool(vk_device, p_vk_descriptor_pool, nullptr);
			p_pool_sets_it->second.erase(p_vk_descriptor_pool);
			if (p_pool_sets_it->second.empty()) {
				if (linear_descriptor_pools_enabled && p_linear_pool_index >= 0) {
					linear_descriptor_set_pools[p_linear_pool_index].erase(p_pool_sets_it);
				}
				else {
					descriptor_set_pools.erase(p_pool_sets_it);
				}
			}
		}
	}


	RenderingDeviceDriverVulkan::UniformSetID RenderingDeviceDriverVulkan::uniform_set_create(std::span<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) {
		if (!linear_descriptor_pools_enabled) {
			p_linear_pool_index = -1;
		}
		DescriptorSetPoolKey pool_key;

		// We first gather dynamic arrays in a local array because TightLocalVector's
		// growth is not efficient when the number of elements is unknown.
		const BufferInfo* dynamic_buffers[MAX_DYNAMIC_BUFFERS];
		uint32_t num_dynamic_buffers = 0u;

		// Immutable samplers will be skipped so we need to track the number of vk_writes used.
		std::vector<VkWriteDescriptorSet> vk_writes_vec(p_uniforms.size());
		VkWriteDescriptorSet* vk_writes = vk_writes_vec.data();
		uint32_t writes_amount = 0;

		// TODO: i think we can get away with one vector of each type. 
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_buf_info_vec;
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_buf_info_vec_dynamic;
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_buf_info_vec_storage;
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_buf_info_vec_storage_dynamic;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec_sampler;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec_texture;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec_input_attachment;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec_texture_buffer;
		std::vector<std::unique_ptr<VkDescriptorImageInfo>> vk_img_infos_vec_texture_sampler;
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_texbuf_infos_vec;
		std::vector<std::unique_ptr<VkDescriptorBufferInfo>> vk_texbuf_infos_vec_sampler;
		std::vector<std::unique_ptr<VkBufferView>> vk_texbuf_views_vec;
		std::vector<std::unique_ptr<VkBufferView>> vk_texbuf_views_vec_sampler;
		for (uint32_t i = 0; i < p_uniforms.size(); i++) {
			const BoundUniform& uniform = p_uniforms[i];

			vk_writes[writes_amount] = {};
			vk_writes[writes_amount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

			bool add_write = true;
			uint32_t num_descriptors = 1;

			switch (uniform.type) {
			case UNIFORM_TYPE_SAMPLER: {
				num_descriptors = uniform.ids.size();

				if (uniform.immutable_sampler && immutable_samplers_enabled) {
					add_write = false;
				}
				else {

					for (uint32_t j = 0; j < num_descriptors; j++) {
						vk_img_infos_vec_sampler.emplace_back(std::make_unique<VkDescriptorImageInfo>());
						VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec_sampler.back().get();


						vk_img_infos[j] = {};
						vk_img_infos[j].sampler = (VkSampler)uniform.ids[j].id;
						vk_img_infos[j].imageView = VK_NULL_HANDLE;
						vk_img_infos[j].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					}

					vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
					vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec_sampler.end() - num_descriptors)->get();
				}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
				num_descriptors = uniform.ids.size() / 2;

				for (uint32_t j = 0; j < num_descriptors; j++) {

				vk_img_infos_vec_texture_sampler.emplace_back(std::make_unique<VkDescriptorImageInfo>());
				VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec_texture_sampler.back().get();


#ifdef DEBUG_ENABLED
					if (((const TextureInfo*)uniform.ids[j * 2 + 1].id)->transient) {
						ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT texture must not be used for sampling in a shader.");
					}
#endif
					vk_img_infos[j] = {};
					vk_img_infos[j].sampler = (VkSampler)uniform.ids[j * 2 + 0].id;
					vk_img_infos[j].imageView = ((const TextureInfo*)uniform.ids[j * 2 + 1].id)->vk_view;
					vk_img_infos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec_texture_sampler.end() - num_descriptors)->get();;
			} break;
			case UNIFORM_TYPE_TEXTURE: {
				num_descriptors = uniform.ids.size();

				for (uint32_t j = 0; j < num_descriptors; j++) {
				vk_img_infos_vec_texture.emplace_back(std::make_unique<VkDescriptorImageInfo>());
				VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec_texture.back().get();

#ifdef DEBUG_ENABLED
					if (((const TextureInfo*)uniform.ids[j].id)->transient) {
						ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT texture must not be used for sampling in a shader.");
					}
#endif
					//vk_img_infos[j] = {};
					vk_img_infos->imageView = ((const TextureInfo*)uniform.ids[j].id)->vk_view;
					vk_img_infos->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				DEBUG_ASSERT(vk_img_infos_vec_texture.size() >= num_descriptors);
				vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec_texture.end() - num_descriptors)->get();
			} break;
			case UNIFORM_TYPE_IMAGE: {
				num_descriptors = uniform.ids.size();

				for (uint32_t j = 0; j < num_descriptors; j++) {

				vk_img_infos_vec.emplace_back(std::make_unique<VkDescriptorImageInfo>()); ;

				VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec.back().get();
#ifdef DEBUG_ENABLED
					if (((const TextureInfo*)uniform.ids[j].id)->transient) {
						ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT texture must not be used for sampling in a shader.");
					}
#endif
					vk_img_infos[j] = {};
					vk_img_infos[j].imageView = ((const TextureInfo*)uniform.ids[j].id)->vk_view;
					vk_img_infos[j].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec.end() - num_descriptors)->get();
			} break;
			case UNIFORM_TYPE_TEXTURE_BUFFER: {
				num_descriptors = uniform.ids.size();

				for (uint32_t j = 0; j < num_descriptors; j++) {
					vk_texbuf_infos_vec.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
					VkDescriptorBufferInfo* vk_buf_infos = vk_texbuf_infos_vec.back().get();

					vk_texbuf_views_vec.emplace_back(std::make_unique<VkBufferView>());
					VkBufferView* vk_buf_views = vk_texbuf_views_vec.back().get();

					const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[j].id;
					vk_buf_infos[j] = {};
					vk_buf_infos[j].buffer = buf_info->vk_buffer;
					vk_buf_infos[j].range = buf_info->size;

					vk_buf_views[j] = buf_info->vk_view;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				vk_writes[writes_amount].pBufferInfo = (vk_texbuf_infos_vec.end() - num_descriptors)->get();
				vk_writes[writes_amount].pTexelBufferView = (vk_texbuf_views_vec.end() - num_descriptors)->get();
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
				num_descriptors = uniform.ids.size() / 2;
				for (uint32_t j = 0; j < num_descriptors; j++) {
					vk_texbuf_infos_vec_sampler.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
					VkDescriptorBufferInfo* vk_buf_infos = vk_texbuf_infos_vec_sampler.back().get();

					vk_texbuf_views_vec_sampler.emplace_back(std::make_unique<VkBufferView>());
					VkBufferView* vk_buf_views = vk_texbuf_views_vec_sampler.back().get();

					vk_img_infos_vec_texture_buffer.emplace_back(std::make_unique<VkDescriptorImageInfo>());
					VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec_texture_buffer.back().get();

					vk_img_infos[j] = {};
					vk_img_infos[j].sampler = (VkSampler)uniform.ids[j * 2 + 0].id;

					const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[j * 2 + 1].id;
					vk_buf_infos[j] = {};
					vk_buf_infos[j].buffer = buf_info->vk_buffer;
					vk_buf_infos[j].range = buf_info->size;

					vk_buf_views[j] = buf_info->vk_view;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec_texture_buffer.end() - num_descriptors)->get();
				vk_writes[writes_amount].pBufferInfo = (vk_texbuf_infos_vec_sampler.end() - num_descriptors)->get();
				vk_writes[writes_amount].pTexelBufferView = (vk_texbuf_views_vec_sampler.end() - num_descriptors)->get();
			} break;
			case UNIFORM_TYPE_IMAGE_BUFFER: {
				CRASH_NOW_MSG("Unimplemented!"); // TODO.
			} break;
			case UNIFORM_TYPE_UNIFORM_BUFFER: {
				const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[0].id;
				vk_buf_info_vec.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
				VkDescriptorBufferInfo* vk_buf_info = vk_buf_info_vec.back().get();

				vk_buf_info->buffer = buf_info->vk_buffer;
				vk_buf_info->range = buf_info->size;

				ERR_FAIL_COND_V_MSG(buf_info->is_dynamic(), UniformSetID(),
					std::format("Sent a buffer with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT but binding ( {} ), set ( {} ) is UNIFORM_TYPE_UNIFORM_BUFFER instead of UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC.", std::to_string(uniform.binding), std::to_string(p_set_index)));

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				vk_writes[writes_amount].pBufferInfo = vk_buf_info;
			} break;
			case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC: {
				const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[0].id;
				vk_buf_info_vec_dynamic.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
				VkDescriptorBufferInfo* vk_buf_info = vk_buf_info_vec_dynamic.back().get();
				vk_buf_info->buffer = buf_info->vk_buffer;
				vk_buf_info->range = buf_info->size;

				ERR_FAIL_COND_V_MSG(!buf_info->is_dynamic(), UniformSetID(),
					std::format("Sent a buffer without BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT but binding ( {} ), set ( {} ) is UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC instead of UNIFORM_TYPE_UNIFORM_BUFFER.", std::to_string(uniform.binding), std::to_string(p_set_index) ));
				ERR_FAIL_COND_V_MSG(num_dynamic_buffers >= MAX_DYNAMIC_BUFFERS, UniformSetID(),
					std::format("Uniform set exceeded the limit of dynamic/persistent buffers. ( {} ).", std::to_string(MAX_DYNAMIC_BUFFERS)));

				dynamic_buffers[num_dynamic_buffers++] = buf_info;
				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				vk_writes[writes_amount].pBufferInfo = vk_buf_info;
			} break;
			case UNIFORM_TYPE_STORAGE_BUFFER: {
				const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[0].id;
				vk_buf_info_vec_storage.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
				VkDescriptorBufferInfo* vk_buf_info = vk_buf_info_vec_storage.back().get();
				vk_buf_info->buffer = buf_info->vk_buffer;
				vk_buf_info->range = buf_info->size;

				ERR_FAIL_COND_V_MSG(buf_info->is_dynamic(), UniformSetID(),
					std::format("Sent a buffer with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT but binding ( {} ), set ( {} ) is UNIFORM_TYPE_STORAGE_BUFFER instead of UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC.", std::to_string(uniform.binding), std::to_string(p_set_index)));

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				vk_writes[writes_amount].pBufferInfo = vk_buf_info;
			} break;
			case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC: {
				const BufferInfo* buf_info = (const BufferInfo*)uniform.ids[0].id;
				vk_buf_info_vec_storage_dynamic.emplace_back(std::make_unique<VkDescriptorBufferInfo>());
				VkDescriptorBufferInfo* vk_buf_info = vk_buf_info_vec_storage_dynamic.back().get();

				vk_buf_info->buffer = buf_info->vk_buffer;
				vk_buf_info->range = buf_info->size;

				ERR_FAIL_COND_V_MSG(!buf_info->is_dynamic(), UniformSetID(),
					std::format("Sent a buffer without BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT but binding ( {} ), set ( {} ) is UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC instead of UNIFORM_TYPE_STORAGE_BUFFER.", std::to_string(uniform.binding), std::to_string(p_set_index)));
				ERR_FAIL_COND_V_MSG(num_dynamic_buffers >= MAX_DYNAMIC_BUFFERS, UniformSetID(),
					std::format("Uniform set exceeded the limit of dynamic/persistent buffers. ( {} )", std::to_string(MAX_DYNAMIC_BUFFERS)));

				dynamic_buffers[num_dynamic_buffers++] = buf_info;
				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
				vk_writes[writes_amount].pBufferInfo = vk_buf_info;
			} break;
			case UNIFORM_TYPE_INPUT_ATTACHMENT: {
				num_descriptors = uniform.ids.size();
				
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
				vk_img_infos_vec_input_attachment.emplace_back(std::make_unique<VkDescriptorImageInfo>());

					VkDescriptorImageInfo* vk_img_infos = vk_img_infos_vec_input_attachment.back().get();
					vk_img_infos[j] = {};
					vk_img_infos[j].imageView = ((const TextureInfo*)uniform.ids[j].id)->vk_view;
					vk_img_infos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}

				vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				vk_writes[writes_amount].pImageInfo = (vk_img_infos_vec_input_attachment.end() - num_descriptors)->get();
			} break;
			//case UNIFORM_TYPE_ACCELERATION_STRUCTURE: {
			//	const AccelerationStructureInfo* accel_info = (const AccelerationStructureInfo*)uniform.ids[0].id;
			//	VkWriteDescriptorSetAccelerationStructureKHR* acceleration_structure_write = ALLOCA_SINGLE(VkWriteDescriptorSetAccelerationStructureKHR);
			//	*acceleration_structure_write = {};
			//	acceleration_structure_write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			//	acceleration_structure_write->accelerationStructureCount = 1;
			//	acceleration_structure_write->pAccelerationStructures = &accel_info->vk_acceleration_structure;

			//	vk_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			//	vk_writes[i].pNext = acceleration_structure_write;
			//} break;
			default: {
				DEV_ASSERT(false);
			}
			}

			if (add_write) {
				vk_writes[writes_amount].dstBinding = uniform.binding;
				vk_writes[writes_amount].descriptorCount = num_descriptors;
				writes_amount++;
			}

			ERR_FAIL_COND_V_MSG(pool_key.uniform_type[uniform.type] == MAX_UNIFORM_POOL_ELEMENT, UniformSetID(), std::format("Uniform set reached the limit of bindings for the same type ( {} )", std::to_string(MAX_UNIFORM_POOL_ELEMENT)));
			pool_key.uniform_type[uniform.type] += num_descriptors;
		}

		bool linear_pool = p_linear_pool_index >= 0;
		auto& pool = linear_pool
			? linear_descriptor_set_pools[p_linear_pool_index]
			: descriptor_set_pools;

		auto pool_sets_it = pool.find(pool_key);

		if (pool_sets_it == pool.end()) {
			if (linear_pool) {
				pool_sets_it = linear_descriptor_set_pools[p_linear_pool_index].insert({ pool_key, std::unordered_map<VkDescriptorPool, uint32_t>() }).first;
			}
			else {
				pool_sets_it = descriptor_set_pools.insert({ pool_key, std::unordered_map<VkDescriptorPool, uint32_t>() }).first;
			}
		}

		VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
		descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocate_info.descriptorSetCount = 1;
		const ShaderInfo* shader_info = (const ShaderInfo*)p_shader.id;
		descriptor_set_allocate_info.pSetLayouts = &shader_info->vk_descriptor_set_layouts[p_set_index];

		VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
		for (std::pair<VkDescriptorPool, uint32_t> E : pool_sets_it->second) {
			if (E.second < max_descriptor_sets_per_pool) {
				descriptor_set_allocate_info.descriptorPool = E.first;
				VkResult res = vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &vk_descriptor_set);

				// Break early on success.
				if (res == VK_SUCCESS) {
					break;
				}

				// "Fragmented pool" and "out of memory pool" errors are handled by creating more pools. Any other error is unexpected.
				if (res != VK_ERROR_FRAGMENTED_POOL && res != VK_ERROR_OUT_OF_POOL_MEMORY) {
					ERR_FAIL_V_MSG(UniformSetID(), std::format("Cannot allocate descriptor sets, error {} .", std::to_string(res)));
				}
			}
		}

		// Create a new pool when no allocations could be made from the existing pools.
		if (vk_descriptor_set == VK_NULL_HANDLE) {
			descriptor_set_allocate_info.descriptorPool = _descriptor_set_pool_create(pool_key, linear_pool);
			VkResult res = vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &vk_descriptor_set);

			// All errors are unexpected at this stage.
			if (res) {
				vkDestroyDescriptorPool(vk_device, descriptor_set_allocate_info.descriptorPool, nullptr);
				ERR_FAIL_V_MSG(UniformSetID(),std::format("Cannot allocate descriptor sets, error {} .", std::to_string(res)));
			}
		}

		DEV_ASSERT(descriptor_set_allocate_info.descriptorPool != VK_NULL_HANDLE && vk_descriptor_set != VK_NULL_HANDLE);
		pool_sets_it->second[descriptor_set_allocate_info.descriptorPool]++;

		for (uint32_t i = 0; i < writes_amount; i++) {
			vk_writes[i].dstSet = vk_descriptor_set;
		}
		vkUpdateDescriptorSets(vk_device, writes_amount, vk_writes, 0, nullptr);

		// Bookkeep.

		UniformSetInfo* usi = new UniformSetInfo();
		usi->vk_descriptor_set = vk_descriptor_set;
		if (p_linear_pool_index >= 0) {
			usi->vk_linear_descriptor_pool = descriptor_set_allocate_info.descriptorPool;
		}
		else {
			usi->vk_descriptor_pool = descriptor_set_allocate_info.descriptorPool;
		}
		usi->pool_sets_it = pool_sets_it;
		usi->dynamic_buffers.resize(num_dynamic_buffers);
		for (uint32_t i = 0u; i < num_dynamic_buffers; ++i) {
			usi->dynamic_buffers[i] = dynamic_buffers[i];
		}

		return UniformSetID(usi);
	}

	void RenderingDeviceDriverVulkan::uniform_set_free(UniformSetID p_uniform_set) {
		UniformSetInfo* usi = (UniformSetInfo*)p_uniform_set.id;

		if (usi->vk_linear_descriptor_pool) {
			// Nothing to do. All sets are freed at once using vkResetDescriptorPool.
			//
			// We can NOT decrease the reference count (i.e. call _descriptor_set_pool_unreference())
			// because the pool is linear (i.e. the freed set can't be recycled) and further calls to
			// _descriptor_set_pool_find_or_create() need usi->pool_sets_it->value to stay so that we can
			// tell if the pool has ran out of space and we need to create a new pool.
		}
		else {
			vkFreeDescriptorSets(vk_device, usi->vk_descriptor_pool, 1, &usi->vk_descriptor_set);
			_descriptor_set_pool_unreference(usi->pool_sets_it, usi->vk_descriptor_pool, -1);
		}

		delete usi;
	}

	bool RenderingDeviceDriverVulkan::uniform_sets_have_linear_pools() const {
		return true;
	}

	uint32_t RenderingDeviceDriverVulkan::uniform_sets_get_dynamic_offsets(std::span<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) const {
		uint32_t mask = 0u;
		uint32_t shift = 0u;
#ifdef DEV_ENABLED
		uint32_t curr_dynamic_offset = 0u;
#endif

		for (uint32_t i = 0; i < p_set_count; i++) {
			const UniformSetInfo* usi = (const UniformSetInfo*)p_uniform_sets[i].id;
			// At this point this assert should already have been validated.
			DEV_ASSERT(curr_dynamic_offset + usi->dynamic_buffers.size() <= MAX_DYNAMIC_BUFFERS);

			for (const BufferInfo* dynamic_buffer : usi->dynamic_buffers) {
				DEV_ASSERT(dynamic_buffer->frame_idx < 16u);
				mask |= dynamic_buffer->frame_idx << shift;
				shift += 4u;
			}
#ifdef DEV_ENABLED
			curr_dynamic_offset += usi->dynamic_buffers.size();
#endif
		}

		return mask;
	}

	void RenderingDeviceDriverVulkan::linear_uniform_set_pools_reset(int p_linear_pool_index) {
		if (linear_descriptor_pools_enabled) {
			DescriptorSetPools& pools_to_reset = linear_descriptor_set_pools[p_linear_pool_index];
			DescriptorSetPools::iterator curr_pool = pools_to_reset.begin();

			while (curr_pool != pools_to_reset.end()) {
				std::unordered_map<VkDescriptorPool, uint32_t>::iterator curr_pair = curr_pool->second.begin();
				while (curr_pair != curr_pool->second.end()) {
					vkResetDescriptorPool(vk_device, curr_pair->first, 0);
					curr_pair->second = 0;
					++curr_pair;
				}
				++curr_pool;
			}
		}
	}

	// ----- COMMANDS -----

	void RenderingDeviceDriverVulkan::command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	}
#pragma endregion

#pragma region Transfer

	static void _texture_subresource_range_to_vk(const RenderingDeviceDriverVulkan::TextureSubresourceRange& p_subresources, VkImageSubresourceRange* r_vk_subreources) {
		*r_vk_subreources = {};
		r_vk_subreources->aspectMask = (VkImageAspectFlags)p_subresources.aspect;
		r_vk_subreources->baseMipLevel = p_subresources.base_mipmap;
		r_vk_subreources->levelCount = p_subresources.mipmap_count;
		r_vk_subreources->baseArrayLayer = p_subresources.base_layer;
		r_vk_subreources->layerCount = p_subresources.layer_count;
	}

	static void _texture_subresource_layers_to_vk(const RenderingDeviceDriverVulkan::TextureSubresourceLayers& p_subresources, VkImageSubresourceLayers* r_vk_subreources) {
		*r_vk_subreources = {};
		r_vk_subreources->aspectMask = (VkImageAspectFlags)p_subresources.aspect;
		r_vk_subreources->mipLevel = p_subresources.mipmap;
		r_vk_subreources->baseArrayLayer = p_subresources.base_layer;
		r_vk_subreources->layerCount = p_subresources.layer_count;
	}

	static void _buffer_texture_copy_region_to_vk(const RenderingDeviceDriverVulkan::BufferTextureCopyRegion& p_copy_region, uint32_t p_buffer_row_length, VkBufferImageCopy* r_vk_copy_region) {
		*r_vk_copy_region = {};
		r_vk_copy_region->bufferOffset = p_copy_region.buffer_offset;
		r_vk_copy_region->bufferRowLength = p_buffer_row_length;
		r_vk_copy_region->imageSubresource.aspectMask = (VkImageAspectFlags)(1 << p_copy_region.texture_subresource.aspect);
		r_vk_copy_region->imageSubresource.mipLevel = p_copy_region.texture_subresource.mipmap;
		r_vk_copy_region->imageSubresource.baseArrayLayer = p_copy_region.texture_subresource.layer;
		r_vk_copy_region->imageSubresource.layerCount = 1;
		r_vk_copy_region->imageOffset.x = p_copy_region.texture_offset.x;
		r_vk_copy_region->imageOffset.y = p_copy_region.texture_offset.y;
		r_vk_copy_region->imageOffset.z = p_copy_region.texture_offset.z;
		r_vk_copy_region->imageExtent.width = p_copy_region.texture_region_size.x;
		r_vk_copy_region->imageExtent.height = p_copy_region.texture_region_size.y;
		r_vk_copy_region->imageExtent.depth = p_copy_region.texture_region_size.z;
	}

	static void _texture_copy_region_to_vk(const RenderingDeviceDriverVulkan::TextureCopyRegion& p_copy_region, VkImageCopy* r_vk_copy_region) {
		*r_vk_copy_region = {};
		_texture_subresource_layers_to_vk(p_copy_region.src_subresources, &r_vk_copy_region->srcSubresource);
		r_vk_copy_region->srcOffset.x = p_copy_region.src_offset.x;
		r_vk_copy_region->srcOffset.y = p_copy_region.src_offset.y;
		r_vk_copy_region->srcOffset.z = p_copy_region.src_offset.z;
		_texture_subresource_layers_to_vk(p_copy_region.dst_subresources, &r_vk_copy_region->dstSubresource);
		r_vk_copy_region->dstOffset.x = p_copy_region.dst_offset.x;
		r_vk_copy_region->dstOffset.y = p_copy_region.dst_offset.y;
		r_vk_copy_region->dstOffset.z = p_copy_region.dst_offset.z;
		r_vk_copy_region->extent.width = p_copy_region.size.x;
		r_vk_copy_region->extent.height = p_copy_region.size.y;
		r_vk_copy_region->extent.depth = p_copy_region.size.z;
	}

	void RenderingDeviceDriverVulkan::command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		vkCmdFillBuffer(command_buffer->vk_command_buffer, buf_info->vk_buffer, p_offset, p_size, 0);
	}

	void RenderingDeviceDriverVulkan::command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, std::span<BufferCopyRegion> p_regions)   {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* src_buf_info = (const BufferInfo*)p_src_buffer.id;
		const BufferInfo* dst_buf_info = (const BufferInfo*)p_dst_buffer.id;
		vkCmdCopyBuffer(command_buffer->vk_command_buffer, src_buf_info->vk_buffer, dst_buf_info->vk_buffer, p_regions.size(), (const VkBufferCopy*)p_regions.data());
	}

	void RenderingDeviceDriverVulkan::command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, std::span<TextureCopyRegion> p_regions) {
		std::vector<VkImageCopy> vk_copy_regions_vec(p_regions.size());
		VkImageCopy* vk_copy_regions = vk_copy_regions_vec.data();
		for (uint32_t i = 0; i < p_regions.size(); i++) {
			_texture_copy_region_to_vk(p_regions[i], &vk_copy_regions[i]);
		}

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const TextureInfo* src_tex_info = (const TextureInfo*)p_src_texture.id;
		const TextureInfo* dst_tex_info = (const TextureInfo*)p_dst_texture.id;

#ifdef DEBUG_ENABLED
		if (src_tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_src_texture must not be used in command_copy_texture.");
		}
		if (dst_tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_dst_texture must not be used in command_copy_texture.");
		}
#endif

		vkCmdCopyImage(command_buffer->vk_command_buffer, src_tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_src_texture_layout], dst_tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_dst_texture_layout], p_regions.size(), vk_copy_regions);
	}

	void RenderingDeviceDriverVulkan::command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const TextureInfo* src_tex_info = (const TextureInfo*)p_src_texture.id;
		const TextureInfo* dst_tex_info = (const TextureInfo*)p_dst_texture.id;

		VkImageResolve vk_resolve = {};
		vk_resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vk_resolve.srcSubresource.mipLevel = p_src_mipmap;
		vk_resolve.srcSubresource.baseArrayLayer = p_src_layer;
		vk_resolve.srcSubresource.layerCount = 1;
		vk_resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vk_resolve.dstSubresource.mipLevel = p_dst_mipmap;
		vk_resolve.dstSubresource.baseArrayLayer = p_dst_layer;
		vk_resolve.dstSubresource.layerCount = 1;
		vk_resolve.extent.width = MAX(1u, src_tex_info->vk_create_info.extent.width >> p_src_mipmap);
		vk_resolve.extent.height = MAX(1u, src_tex_info->vk_create_info.extent.height >> p_src_mipmap);
		vk_resolve.extent.depth = MAX(1u, src_tex_info->vk_create_info.extent.depth >> p_src_mipmap);

#ifdef DEBUG_ENABLED
		if (src_tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_src_texture must not be used in command_resolve_texture. Use a resolve store action pass instead.");
		}
		if (dst_tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_dst_texture must not be used in command_resolve_texture.");
		}
#endif

		vkCmdResolveImage(command_buffer->vk_command_buffer, src_tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_src_texture_layout], dst_tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_dst_texture_layout], 1, &vk_resolve);
	}

	void RenderingDeviceDriverVulkan::command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color& p_color, const TextureSubresourceRange& p_subresources) {
		VkClearColorValue vk_color = {};
		memcpy(&vk_color.float32, glm::value_ptr(p_color), sizeof(VkClearColorValue::float32));

		VkImageSubresourceRange vk_subresources = {};
		_texture_subresource_range_to_vk(p_subresources, &vk_subresources);

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const TextureInfo* tex_info = (const TextureInfo*)p_texture.id;
#ifdef DEBUG_ENABLED
		if (tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_texture must not be used in command_clear_color_texture. Use a clear store action pass instead.");
		}
#endif
		vkCmdClearColorImage(command_buffer->vk_command_buffer, tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_texture_layout], &vk_color, 1, &vk_subresources);
	}

	void RenderingDeviceDriverVulkan::command_clear_depth_stencil_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, float p_depth, uint8_t p_stencil, const TextureSubresourceRange& p_subresources) {
		VkClearDepthStencilValue vk_depth_stencil = {};
		vk_depth_stencil.depth = p_depth;
		vk_depth_stencil.stencil = p_stencil;

		VkImageSubresourceRange vk_subresources = {};
		_texture_subresource_range_to_vk(p_subresources, &vk_subresources);

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const TextureInfo* tex_info = (const TextureInfo*)p_texture.id;
#ifdef DEBUG_ENABLED
		if (tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_texture must not be used in command_clear_depth_stencil_texture. Use a clear store action pass instead.");
		}
#endif
		vkCmdClearDepthStencilImage(command_buffer->vk_command_buffer, tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_texture_layout], &vk_depth_stencil, 1, &vk_subresources);
	}

	void RenderingDeviceDriverVulkan::command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, std::span<BufferTextureCopyRegion> p_regions) {
		const TextureInfo* tex_info = (const TextureInfo*)p_dst_texture.id;

		uint32_t pixel_size = get_image_format_pixel_size(tex_info->rd_format);
		uint32_t block_size = get_compressed_image_format_block_byte_size(tex_info->rd_format);
		uint32_t block_w, block_h;
		get_compressed_image_format_block_dimensions(tex_info->rd_format, block_w, block_h);

		std::vector<VkBufferImageCopy> vk_copy_regions_vec(p_regions.size());
		VkBufferImageCopy* vk_copy_regions = vk_copy_regions_vec.data();
		for (uint32_t i = 0; i < p_regions.size(); i++) {
			_buffer_texture_copy_region_to_vk(p_regions[i], p_regions[i].row_pitch * block_w / (pixel_size * block_size), &vk_copy_regions[i]);
		}

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_src_buffer.id;
#ifdef DEBUG_ENABLED
		if (tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_dst_texture must not be used in command_copy_buffer_to_texture.");
		}
#endif
		vkCmdCopyBufferToImage(command_buffer->vk_command_buffer, buf_info->vk_buffer, tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_dst_texture_layout], p_regions.size(), vk_copy_regions);
	}

	void RenderingDeviceDriverVulkan::command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, std::span<BufferTextureCopyRegion> p_regions) {
		const TextureInfo* tex_info = (const TextureInfo*)p_src_texture.id;

		uint32_t pixel_size = get_image_format_pixel_size(tex_info->rd_format);
		uint32_t block_size = get_compressed_image_format_block_byte_size(tex_info->rd_format);
		uint32_t block_w, block_h;
		get_compressed_image_format_block_dimensions(tex_info->rd_format, block_w, block_h);

		std::vector<VkBufferImageCopy> vk_copy_regions_vec(p_regions.size());
		VkBufferImageCopy* vk_copy_regions = vk_copy_regions_vec.data();
		for (uint32_t i = 0; i < p_regions.size(); i++) {
			_buffer_texture_copy_region_to_vk(p_regions[i], p_regions[i].row_pitch * block_w / (pixel_size * block_size), &vk_copy_regions[i]);
		}

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_dst_buffer.id;
#ifdef DEBUG_ENABLED
		if (tex_info->transient) {
			ERR_PRINT("TEXTURE_USAGE_TRANSIENT_BIT p_src_texture must not be used in command_copy_texture_to_buffer.");
		}
#endif
		vkCmdCopyImageToBuffer(command_buffer->vk_command_buffer, tex_info->vk_view_create_info.image, RD_TO_VK_LAYOUT[p_src_texture_layout], buf_info->vk_buffer, p_regions.size(), vk_copy_regions);
	}

#pragma endregion

#pragma region Pipeline

	void RenderingDeviceDriverVulkan::pipeline_free(PipelineID p_pipeline) {
		vkDestroyPipeline(vk_device, (VkPipeline)p_pipeline.id, nullptr);
	}

	// ----- BINDING -----

	void RenderingDeviceDriverVulkan::command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, std::span<uint32_t> p_data) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const ShaderInfo* shader_info = (const ShaderInfo*)p_shader.id;
		vkCmdPushConstants(command_buffer->vk_command_buffer, shader_info->vk_pipeline_layout, shader_info->vk_push_constant_stages, p_dst_first_index * sizeof(uint32_t), p_data.size() * sizeof(uint32_t), p_data.data());
	}

	// ----- CACHE -----

	int RenderingDeviceDriverVulkan::caching_instance_count = 0;


	bool RenderingDeviceDriverVulkan::pipeline_cache_create(const std::vector<uint8_t>& p_data) {
		if (caching_instance_count) {
			WARN_PRINT("There's already a RenderingDeviceDriverVulkan instance doing PSO caching. Only one can at the same time. This one won't.");
			return false;
		}
		caching_instance_count++;

		pipelines_cache.current_size = 0;
		pipelines_cache.buffer.resize(sizeof(PipelineCacheHeader));

		// Parse.
		{
			if (p_data.empty()) {
				// No pre-existing cache, just create it.
			}
			else if (p_data.size() <= (int)sizeof(PipelineCacheHeader)) {
				LOGW("Invalid/corrupt Vulkan pipelines cache. Existing shader pipeline cache will be ignored, which may result in stuttering during gameplay.");
			}
			else {
				const PipelineCacheHeader* loaded_header = reinterpret_cast<const PipelineCacheHeader*>(p_data.data());
				if (loaded_header->magic != 868 + VK_PIPELINE_CACHE_HEADER_VERSION_ONE) {
					LOGW("Invalid Vulkan pipelines cache magic number. Existing shader pipeline cache will be ignored, which may result in stuttering during gameplay.");
				}
				else {
					const uint8_t* loaded_buffer_start = p_data.data() + sizeof(PipelineCacheHeader);		// skips the header
					uint32_t loaded_buffer_size = p_data.size() - sizeof(PipelineCacheHeader);
					const PipelineCacheHeader* current_header = (PipelineCacheHeader*)pipelines_cache.buffer.data();
					if (loaded_header->data_hash != XXH32(loaded_buffer_start, loaded_buffer_size, 0) ||
						loaded_header->data_size != loaded_buffer_size ||
						loaded_header->vendor_id != current_header->vendor_id ||
						loaded_header->device_id != current_header->device_id ||
						loaded_header->driver_version != current_header->driver_version ||
						memcmp(loaded_header->uuid, current_header->uuid, VK_UUID_SIZE) != 0 ||
						loaded_header->driver_abi != current_header->driver_abi) {
						LOGW("Invalid Vulkan pipelines cache header. This may be due to an engine change, GPU change or graphics driver version change. Existing shader pipeline cache will be ignored, which may result in stuttering during gameplay.");
					}
					else {
						pipelines_cache.current_size = loaded_buffer_size;
						pipelines_cache.buffer = p_data;
					}
				}
			}
		}

		// Create.
		{
			VkPipelineCacheCreateInfo cache_info = {};
			cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			cache_info.initialDataSize = pipelines_cache.buffer.size() - sizeof(PipelineCacheHeader);
			cache_info.pInitialData = pipelines_cache.buffer.data() + sizeof(PipelineCacheHeader);

			VkResult err = vkCreatePipelineCache(vk_device, &cache_info, nullptr, &pipelines_cache.vk_cache);
			if (err != VK_SUCCESS) {
				LOGW("vkCreatePipelinecache failed with error ", std::to_string(err));
				return false;
			}
		}

		return true;
	}

	void RenderingDeviceDriverVulkan::pipeline_cache_free() {
		DEV_ASSERT(pipelines_cache.vk_cache);

		vkDestroyPipelineCache(vk_device, pipelines_cache.vk_cache, nullptr);
		pipelines_cache.vk_cache = VK_NULL_HANDLE;

		DEV_ASSERT(caching_instance_count > 0);
		caching_instance_count--;
	}

	size_t RenderingDeviceDriverVulkan::pipeline_cache_query_size() {
		DEV_ASSERT(pipelines_cache.vk_cache);

		// FIXME:
		// We're letting the cache grow unboundedly. We may want to set at limit and see if implementations use LRU or the like.
		// If we do, we won't be able to assume any longer that the cache is dirty if, and only if, it has grown.
		VkResult err = vkGetPipelineCacheData(vk_device, pipelines_cache.vk_cache, &pipelines_cache.current_size, nullptr);
		ERR_FAIL_COND_V_MSG(err, 0, std::format("vkGetPipelineCacheData failed with error ", std::to_string(err)));

		return pipelines_cache.current_size;
	}

	std::vector<uint8_t> RenderingDeviceDriverVulkan::pipeline_cache_serialize() {
		DEV_ASSERT(pipelines_cache.vk_cache);

		pipelines_cache.buffer.resize(pipelines_cache.current_size + sizeof(PipelineCacheHeader));

		VkResult err = vkGetPipelineCacheData(vk_device, pipelines_cache.vk_cache, &pipelines_cache.current_size, pipelines_cache.buffer.data() + sizeof(PipelineCacheHeader));
		ERR_FAIL_COND_V(err != VK_SUCCESS && err != VK_INCOMPLETE, std::vector<uint8_t>()); // Incomplete is OK because the cache may have grown since the size was queried (unless when exiting).

		// The real buffer size may now be bigger than the updated current_size.
		// We take into account the new size but keep the buffer resized in a worst-case fashion.

		PipelineCacheHeader* header = (PipelineCacheHeader*)pipelines_cache.buffer.data();
		header->data_size = pipelines_cache.current_size;
		header->data_hash = XXH32(pipelines_cache.buffer.data() + sizeof(PipelineCacheHeader), pipelines_cache.current_size, 0);

		return pipelines_cache.buffer;
	}

#pragma endregion

#pragma region Rendering

	// ----- SUBPASS -----

	static void _attachment_reference_to_vk(const RenderingDeviceDriverVulkan::AttachmentReference& p_attachment_reference, VkAttachmentReference2KHR* r_vk_attachment_reference) {
		*r_vk_attachment_reference = {};
		r_vk_attachment_reference->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		r_vk_attachment_reference->attachment = p_attachment_reference.attachment;
		r_vk_attachment_reference->layout = RD_TO_VK_LAYOUT[p_attachment_reference.layout];
		r_vk_attachment_reference->aspectMask = (VkImageAspectFlags)p_attachment_reference.aspect;
	}

	RenderingDeviceDriverVulkan::RenderPassID RenderingDeviceDriverVulkan::render_pass_create(std::span<Attachment> p_attachments, std::span<Subpass> p_subpasses, std::span<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment) {
		// These are only used if we use multiview but we need to define them in scope.
		const uint32_t view_mask = (1 << p_view_count) - 1;
		const uint32_t correlation_mask = (1 << p_view_count) - 1;

		std::vector<VkAttachmentDescription2KHR> vk_attachments_vec(p_attachments.size());
		VkAttachmentDescription2KHR* vk_attachments = vk_attachments_vec.data();
		for (uint32_t i = 0; i < p_attachments.size(); i++) {
			vk_attachments[i] = {};
			vk_attachments[i].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
			vk_attachments[i].format = RD_TO_VK_FORMAT[p_attachments[i].format];
			vk_attachments[i].samples = _ensure_supported_sample_count(p_attachments[i].samples);
			vk_attachments[i].loadOp = (VkAttachmentLoadOp)p_attachments[i].load_op;
			vk_attachments[i].storeOp = (VkAttachmentStoreOp)p_attachments[i].store_op;
			vk_attachments[i].stencilLoadOp = (VkAttachmentLoadOp)p_attachments[i].stencil_load_op;
			vk_attachments[i].stencilStoreOp = (VkAttachmentStoreOp)p_attachments[i].stencil_store_op;
			vk_attachments[i].initialLayout = RD_TO_VK_LAYOUT[p_attachments[i].initial_layout];
			vk_attachments[i].finalLayout = RD_TO_VK_LAYOUT[p_attachments[i].final_layout];
		}
		std::vector<VkAttachmentReference2KHR> vk_subpass_input_attachments_vec;
		std::vector<VkAttachmentReference2KHR> vk_subpass_color_attachments_vec;
		std::vector<VkAttachmentReference2KHR> vk_subpass_resolve_attachments_vec;
		std::vector<VkAttachmentReference2KHR> vk_subpass_depth_stencil_attachment_vec;

		std::vector<VkSubpassDescription2KHR> vk_subpasses_vec(p_subpasses.size());
		VkSubpassDescription2KHR* vk_subpasses = vk_subpasses_vec.data();
		for (uint32_t i = 0; i < p_subpasses.size(); i++) {

			vk_subpass_input_attachments_vec.resize(p_subpasses[i].input_references.size());
			VkAttachmentReference2KHR* vk_subpass_input_attachments = vk_subpass_input_attachments_vec.data();
			for (uint32_t j = 0; j < p_subpasses[i].input_references.size(); j++) {
				_attachment_reference_to_vk(p_subpasses[i].input_references[j], &vk_subpass_input_attachments[j]);
			}

			vk_subpass_color_attachments_vec.resize(p_subpasses[i].color_references.size());
			VkAttachmentReference2KHR* vk_subpass_color_attachments = vk_subpass_color_attachments_vec.data();
			for (uint32_t j = 0; j < p_subpasses[i].color_references.size(); j++) {
				_attachment_reference_to_vk(p_subpasses[i].color_references[j], &vk_subpass_color_attachments[j]);
			}

			vk_subpass_resolve_attachments_vec.resize(p_subpasses[i].resolve_references.size());
			VkAttachmentReference2KHR* vk_subpass_resolve_attachments = vk_subpass_resolve_attachments_vec.data();
			for (uint32_t j = 0; j < p_subpasses[i].resolve_references.size(); j++) {
				_attachment_reference_to_vk(p_subpasses[i].resolve_references[j], &vk_subpass_resolve_attachments[j]);
			}

			VkAttachmentReference2KHR* vk_subpass_depth_stencil_attachment = nullptr;
			if (p_subpasses[i].depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
				vk_subpass_depth_stencil_attachment = vk_subpass_depth_stencil_attachment_vec.data();
				_attachment_reference_to_vk(p_subpasses[i].depth_stencil_reference, vk_subpass_depth_stencil_attachment);
			}

			vk_subpasses[i] = {};
			vk_subpasses[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
			vk_subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			vk_subpasses[i].viewMask = p_view_count == 1 ? 0 : view_mask;
			vk_subpasses[i].inputAttachmentCount = p_subpasses[i].input_references.size();
			vk_subpasses[i].pInputAttachments = vk_subpass_input_attachments;
			vk_subpasses[i].colorAttachmentCount = p_subpasses[i].color_references.size();
			vk_subpasses[i].pColorAttachments = vk_subpass_color_attachments;
			vk_subpasses[i].pResolveAttachments = vk_subpass_resolve_attachments;
			vk_subpasses[i].pDepthStencilAttachment = vk_subpass_depth_stencil_attachment;
			vk_subpasses[i].preserveAttachmentCount = p_subpasses[i].preserve_attachments.size();
			vk_subpasses[i].pPreserveAttachments = p_subpasses[i].preserve_attachments.data();

			// Fragment shading rate.
			/*if (fsr_capabilities.attachment_supported && p_subpasses[i].fragment_shading_rate_reference.attachment != AttachmentReference::UNUSED) {
				VkAttachmentReference2KHR* vk_subpass_fsr_attachment = ALLOCA_SINGLE(VkAttachmentReference2KHR);
				*vk_subpass_fsr_attachment = {};
				vk_subpass_fsr_attachment->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
				vk_subpass_fsr_attachment->attachment = p_subpasses[i].fragment_shading_rate_reference.attachment;
				vk_subpass_fsr_attachment->layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

				VkFragmentShadingRateAttachmentInfoKHR* vk_fsr_info = ALLOCA_SINGLE(VkFragmentShadingRateAttachmentInfoKHR);
				*vk_fsr_info = {};
				vk_fsr_info->sType = VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
				vk_fsr_info->pNext = vk_subpasses[i].pNext;
				vk_fsr_info->pFragmentShadingRateAttachment = vk_subpass_fsr_attachment;
				vk_fsr_info->shadingRateAttachmentTexelSize.width = p_subpasses[i].fragment_shading_rate_texel_size.x;
				vk_fsr_info->shadingRateAttachmentTexelSize.height = p_subpasses[i].fragment_shading_rate_texel_size.y;

				vk_subpasses[i].pNext = vk_fsr_info;
			}*/

			// Depth resolve.
			if (framebuffer_depth_resolve && p_subpasses[i].depth_resolve_reference.attachment != AttachmentReference::UNUSED) {
				std::vector<VkAttachmentReference2KHR> vk_subpass_depth_resolve_attachment_vec;
				VkAttachmentReference2KHR* vk_subpass_depth_resolve_attachment = vk_subpass_depth_resolve_attachment_vec.data();
				*vk_subpass_depth_resolve_attachment = {};
				vk_subpass_depth_resolve_attachment->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
				vk_subpass_depth_resolve_attachment->attachment = p_subpasses[i].depth_resolve_reference.attachment;
				vk_subpass_depth_resolve_attachment->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				std::vector<VkSubpassDescriptionDepthStencilResolveKHR> vk_depth_resolve_info_vec;
				VkSubpassDescriptionDepthStencilResolveKHR* vk_depth_resolve_info = vk_depth_resolve_info_vec.data();
				*vk_depth_resolve_info = {};
				vk_depth_resolve_info->sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
				vk_depth_resolve_info->pNext = vk_subpasses[i].pNext;
				vk_depth_resolve_info->depthResolveMode = VK_RESOLVE_MODE_MAX_BIT_KHR;
				vk_depth_resolve_info->stencilResolveMode = VK_RESOLVE_MODE_NONE_KHR; // we don't resolve our stencil (for now)
				vk_depth_resolve_info->pDepthStencilResolveAttachment = vk_subpass_depth_resolve_attachment;

				vk_subpasses[i].pNext = vk_depth_resolve_info;
			}
		}

		std::vector<VkSubpassDependency2KHR> vk_subpass_dependencies_vec(p_subpass_dependencies.size());
		VkSubpassDependency2KHR* vk_subpass_dependencies = vk_subpass_dependencies_vec.data();
		for (uint32_t i = 0; i < p_subpass_dependencies.size(); i++) {
			vk_subpass_dependencies[i] = {};
			vk_subpass_dependencies[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
			vk_subpass_dependencies[i].srcSubpass = p_subpass_dependencies[i].src_subpass;
			vk_subpass_dependencies[i].dstSubpass = p_subpass_dependencies[i].dst_subpass;
			vk_subpass_dependencies[i].srcStageMask = _rd_to_vk_pipeline_stages(p_subpass_dependencies[i].src_stages);
			vk_subpass_dependencies[i].dstStageMask = _rd_to_vk_pipeline_stages(p_subpass_dependencies[i].dst_stages);
			vk_subpass_dependencies[i].srcAccessMask = _rd_to_vk_access_flags(p_subpass_dependencies[i].src_access);
			vk_subpass_dependencies[i].dstAccessMask = _rd_to_vk_access_flags(p_subpass_dependencies[i].dst_access);
		}

		VkRenderPassCreateInfo2KHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
		create_info.attachmentCount = p_attachments.size();
		create_info.pAttachments = vk_attachments;
		create_info.subpassCount = p_subpasses.size();
		create_info.pSubpasses = vk_subpasses;
		create_info.dependencyCount = p_subpass_dependencies.size();
		create_info.pDependencies = vk_subpass_dependencies;
		create_info.correlatedViewMaskCount = p_view_count == 1 ? 0 : 1;
		create_info.pCorrelatedViewMasks = p_view_count == 1 ? nullptr : &correlation_mask;

		// Multiview.
		if (p_view_count > 1 && vkCreateRenderPass2KHR == nullptr) {
			// This is only required when not using vkCreateRenderPass2.
			// We add it if vkCreateRenderPass2KHR is not supported,
			// resulting this in being passed to our vkCreateRenderPass fallback.
			std::vector<uint32_t> vk_view_masks_vec(p_subpasses.size());
			uint32_t* vk_view_masks = vk_view_masks_vec.data();
			for (uint32_t i = 0; i < p_subpasses.size(); i++) {
				vk_view_masks[i] = view_mask;
			}

			std::vector<VkRenderPassMultiviewCreateInfo> multiview_create_info_vec;
				VkRenderPassMultiviewCreateInfo* multiview_create_info = multiview_create_info_vec.data();
			*multiview_create_info = {};
			multiview_create_info->sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
			multiview_create_info->subpassCount = p_subpasses.size();
			multiview_create_info->pViewMasks = vk_view_masks;
			multiview_create_info->correlationMaskCount = 1;
			multiview_create_info->pCorrelationMasks = &correlation_mask;

			create_info.pNext = multiview_create_info;
		}

		// Fragment density map.
		//bool uses_fragment_density_map = fdm_capabilities.attachment_supported && p_fragment_density_map_attachment.attachment != AttachmentReference::UNUSED;
		//if (uses_fragment_density_map) {
		//	VkRenderPassFragmentDensityMapCreateInfoEXT* vk_fdm_info = ALLOCA_SINGLE(VkRenderPassFragmentDensityMapCreateInfoEXT);
		//	vk_fdm_info->sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
		//	vk_fdm_info->fragmentDensityMapAttachment.attachment = p_fragment_density_map_attachment.attachment;
		//	vk_fdm_info->fragmentDensityMapAttachment.layout = RD_TO_VK_LAYOUT[p_fragment_density_map_attachment.layout];
		//	vk_fdm_info->pNext = create_info.pNext;
		//	create_info.pNext = vk_fdm_info;
		//}

		VkRenderPass vk_render_pass = VK_NULL_HANDLE;
		VkResult res = _create_render_pass(vk_device, &create_info, nullptr, &vk_render_pass);
		ERR_FAIL_COND_V_MSG(res, RenderPassID(), std::format("vkCreateRenderPass2KHR failed with error {} .", std::to_string(res)));

		RenderPassInfo* render_pass = new RenderPassInfo;
		render_pass->vk_render_pass = vk_render_pass;
		render_pass->uses_fragment_density_map = false;// uses_fragment_density_map;
		return RenderPassID(render_pass);
	}

	void RenderingDeviceDriverVulkan::render_pass_free(RenderPassID p_render_pass) {
		RenderPassInfo* render_pass = (RenderPassInfo*)(p_render_pass.id);
		vkDestroyRenderPass(vk_device, render_pass->vk_render_pass, nullptr);
		delete render_pass;
	}

	// ----- COMMANDS -----

	void RenderingDeviceDriverVulkan::command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, 
		FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i& p_rect,
		std::span<RenderingDeviceDriver::RenderPassClearValue> p_clear_values) {

		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);
		RenderPassInfo* render_pass = (RenderPassInfo*)(p_render_pass.id);

		Framebuffer* framebuffer = (Framebuffer*)(p_framebuffer.id);

		if (framebuffer->swap_chain_acquired) {
			// Insert a barrier to wait for the acquisition of the framebuffer before the render pass begins.
			VkImageMemoryBarrier image_barrier = {};
			image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.image = framebuffer->swap_chain_image;
			image_barrier.subresourceRange = framebuffer->swap_chain_image_subresource_range;
			vkCmdPipelineBarrier(command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
			framebuffer->swap_chain_acquired = false;
		}

		VkRenderPassBeginInfo render_pass_begin = {};
		render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin.renderPass = render_pass->vk_render_pass;
		render_pass_begin.framebuffer = framebuffer->vk_framebuffer;

		render_pass_begin.renderArea.offset.x = p_rect.position.x;
		render_pass_begin.renderArea.offset.y = p_rect.position.y;
		render_pass_begin.renderArea.extent.width = p_rect.size.x;
		render_pass_begin.renderArea.extent.height = p_rect.size.y;

		render_pass_begin.clearValueCount = p_clear_values.size();
		render_pass_begin.pClearValues = (const VkClearValue*)p_clear_values.data();

		VkSubpassContents vk_subpass_contents = p_cmd_buffer_type == COMMAND_BUFFER_TYPE_PRIMARY ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
		vkCmdBeginRenderPass(command_buffer->vk_command_buffer, &render_pass_begin, vk_subpass_contents);

		command_buffer->active_framebuffer = framebuffer;
		command_buffer->active_render_pass = render_pass;

#if PRINT_NATIVE_COMMANDS
		LOGI(vformat("vkCmdBeginRenderPass Pass 0x%uX Framebuffer 0x%uX", p_render_pass.id, p_framebuffer.id));
#endif
	}

	void RenderingDeviceDriverVulkan::command_end_render_pass(CommandBufferID p_cmd_buffer) {
		CommandBufferInfo* command_buffer = (CommandBufferInfo*)(p_cmd_buffer.id);
		DEBUG_ASSERT(command_buffer->active_framebuffer != nullptr && "A framebuffer must be active.");
		DEBUG_ASSERT(command_buffer->active_render_pass != nullptr && "A render pass must be active.");

		//if (vkEndRenderPass2KHR != nullptr && fdm_capabilities.offset_supported && command_buffer->active_render_pass->uses_fragment_density_map) {
		//	std::vector<VkOffset2D> fragment_density_offsets;
		//	if (VulkanHooks::get_singleton() != nullptr) {
		//		VulkanHooks::get_singleton()->get_fragment_density_offsets(fragment_density_offsets, fdm_capabilities.offset_granularity);
		//	}
		//	if (fragment_density_offsets.size() > 0) {
		//		VkSubpassFragmentDensityMapOffsetEndInfoQCOM offset_info = {};
		//		offset_info.sType = VK_STRUCTURE_TYPE_SUBPASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_QCOM;
		//		offset_info.pFragmentDensityOffsets = fragment_density_offsets.ptr();
		//		offset_info.fragmentDensityOffsetCount = fragment_density_offsets.size();

		//		VkSubpassEndInfo subpass_end_info = {};
		//		subpass_end_info.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO;
		//		subpass_end_info.pNext = &offset_info;

		//		device_functions.EndRenderPass2KHR(command_buffer->vk_command_buffer, &subpass_end_info);
		//	}
		//	else {
		//		vkCmdEndRenderPass(command_buffer->vk_command_buffer);
		//	}
		//}
		//else {
			vkCmdEndRenderPass(command_buffer->vk_command_buffer);
		//}

		command_buffer->active_render_pass = nullptr;
		command_buffer->active_framebuffer = nullptr;

#if PRINT_NATIVE_COMMANDS
		LOGI("vkCmdEndRenderPass");
#endif
	}

	void RenderingDeviceDriverVulkan::command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		VkSubpassContents vk_subpass_contents = p_cmd_buffer_type == COMMAND_BUFFER_TYPE_PRIMARY ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
		vkCmdNextSubpass(command_buffer->vk_command_buffer, vk_subpass_contents);
	}

	void RenderingDeviceDriverVulkan::command_render_set_viewport(CommandBufferID p_cmd_buffer, std::span<Rect2i> p_viewports) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;

		std::vector<VkViewport> vk_viewports_vec(p_viewports.size());
		VkViewport* vk_viewports = vk_viewports_vec.data();
		for (uint32_t i = 0; i < p_viewports.size(); i++) {
			vk_viewports[i] = {};
			vk_viewports[i].x = p_viewports[i].position.x;
			vk_viewports[i].y = p_viewports[i].position.y;
			vk_viewports[i].width = p_viewports[i].size.x;
			vk_viewports[i].height = p_viewports[i].size.y;
			vk_viewports[i].minDepth = 0.0f;
			vk_viewports[i].maxDepth = 1.0f;
		}
		vkCmdSetViewport(command_buffer->vk_command_buffer, 0, p_viewports.size(), vk_viewports);
	}

	void RenderingDeviceDriverVulkan::command_render_set_scissor(CommandBufferID p_cmd_buffer, std::span<Rect2i> p_scissors) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdSetScissor(command_buffer->vk_command_buffer, 0, p_scissors.size(), (VkRect2D*)p_scissors.data());
	}

	void RenderingDeviceDriverVulkan::command_render_clear_attachments(CommandBufferID p_cmd_buffer, std::span<AttachmentClear> p_attachment_clears, std::span<Rect2i> p_rects) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;

		std::vector<VkClearAttachment> vk_clears_vec(p_attachment_clears.size());
		VkClearAttachment* vk_clears = vk_clears_vec.data();
		for (uint32_t i = 0; i < p_attachment_clears.size(); i++) {
			vk_clears[i] = {};
			memcpy(&vk_clears[i].clearValue, &p_attachment_clears[i].value, sizeof(VkClearValue));
			vk_clears[i].colorAttachment = p_attachment_clears[i].color_attachment;
			vk_clears[i].aspectMask = p_attachment_clears[i].aspect;
		}

		std::vector<VkClearRect> vk_rects_vec(p_rects.size());
		VkClearRect* vk_rects = vk_rects_vec.data();
		for (uint32_t i = 0; i < p_rects.size(); i++) {
			vk_rects[i] = {};
			vk_rects[i].rect.offset.x = p_rects[i].position.x;
			vk_rects[i].rect.offset.y = p_rects[i].position.y;
			vk_rects[i].rect.extent.width = p_rects[i].size.x;
			vk_rects[i].rect.extent.height = p_rects[i].size.y;
			vk_rects[i].baseArrayLayer = 0;
			vk_rects[i].layerCount = 1;
		}

		vkCmdClearAttachments(command_buffer->vk_command_buffer, p_attachment_clears.size(), vk_clears, p_rects.size(), vk_rects);
	}

	void RenderingDeviceDriverVulkan::command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdBindPipeline(command_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)p_pipeline.id);
	}

	void RenderingDeviceDriverVulkan::command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, std::span<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) {
		if (p_set_count == 0) {
			return;
		}

		thread_local std::vector<VkDescriptorSet> sets;
		sets.clear();
		sets.resize(p_set_count);

		uint32_t dynamic_offsets[MAX_DYNAMIC_BUFFERS];
		uint32_t shift = 0u;
		uint32_t curr_dynamic_offset = 0u;

		for (uint32_t i = 0; i < p_set_count; i++) {
			const UniformSetInfo* usi = (const UniformSetInfo*)p_uniform_sets[i].id;

			sets[i] = usi->vk_descriptor_set;

			// At this point this assert should already have been validated.
			DEV_ASSERT(curr_dynamic_offset + usi->dynamic_buffers.size() <= MAX_DYNAMIC_BUFFERS);

			const uint32_t dynamic_offset_count = usi->dynamic_buffers.size();
			for (uint32_t j = 0u; j < dynamic_offset_count; ++j) {
				const uint32_t frame_idx = (p_dynamic_offsets >> shift) & 0xFu;
				shift += 4u;
				dynamic_offsets[curr_dynamic_offset++] = uint32_t(frame_idx * usi->dynamic_buffers[j]->size);
			}
		}

		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const ShaderInfo* shader_info = (const ShaderInfo*)p_shader.id;
		vkCmdBindDescriptorSets(command_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader_info->vk_pipeline_layout, p_first_set_index, p_set_count, &sets[0], curr_dynamic_offset, dynamic_offsets);
	}

	void RenderingDeviceDriverVulkan::command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdDraw(command_buffer->vk_command_buffer, p_vertex_count, p_instance_count, p_base_vertex, p_first_instance);
	}

	void RenderingDeviceDriverVulkan::command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdDrawIndexed(command_buffer->vk_command_buffer, p_index_count, p_instance_count, p_first_index, p_vertex_offset, p_first_instance);
	}

	void RenderingDeviceDriverVulkan::command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_indirect_buffer.id;
		vkCmdDrawIndexedIndirect(command_buffer->vk_command_buffer, buf_info->vk_buffer, p_offset, p_draw_count, p_stride);
	}

	void RenderingDeviceDriverVulkan::command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* indirect_buf_info = (const BufferInfo*)p_indirect_buffer.id;
		const BufferInfo* count_buf_info = (const BufferInfo*)p_count_buffer.id;
		vkCmdDrawIndexedIndirectCount(command_buffer->vk_command_buffer, indirect_buf_info->vk_buffer, p_offset, count_buf_info->vk_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
	}

	void RenderingDeviceDriverVulkan::command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_indirect_buffer.id;
		vkCmdDrawIndirect(command_buffer->vk_command_buffer, buf_info->vk_buffer, p_offset, p_draw_count, p_stride);
	}

	void RenderingDeviceDriverVulkan::command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* indirect_buf_info = (const BufferInfo*)p_indirect_buffer.id;
		const BufferInfo* count_buf_info = (const BufferInfo*)p_count_buffer.id;
		vkCmdDrawIndirectCount(command_buffer->vk_command_buffer, indirect_buf_info->vk_buffer, p_offset, count_buf_info->vk_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
	}

	void RenderingDeviceDriverVulkan::command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID* p_buffers, const uint64_t* p_offsets, uint64_t p_dynamic_offsets) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;

		std::vector<VkBuffer> vk_buffers_vec(p_binding_count);
		VkBuffer* vk_buffers = vk_buffers_vec.data();
		std::vector<uint64_t> vk_offsets_vec(p_binding_count);
		uint64_t* vk_offsets = vk_offsets_vec.data();
		for (uint32_t i = 0; i < p_binding_count; i++) {
			const BufferInfo* buf_info = (const BufferInfo*)p_buffers[i].id;
			uint64_t offset = p_offsets[i];
			if (buf_info->is_dynamic()) {
				uint64_t frame_idx = p_dynamic_offsets & 0x3; // Assuming max 4 frames.
				p_dynamic_offsets >>= 2;
				offset += frame_idx * buf_info->size;
			}
			vk_buffers[i] = ((const BufferInfo*)p_buffers[i].id)->vk_buffer;
			vk_offsets[i] = offset;
		}
		vkCmdBindVertexBuffers(command_buffer->vk_command_buffer, 0, p_binding_count, vk_buffers, vk_offsets);
	}

	void RenderingDeviceDriverVulkan::command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		vkCmdBindIndexBuffer(command_buffer->vk_command_buffer, buf_info->vk_buffer, p_offset, p_format == INDEX_BUFFER_FORMAT_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	}

	void RenderingDeviceDriverVulkan::command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color& p_constants) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdSetBlendConstants(command_buffer->vk_command_buffer, glm::value_ptr(p_constants));
	}

	void RenderingDeviceDriverVulkan::command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdSetLineWidth(command_buffer->vk_command_buffer, p_width);
	}

	// ----- PIPELINE -----

	static const VkPrimitiveTopology RD_TO_VK_PRIMITIVE[RenderingDeviceDriverVulkan::RENDER_PRIMITIVE_MAX] = {
	VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
	VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
	};

	RenderingDeviceDriverVulkan::PipelineID RenderingDeviceDriverVulkan::render_pipeline_create(
								ShaderID p_shader,
								VertexFormatID p_vertex_format,
								RenderPrimitive p_render_primitive,
								PipelineRasterizationState p_rasterization_state,
								PipelineMultisampleState p_multisample_state,
								PipelineDepthStencilState p_depth_stencil_state,
								PipelineColorBlendState p_blend_state,
								std::span<int32_t> p_color_attachments,
								BitField<PipelineDynamicStateFlags> p_dynamic_state,
								RenderPassID p_render_pass,
								uint32_t p_render_subpass,
								std::span<PipelineSpecializationConstant> p_specialization_constants) {
		// Vertex.
		const VkPipelineVertexInputStateCreateInfo* vertex_input_state_create_info = nullptr;
		std::vector< VkPipelineVertexInputStateCreateInfo> null_vertex_input_state_vec;

		if (p_vertex_format.id) {
			const VertexFormatInfo* vf_info = (const VertexFormatInfo*)p_vertex_format.id;
			vertex_input_state_create_info = &vf_info->vk_create_info;
		}
		else {
			null_vertex_input_state_vec.resize(1);
			VkPipelineVertexInputStateCreateInfo* null_vertex_input_state = null_vertex_input_state_vec.data();
			null_vertex_input_state->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertex_input_state_create_info = null_vertex_input_state;
		}

		// Input assembly.
		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
		input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_create_info.topology = RD_TO_VK_PRIMITIVE[p_render_primitive];
		input_assembly_create_info.primitiveRestartEnable = (p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX);

		// Tessellation.
		VkPipelineTessellationStateCreateInfo tessellation_create_info = {};
		tessellation_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		ERR_FAIL_COND_V(physical_device_properties.limits.maxTessellationPatchSize > 0 && (p_rasterization_state.patch_control_points < 1 || p_rasterization_state.patch_control_points > physical_device_properties.limits.maxTessellationPatchSize), PipelineID());
		tessellation_create_info.patchControlPoints = p_rasterization_state.patch_control_points;

		// Viewport.
		VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
		viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_create_info.viewportCount = 1; // If VR extensions are supported at some point, this will have to be customizable in the framebuffer format.
		viewport_state_create_info.scissorCount = 1;

		// Rasterization.
		VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
		rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_state_create_info.depthClampEnable = p_rasterization_state.enable_depth_clamp;
		rasterization_state_create_info.rasterizerDiscardEnable = p_rasterization_state.discard_primitives;
		rasterization_state_create_info.polygonMode = p_rasterization_state.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
		rasterization_state_create_info.cullMode = (PolygonCullMode)p_rasterization_state.cull_mode;
		rasterization_state_create_info.frontFace = (p_rasterization_state.front_face == POLYGON_FRONT_FACE_CLOCKWISE ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE);
		rasterization_state_create_info.depthBiasEnable = p_rasterization_state.depth_bias_enabled;
		rasterization_state_create_info.depthBiasConstantFactor = p_rasterization_state.depth_bias_constant_factor;
		rasterization_state_create_info.depthBiasClamp = p_rasterization_state.depth_bias_clamp;
		rasterization_state_create_info.depthBiasSlopeFactor = p_rasterization_state.depth_bias_slope_factor;
		rasterization_state_create_info.lineWidth = p_rasterization_state.line_width;

		// Multisample.
		VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
		multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_state_create_info.rasterizationSamples = _ensure_supported_sample_count(p_multisample_state.sample_count);
		multisample_state_create_info.sampleShadingEnable = p_multisample_state.enable_sample_shading;
		multisample_state_create_info.minSampleShading = p_multisample_state.min_sample_shading;
		if (p_multisample_state.sample_mask.size()) {
			//static_assert(ARRAYS_COMPATIBLE(uint32_t, VkSampleMask));
			multisample_state_create_info.pSampleMask = p_multisample_state.sample_mask.data();
		}
		else {
			multisample_state_create_info.pSampleMask = nullptr;
		}
		multisample_state_create_info.alphaToCoverageEnable = p_multisample_state.enable_alpha_to_coverage;
		multisample_state_create_info.alphaToOneEnable = p_multisample_state.enable_alpha_to_one;

		// Depth stencil.

		VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
		depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_state_create_info.depthTestEnable = p_depth_stencil_state.enable_depth_test;
		depth_stencil_state_create_info.depthWriteEnable = p_depth_stencil_state.enable_depth_write;
		depth_stencil_state_create_info.depthCompareOp = (VkCompareOp)p_depth_stencil_state.depth_compare_operator;
		depth_stencil_state_create_info.depthBoundsTestEnable = p_depth_stencil_state.enable_depth_range;
		depth_stencil_state_create_info.stencilTestEnable = p_depth_stencil_state.enable_stencil;

		depth_stencil_state_create_info.front.failOp = (VkStencilOp)p_depth_stencil_state.front_op.fail;
		depth_stencil_state_create_info.front.passOp = (VkStencilOp)p_depth_stencil_state.front_op.pass;
		depth_stencil_state_create_info.front.depthFailOp = (VkStencilOp)p_depth_stencil_state.front_op.depth_fail;
		depth_stencil_state_create_info.front.compareOp = (VkCompareOp)p_depth_stencil_state.front_op.compare;
		depth_stencil_state_create_info.front.compareMask = p_depth_stencil_state.front_op.compare_mask;
		depth_stencil_state_create_info.front.writeMask = p_depth_stencil_state.front_op.write_mask;
		depth_stencil_state_create_info.front.reference = p_depth_stencil_state.front_op.reference;

		depth_stencil_state_create_info.back.failOp = (VkStencilOp)p_depth_stencil_state.back_op.fail;
		depth_stencil_state_create_info.back.passOp = (VkStencilOp)p_depth_stencil_state.back_op.pass;
		depth_stencil_state_create_info.back.depthFailOp = (VkStencilOp)p_depth_stencil_state.back_op.depth_fail;
		depth_stencil_state_create_info.back.compareOp = (VkCompareOp)p_depth_stencil_state.back_op.compare;
		depth_stencil_state_create_info.back.compareMask = p_depth_stencil_state.back_op.compare_mask;
		depth_stencil_state_create_info.back.writeMask = p_depth_stencil_state.back_op.write_mask;
		depth_stencil_state_create_info.back.reference = p_depth_stencil_state.back_op.reference;

		depth_stencil_state_create_info.minDepthBounds = p_depth_stencil_state.depth_range_min;
		depth_stencil_state_create_info.maxDepthBounds = p_depth_stencil_state.depth_range_max;

		// Blend state.

		VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
		color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_state_create_info.logicOpEnable = p_blend_state.enable_logic_op;
		color_blend_state_create_info.logicOp = (VkLogicOp)p_blend_state.logic_op;

		std::vector<VkPipelineColorBlendAttachmentState> vk_attachment_states_vec(p_color_attachments.size());
		VkPipelineColorBlendAttachmentState* vk_attachment_states = vk_attachment_states_vec.data();
		{
			for (uint32_t i = 0; i < p_color_attachments.size(); i++) {
				vk_attachment_states[i] = {};
				if (p_color_attachments[i] != ATTACHMENT_UNUSED) {
					vk_attachment_states[i].blendEnable = p_blend_state.attachments[i].enable_blend;

					vk_attachment_states[i].srcColorBlendFactor = (VkBlendFactor)p_blend_state.attachments[i].src_color_blend_factor;
					vk_attachment_states[i].dstColorBlendFactor = (VkBlendFactor)p_blend_state.attachments[i].dst_color_blend_factor;
					vk_attachment_states[i].colorBlendOp = (VkBlendOp)p_blend_state.attachments[i].color_blend_op;

					vk_attachment_states[i].srcAlphaBlendFactor = (VkBlendFactor)p_blend_state.attachments[i].src_alpha_blend_factor;
					vk_attachment_states[i].dstAlphaBlendFactor = (VkBlendFactor)p_blend_state.attachments[i].dst_alpha_blend_factor;
					vk_attachment_states[i].alphaBlendOp = (VkBlendOp)p_blend_state.attachments[i].alpha_blend_op;

					if (p_blend_state.attachments[i].write_r) {
						vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
					}
					if (p_blend_state.attachments[i].write_g) {
						vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
					}
					if (p_blend_state.attachments[i].write_b) {
						vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
					}
					if (p_blend_state.attachments[i].write_a) {
						vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
					}
				}
			}
		}
		color_blend_state_create_info.attachmentCount = p_color_attachments.size();
		color_blend_state_create_info.pAttachments = vk_attachment_states;

		color_blend_state_create_info.blendConstants[0] = p_blend_state.blend_constant.r;
		color_blend_state_create_info.blendConstants[1] = p_blend_state.blend_constant.g;
		color_blend_state_create_info.blendConstants[2] = p_blend_state.blend_constant.b;
		color_blend_state_create_info.blendConstants[3] = p_blend_state.blend_constant.a;

		// Dynamic state.

		VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
		dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

		static const uint32_t MAX_DYN_STATE_COUNT = 9;

		std::vector<VkDynamicState> vk_dynamic_static_vec(MAX_DYN_STATE_COUNT);
		VkDynamicState* vk_dynamic_states = vk_dynamic_static_vec.data();
		uint32_t vk_dynamic_states_count = 0;

		vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_VIEWPORT; // Viewport and scissor are always dynamic.
		vk_dynamic_states_count++;
		vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_SCISSOR;
		vk_dynamic_states_count++;
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_LINE_WIDTH)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_LINE_WIDTH;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BIAS)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_DEPTH_BIAS;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_BLEND_CONSTANTS)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BOUNDS)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_COMPARE_MASK)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_WRITE_MASK)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
			vk_dynamic_states_count++;
		}
		if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_REFERENCE)) {
			vk_dynamic_states[vk_dynamic_states_count] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
			vk_dynamic_states_count++;
		}
		DEV_ASSERT(vk_dynamic_states_count <= MAX_DYN_STATE_COUNT);

		dynamic_state_create_info.dynamicStateCount = vk_dynamic_states_count;
		dynamic_state_create_info.pDynamicStates = vk_dynamic_states;

		void* graphics_pipeline_nextptr = nullptr;

		//if (fsr_capabilities.attachment_supported) {
		//	// Fragment shading rate.
		//	// If FSR is used, this defines how the different FSR types are combined.
		//	// combinerOps[0] decides how we use the output of pipeline and primitive (drawcall) FSR.
		//	// combinerOps[1] decides how we use the output of combinerOps[0] and our attachment FSR.

		//	VkPipelineFragmentShadingRateStateCreateInfoKHR* fsr_create_info = ALLOCA_SINGLE(VkPipelineFragmentShadingRateStateCreateInfoKHR);
		//	*fsr_create_info = {};
		//	fsr_create_info->sType = VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;
		//	fsr_create_info->fragmentSize = { 4, 4 };
		//	fsr_create_info->combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR; // We don't use pipeline/primitive FSR so this really doesn't matter.
		//	fsr_create_info->combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR; // Always use the outcome of attachment FSR if enabled.

		//	graphics_pipeline_nextptr = fsr_create_info;
		//}

		// Finally, pipeline create info.

		const ShaderInfo* shader_info = (const ShaderInfo*)p_shader.id;

		VkGraphicsPipelineCreateInfo pipeline_create_info = {};

		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.pNext = graphics_pipeline_nextptr;
		pipeline_create_info.stageCount = shader_info->vk_stages_create_info.size();

		ERR_FAIL_COND_V_MSG(pipeline_create_info.stageCount == 0, PipelineID(),
			"Cannot create pipeline without shader module, please make sure shader modules are destroyed only after all associated pipelines are created.");
		std::vector<VkPipelineShaderStageCreateInfo> vk_pipeline_stage_vec(shader_info->vk_stages_create_info.size());
		VkPipelineShaderStageCreateInfo* vk_pipeline_stages = vk_pipeline_stage_vec.data();

		thread_local std::vector<uint8_t> respv_optimized_data;
		thread_local std::vector<respv::SpecConstant> respv_spec_constants;
		thread_local std::vector<VkShaderModule> respv_shader_modules;
		thread_local std::vector<VkSpecializationMapEntry> specialization_entries;

#if RECORD_PIPELINE_STATISTICS
		thread_local std::vector<uint64_t> respv_run_time;
		thread_local std::vector<uint64_t> respv_size;
		uint32_t stage_count = shader_info->vk_stages_create_info.size();
		respv_run_time.clear();
		respv_size.clear();
		respv_run_time.resize(stage_count);
		respv_size.resize(stage_count);
#endif

		respv_shader_modules.clear();
		specialization_entries.clear();

		for (uint32_t i = 0; i < shader_info->vk_stages_create_info.size(); i++) {
			vk_pipeline_stages[i] = shader_info->vk_stages_create_info[i];

			if (p_specialization_constants.size()) {
				bool use_pipeline_spec_constants = true;
				if ((i < shader_info->respv_stage_shaders.size()) && !shader_info->respv_stage_shaders[i].empty()) {
#if RECORD_PIPELINE_STATISTICS
					uint64_t respv_start_time = OS::get_singleton()->get_ticks_usec();
#endif
					// Attempt to optimize the shader using re-spirv before relying on the driver.
					respv_spec_constants.resize(p_specialization_constants.size());
					for (uint32_t j = 0; j < p_specialization_constants.size(); j++) {
						respv_spec_constants[j].specId = p_specialization_constants[j].constant_id;
						respv_spec_constants[j].values.resize(1);
						respv_spec_constants[j].values[0] = p_specialization_constants[j].int_value;
					}

					respv::Options respv_options;
#if RESPV_DONT_REMOVE_DEAD_CODE
					respv_options.removeDeadCode = false;
#endif
					if (respv::Optimizer::run(shader_info->respv_stage_shaders[i], respv_spec_constants.data(), respv_spec_constants.size(), respv_optimized_data, respv_options)) {
#if RESPV_VERBOSE
						String spec_constants;
						for (uint32_t j = 0; j < p_specialization_constants.size(); j++) {
							spec_constants += vformat("%d: %d", p_specialization_constants[j].constant_id, p_specialization_constants[j].int_value);
							if (j < p_specialization_constants.size() - 1) {
								spec_constants += ", ";
							}
						}

						LOGI(std::format("re-spirv transformed the shader from {} bytes to {} bytes with constants {} ({}).", shader_info->respv_stage_shaders[i].inlinedSpirvWords.size() * sizeof(uint32_t), respv_optimized_data.size(), spec_constants, p_shader.id));
#endif

						// Create the shader module with the optimized output.
						VkShaderModule shader_module = VK_NULL_HANDLE;
						VkShaderModuleCreateInfo shader_module_create_info = {};
						shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
						shader_module_create_info.pCode = (const uint32_t*)(respv_optimized_data.data());
						shader_module_create_info.codeSize = respv_optimized_data.size();
						VkResult err = vkCreateShaderModule(vk_device, &shader_module_create_info, nullptr, &shader_module);
						if (err == VK_SUCCESS) {
							// Replace the module used in the creation info.
							vk_pipeline_stages[i].module = shader_module;
							respv_shader_modules.push_back(shader_module);
							use_pipeline_spec_constants = false;
						}

#if RECORD_PIPELINE_STATISTICS
						respv_run_time[i] = OS::get_singleton()->get_ticks_usec() - respv_start_time;
						respv_size[i] = respv_optimized_data.size();
#endif
					}
					else {
#if RESPV_VERBOSE
						LOGI("re-spirv failed to optimize the shader.");
#endif
					}
				}

				if (use_pipeline_spec_constants) {
					// Use specialization constants through the driver.
					if (specialization_entries.empty()) {
						specialization_entries.resize(p_specialization_constants.size());
						for (uint32_t j = 0; j < p_specialization_constants.size(); j++) {
							specialization_entries[j] = {};
							specialization_entries[j].constantID = p_specialization_constants[j].constant_id;
							specialization_entries[j].offset = (const char*)&p_specialization_constants[j].int_value - (const char*)p_specialization_constants.data();
							specialization_entries[j].size = sizeof(uint32_t);
						}
					}

					std::vector<VkSpecializationInfo> specialization_info_vec;
					VkSpecializationInfo* specialization_info = specialization_info_vec.data();
					*specialization_info = {};
					specialization_info->dataSize = p_specialization_constants.size() * sizeof(PipelineSpecializationConstant);
					specialization_info->pData = p_specialization_constants.data();
					specialization_info->mapEntryCount = specialization_entries.size();
					specialization_info->pMapEntries = specialization_entries.data();

					vk_pipeline_stages[i].pSpecializationInfo = specialization_info;
				}
			}
		}

		const RenderPassInfo* render_pass = (const RenderPassInfo*)(p_render_pass.id);
		pipeline_create_info.pStages = vk_pipeline_stages;
		pipeline_create_info.pVertexInputState = vertex_input_state_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_create_info.pTessellationState = &tessellation_create_info;
		pipeline_create_info.pViewportState = &viewport_state_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
		pipeline_create_info.pMultisampleState = &multisample_state_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
		pipeline_create_info.pDynamicState = &dynamic_state_create_info;
		pipeline_create_info.layout = shader_info->vk_pipeline_layout;
		pipeline_create_info.renderPass = render_pass->vk_render_pass;
		pipeline_create_info.subpass = p_render_subpass;

#if RECORD_PIPELINE_STATISTICS
		uint64_t pipeline_start_time = OS::get_singleton()->get_ticks_usec();
#endif

		VkPipeline vk_pipeline = VK_NULL_HANDLE;
		VkResult err = vkCreateGraphicsPipelines(vk_device, pipelines_cache.vk_cache, 1, &pipeline_create_info, nullptr, &vk_pipeline);
		ERR_FAIL_COND_V_MSG(err, PipelineID(), std::format("vkCreateGraphicsPipelines failed with error ", std::to_string(err)));

#if RECORD_PIPELINE_STATISTICS
		{
			MutexLock lock(pipeline_statistics.file_access_mutex);
			uint64_t pipeline_creation_time = OS::get_singleton()->get_ticks_usec() - pipeline_start_time;
			for (uint32_t i = 0; i < shader_info->vk_stages_create_info.size(); i++) {
				PackedStringArray csv_array = {
					shader_info->name,
					String::num_uint64(hash_murmur3_buffer(shader_info->spirv_stage_bytes[i].ptr(), shader_info->spirv_stage_bytes[i].size())),
					String::num_uint64(i),
					String::num_uint64(respv_size[i] > 0),
					String::num_uint64(shader_info->original_stage_size[i]),
					String::num_uint64(respv_size[i] > 0 ? respv_size[i] : shader_info->spirv_stage_bytes[i].size()),
					String::num_uint64(respv_run_time[i] + pipeline_creation_time)
				};

				pipeline_statistics.file_access->store_csv_line(csv_array);
			}

			pipeline_statistics.file_access->flush();
		}
#endif

		// Destroy any modules created temporarily by re-spirv.
		for (VkShaderModule vk_module : respv_shader_modules) {
			vkDestroyShaderModule(vk_device, vk_module, nullptr);
		}

		return PipelineID(vk_pipeline);
	}


#pragma endregion

#pragma region Debug

	void RenderingDeviceDriverVulkan::print_lost_device_info() {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		{
			String error_msg = "Printing last known breadcrumbs in reverse order (last executed first).";
			if (!Engine::get_singleton()->is_accurate_breadcrumbs_enabled()) {
				error_msg += "\nSome of them might be inaccurate. Try running with --accurate-breadcrumbs for precise information.";
			}
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, error_msg);
		}

	uint8_t* breadcrumb_ptr = nullptr;
	VkResult map_result = VK_SUCCESS;

	vmaFlushAllocation(allocator, ((BufferInfo*)breadcrumb_buffer.id)->allocation.handle, 0, BREADCRUMB_BUFFER_ENTRIES * sizeof(uint32_t) * 2u);
	vmaInvalidateAllocation(allocator, ((BufferInfo*)breadcrumb_buffer.id)->allocation.handle, 0, BREADCRUMB_BUFFER_ENTRIES * sizeof(uint32_t) * 2u);
	{
		void* ptr = nullptr;
		map_result = vmaMapMemory(allocator, ((BufferInfo*)breadcrumb_buffer.id)->allocation.handle, &ptr);
		breadcrumb_ptr = reinterpret_cast<uint8_t*>(ptr);
	}

	if (breadcrumb_ptr && map_result == VK_SUCCESS) {
		uint32_t last_breadcrumb_offset = 0;
		{
			_err_print_error_asap("Searching last breadcrumb. We've sent up to ID: " + itos(breadcrumb_id - 1u));

			// Scan the whole buffer to find the offset with the highest ID.
			// That means that was the last one to be written.
			//
			// We use "breadcrumb_id - id" to account for wraparound.
			// e.g. breadcrumb_id = 2 and id = 4294967294; then 2 - 4294967294 = 4.
			// The one with the smallest difference is the closest to breadcrumb_id, which means it's
			// the last written command.
			uint32_t biggest_id = 0u;
			uint32_t smallest_id_diff = std::numeric_limits<uint32_t>::max();
			const uint32_t* breadcrumb_ptr32 = reinterpret_cast<const uint32_t*>(breadcrumb_ptr);
			for (size_t i = 0u; i < BREADCRUMB_BUFFER_ENTRIES; ++i) {
				const uint32_t id = breadcrumb_ptr32[i * 2u];
				const uint32_t id_diff = breadcrumb_id - id;
				if (id_diff < smallest_id_diff) {
					biggest_id = i;
					smallest_id_diff = id_diff;
				}
			}

			_err_print_error_asap("Last breadcrumb ID found: " + itos(breadcrumb_ptr32[biggest_id * 2u]));

			last_breadcrumb_offset = biggest_id * sizeof(uint32_t) * 2u;
		}

		const size_t entries_to_print = 8u; // Note: The value is arbitrary.
		for (size_t i = 0u; i < entries_to_print; ++i) {
			const uint32_t last_breadcrumb = *reinterpret_cast<uint32_t*>(breadcrumb_ptr + last_breadcrumb_offset + sizeof(uint32_t));
			const uint32_t phase = last_breadcrumb & uint32_t(~((1 << 16) - 1));
			const uint32_t user_data = last_breadcrumb & ((1 << 16) - 1);
			String error_msg = "Last known breadcrumb: ";

			switch (phase) {
			case BreadcrumbMarker::ALPHA_PASS:
				error_msg += "ALPHA_PASS";
				break;
			case BreadcrumbMarker::BLIT_PASS:
				error_msg += "BLIT_PASS";
				break;
			case BreadcrumbMarker::DEBUG_PASS:
				error_msg += "DEBUG_PASS";
				break;
			case BreadcrumbMarker::LIGHTMAPPER_PASS:
				error_msg += "LIGHTMAPPER_PASS";
				break;
			case BreadcrumbMarker::OPAQUE_PASS:
				error_msg += "OPAQUE_PASS";
				break;
			case BreadcrumbMarker::POST_PROCESSING_PASS:
				error_msg += "POST_PROCESSING_PASS";
				break;
			case BreadcrumbMarker::REFLECTION_PROBES:
				error_msg += "REFLECTION_PROBES";
				break;
			case BreadcrumbMarker::SHADOW_PASS_CUBE:
				error_msg += "SHADOW_PASS_CUBE";
				break;
			case BreadcrumbMarker::SHADOW_PASS_DIRECTIONAL:
				error_msg += "SHADOW_PASS_DIRECTIONAL";
				break;
			case BreadcrumbMarker::SKY_PASS:
				error_msg += "SKY_PASS";
				break;
			case BreadcrumbMarker::TRANSPARENT_PASS:
				error_msg += "TRANSPARENT_PASS";
				break;
			case BreadcrumbMarker::UI_PASS:
				error_msg += "UI_PASS";
				break;
			default:
				error_msg += "UNKNOWN_BREADCRUMB(" + itos((uint32_t)phase) + ')';
				break;
			}

			if (user_data != 0) {
				error_msg += " | User data: " + itos(user_data);
			}

			_err_print_error_asap(error_msg);

			if (last_breadcrumb_offset == 0u) {
				// Decrement last_breadcrumb_idx, wrapping underflow.
				last_breadcrumb_offset = BREADCRUMB_BUFFER_ENTRIES * sizeof(uint32_t) * 2u;
			}
			last_breadcrumb_offset -= sizeof(uint32_t) * 2u;
		}

		vmaUnmapMemory(allocator, ((BufferInfo*)breadcrumb_buffer.id)->allocation.handle);
		breadcrumb_ptr = nullptr;
	}
	else {
		_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Couldn't map breadcrumb buffer. VkResult = " + itos(map_result));
	}
#endif
	//TODO:
	//on_device_lost();
	}

	inline std::string RenderingDeviceDriverVulkan::get_vulkan_result(VkResult err) {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		if (err == VK_ERROR_OUT_OF_HOST_MEMORY) {
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		}
		else if (err == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		}
		else if (err == VK_ERROR_DEVICE_LOST) {
			return "VK_ERROR_DEVICE_LOST";
		}
		else if (err == VK_ERROR_SURFACE_LOST_KHR) {
			return "VK_ERROR_SURFACE_LOST_KHR";
		}
		else if (err == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) {
			return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		}
#endif
		return std::to_string(err);
	}
#pragma endregion

#pragma region Queries

	RenderingDeviceDriverVulkan::QueryPoolID RenderingDeviceDriverVulkan::timestamp_query_pool_create(uint32_t p_query_count) {
		VkQueryPoolCreateInfo query_pool_create_info = {};
		query_pool_create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_pool_create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		query_pool_create_info.queryCount = p_query_count;

		VkQueryPool vk_query_pool = VK_NULL_HANDLE;
		vkCreateQueryPool(vk_device, &query_pool_create_info, nullptr, &vk_query_pool);
		return RenderingDeviceDriverVulkan::QueryPoolID(vk_query_pool);
	}

	void RenderingDeviceDriverVulkan::timestamp_query_pool_free(QueryPoolID p_pool_id) {
		vkDestroyQueryPool(vk_device, (VkQueryPool)p_pool_id.id, nullptr);
	}

	void RenderingDeviceDriverVulkan::timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t* r_results) {
		vkGetQueryPoolResults(vk_device, (VkQueryPool)p_pool_id.id, 0, p_query_count, sizeof(uint64_t) * p_query_count, r_results, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
	}

	uint64_t RenderingDeviceDriverVulkan::timestamp_query_result_to_time(uint64_t p_result) {
		// This sucks because timestampPeriod multiplier is a float, while the timestamp is 64 bits nanosecs.
		// So, in cases like nvidia which give you enormous numbers and 1 as multiplier, multiplying is next to impossible.
		// Need to do 128 bits fixed point multiplication to get the right value.

		auto mult64to128 = [](uint64_t u, uint64_t v, uint64_t& h, uint64_t& l) {
			uint64_t u1 = (u & 0xffffffff);
			uint64_t v1 = (v & 0xffffffff);
			uint64_t t = (u1 * v1);
			uint64_t w3 = (t & 0xffffffff);
			uint64_t k = (t >> 32);

			u >>= 32;
			t = (u * v1) + k;
			k = (t & 0xffffffff);
			uint64_t w1 = (t >> 32);

			v >>= 32;
			t = (u1 * v) + k;
			k = (t >> 32);

			h = (u * v) + w1 + k;
			l = (t << 32) + w3;
			};

		uint64_t shift_bits = 16;
		uint64_t h = 0, l = 0;
		mult64to128(p_result, uint64_t(double(physical_device_properties.limits.timestampPeriod) * double(1 << shift_bits)), h, l);
		l >>= shift_bits;
		l |= h << (64 - shift_bits);

		return l;
	}

	void RenderingDeviceDriverVulkan::command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdResetQueryPool(command_buffer->vk_command_buffer, (VkQueryPool)p_pool_id.id, 0, p_query_count);
	}

	void RenderingDeviceDriverVulkan::command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		vkCmdWriteTimestamp(command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkQueryPool)p_pool_id.id, p_index);
	}

#pragma endregion

#pragma region Lables

	void RenderingDeviceDriverVulkan::command_begin_label(CommandBufferID p_cmd_buffer, const char* p_label_name, const Color& p_color) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		
		if (!vkCmdBeginDebugUtilsLabelEXT) {
			if (vkCmdDebugMarkerBeginEXT) {
				// Debug marker extensions.
				VkDebugMarkerMarkerInfoEXT marker;
				marker.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				marker.pNext = nullptr;
				marker.pMarkerName = p_label_name;
				marker.color[0] = p_color[0];
				marker.color[1] = p_color[1];
				marker.color[2] = p_color[2];
				marker.color[3] = p_color[3];
				vkCmdDebugMarkerBeginEXT(command_buffer->vk_command_buffer, &marker);
			}
			return;
		}
		VkDebugUtilsLabelEXT label;
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pNext = nullptr;
		label.pLabelName = p_label_name;
		label.color[0] = p_color[0];
		label.color[1] = p_color[1];
		label.color[2] = p_color[2];
		label.color[3] = p_color[3];
		vkCmdBeginDebugUtilsLabelEXT(command_buffer->vk_command_buffer, &label);
	}

	void RenderingDeviceDriverVulkan::command_end_label(CommandBufferID p_cmd_buffer) {
		const CommandBufferInfo* command_buffer = (const CommandBufferInfo*)p_cmd_buffer.id;
		
		if (!vkCmdEndDebugUtilsLabelEXT) {
			if (vkCmdDebugMarkerEndEXT) {
				// Debug marker extensions.
				vkCmdDebugMarkerEndEXT(command_buffer->vk_command_buffer);
			}
			return;
		}
		vkCmdEndDebugUtilsLabelEXT(command_buffer->vk_command_buffer);
	}

#pragma endregion

#pragma region Misc
	const RenderingShaderContainerFormat& RenderingDeviceDriverVulkan::get_shader_container_format() const {
		return shader_container_format;
	}

	void RenderingDeviceDriverVulkan::set_object_name(ObjectType p_type, ID p_driver_id, const std::string& p_name) {
		switch (p_type) {
		case OBJECT_TYPE_TEXTURE: {
			const TextureInfo* tex_info = (const TextureInfo*)p_driver_id.id;
			if (tex_info->allocation.handle) {
				_set_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)tex_info->vk_view_create_info.image, p_name);
			}
			_set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)tex_info->vk_view, p_name + " View");
		} break;
		case OBJECT_TYPE_SAMPLER: {
			_set_object_name(VK_OBJECT_TYPE_SAMPLER, p_driver_id.id, p_name);
		} break;
		case OBJECT_TYPE_BUFFER: {
			const BufferInfo* buf_info = (const BufferInfo*)p_driver_id.id;
			_set_object_name(VK_OBJECT_TYPE_BUFFER, (uint64_t)buf_info->vk_buffer, p_name);
			if (buf_info->vk_view) {
				_set_object_name(VK_OBJECT_TYPE_BUFFER_VIEW, (uint64_t)buf_info->vk_view, p_name + " View");
			}
		} break;
		case OBJECT_TYPE_SHADER: {
			const ShaderInfo* shader_info = (const ShaderInfo*)p_driver_id.id;
			for (uint32_t i = 0; i < shader_info->vk_descriptor_set_layouts.size(); i++) {
				_set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)shader_info->vk_descriptor_set_layouts[i], p_name);
			}
			_set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)shader_info->vk_pipeline_layout, p_name + " Pipeline Layout");
		} break;
		case OBJECT_TYPE_UNIFORM_SET: {
			const UniformSetInfo* usi = (const UniformSetInfo*)p_driver_id.id;
			_set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)usi->vk_descriptor_set, p_name);
		} break;
		case OBJECT_TYPE_PIPELINE: {
			_set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)p_driver_id.id, p_name);
		} break;
		//case OBJECT_TYPE_ACCELERATION_STRUCTURE: {
		//	const AccelerationStructureInfo* asi = (const AccelerationStructureInfo*)p_driver_id.id;
		//	_set_object_name(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, (uint64_t)asi->vk_acceleration_structure, p_name);
		//} break;
		//case OBJECT_TYPE_RAYTRACING_PIPELINE: {
		//	const RaytracingPipelineInfo* rpi = (const RaytracingPipelineInfo*)p_driver_id.id;
		//	_set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)rpi->vk_pipeline, p_name);
		//} break;
		default: {
			DEV_ASSERT(false);
		}
		}
	}

#pragma endregion


	RenderingDeviceDriverVulkan::~RenderingDeviceDriverVulkan()
	{
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		if (breadcrumb_buffer != BufferID()) {
			buffer_free(breadcrumb_buffer);
		}
#endif

		while (small_allocs_pools.size()) {
			std::unordered_map<uint32_t, VmaPool>::iterator E = small_allocs_pools.begin();
			vmaDestroyPool(allocator, E->second);
			small_allocs_pools.erase(E);
		}
		vmaDestroyAllocator(allocator);

		// Destroy linearly allocated descriptor pools.
		for (std::pair<int, DescriptorSetPools> pool_map : linear_descriptor_set_pools) {
			for (std::pair<DescriptorSetPoolKey, std::unordered_map<VkDescriptorPool, uint32_t>> pools : pool_map.second) {
				for (std::pair<VkDescriptorPool, uint32_t> descriptor_pool : pools.second) {
					vkDestroyDescriptorPool(vk_device, descriptor_pool.first, nullptr);
				}
			}
		}

		if (vk_device != VK_NULL_HANDLE) {
			vkDestroyDevice(vk_device, nullptr);
		}
	}
}
