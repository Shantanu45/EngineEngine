#include "vulkan_device.h"
#include "libassert/assert.hpp"
#include "util/error_macros.h"

namespace Vulkan
{

	Device::Device(Context* p_context_driver)
	{
		DEBUG_ASSERT(p_context_driver != nullptr);

		context_driver = p_context_driver;
		//TODO:
		//max_descriptor_sets_per_pool = 3;
	}

	Error Device::initialize(uint32_t p_device_index, uint32_t p_frame_count)
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

	void Device::_register_requested_device_extension(const std::string& p_extension_name, bool p_required) {
		ERR_FAIL_COND(requested_device_extensions.contains(p_extension_name));
		requested_device_extensions[p_extension_name] = p_required;
	}

	Error Device::_initialize_device_extensions() {
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
			LOGI(String("VULKAN: Found device extension ") + String::utf8(device_extensions[i].extensionName));
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
					LOGI("Optional extension %s not found.", requested_extension.first);
				}
			}
		}

		return OK;
	}

	Error Device::_check_device_features() {
		vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

		// Check for required features.
		if (!physical_device_features.imageCubeArray || !physical_device_features.independentBlend) {
			std::string error_string = std::format("Your GPU (%s) does not support the following features which are required to use Vulkan-based renderers in Godot:\n\n", context_device.name);
			if (!physical_device_features.imageCubeArray) {
				error_string += "- No support for image cube arrays.\n";
			}
			if (!physical_device_features.independentBlend) {
				error_string += "- No support for independentBlend.\n";
			}
			error_string += "\nThis is usually a hardware limitation, so updating graphics drivers won't help in most cases.";


			LOGE("%s\n",error_string);

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

	Error Device::_check_device_capabilities() {
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

	Error Device::_add_queue_create_info(std::vector<VkDeviceQueueCreateInfo>& r_queue_create_info) {
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

	Error Device::_initialize_device(const std::vector<VkDeviceQueueCreateInfo>& p_queue_create_info) {
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

	Error Device::_initialize_allocator() {
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
		VkResult err = vmaCreateAllocator(&allocator_info, &allocator);
		std::string msg = "vmaCreateAllocator failed with error " + std::to_string(err) + ".";
		ERR_FAIL_COND_V_MSG(err, ERR_CANT_CREATE, msg);

		return OK;
	}

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

	static void _convert_subpass_attachments(const VkAttachmentReference2* p_attachment_references_2, uint32_t p_attachment_references_count, std::vector<VkAttachmentReference>& r_attachment_references) {
		r_attachment_references.resize(p_attachment_references_count);
		for (uint32_t i = 0; i < p_attachment_references_count; i++) {
			// Ignore sType, pNext and aspectMask (which is currently unused).
			r_attachment_references[i].attachment = p_attachment_references_2[i].attachment;
			r_attachment_references[i].layout = p_attachment_references_2[i].layout;
		}
	}

	Error Device::_initialize_pipeline_cache() {
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

	VkResult Device::_create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2* p_create_info, const VkAllocationCallbacks* p_allocator, VkRenderPass* p_render_pass) {
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


	// TODO: do I need to use vold device to call vulkan functions!!??

	/****************/
	/**** MEMORY ****/
	/****************/

	static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

	VmaPool Device::_find_or_create_small_allocs_pool(uint32_t p_mem_type_index) {
		if (small_allocs_pools.contains(p_mem_type_index)) {
			return small_allocs_pools[p_mem_type_index];
		}

		LOGI("Creating VMA small objects pool for memory type index %s", std::to_string(p_mem_type_index));

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
}
