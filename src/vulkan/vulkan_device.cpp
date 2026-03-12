#include "vulkan_device.h"
#include "libassert/assert.hpp"
#include "util/error_macros.h"
#include <array>

namespace Vulkan
{
#pragma region Generic
	static const VkFormat RD_TO_VK_FORMAT[DATA_FORMAT_MAX] = {
	VK_FORMAT_R4G4_UNORM_PACK8,
	VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	VK_FORMAT_B4G4R4A4_UNORM_PACK16,
	VK_FORMAT_R5G6B5_UNORM_PACK16,
	VK_FORMAT_B5G6R5_UNORM_PACK16,
	VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	VK_FORMAT_B5G5R5A1_UNORM_PACK16,
	VK_FORMAT_A1R5G5B5_UNORM_PACK16,
	VK_FORMAT_R8_UNORM,
	VK_FORMAT_R8_SNORM,
	VK_FORMAT_R8_USCALED,
	VK_FORMAT_R8_SSCALED,
	VK_FORMAT_R8_UINT,
	VK_FORMAT_R8_SINT,
	VK_FORMAT_R8_SRGB,
	VK_FORMAT_R8G8_UNORM,
	VK_FORMAT_R8G8_SNORM,
	VK_FORMAT_R8G8_USCALED,
	VK_FORMAT_R8G8_SSCALED,
	VK_FORMAT_R8G8_UINT,
	VK_FORMAT_R8G8_SINT,
	VK_FORMAT_R8G8_SRGB,
	VK_FORMAT_R8G8B8_UNORM,
	VK_FORMAT_R8G8B8_SNORM,
	VK_FORMAT_R8G8B8_USCALED,
	VK_FORMAT_R8G8B8_SSCALED,
	VK_FORMAT_R8G8B8_UINT,
	VK_FORMAT_R8G8B8_SINT,
	VK_FORMAT_R8G8B8_SRGB,
	VK_FORMAT_B8G8R8_UNORM,
	VK_FORMAT_B8G8R8_SNORM,
	VK_FORMAT_B8G8R8_USCALED,
	VK_FORMAT_B8G8R8_SSCALED,
	VK_FORMAT_B8G8R8_UINT,
	VK_FORMAT_B8G8R8_SINT,
	VK_FORMAT_B8G8R8_SRGB,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8B8A8_SNORM,
	VK_FORMAT_R8G8B8A8_USCALED,
	VK_FORMAT_R8G8B8A8_SSCALED,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R8G8B8A8_SINT,
	VK_FORMAT_R8G8B8A8_SRGB,
	VK_FORMAT_B8G8R8A8_UNORM,
	VK_FORMAT_B8G8R8A8_SNORM,
	VK_FORMAT_B8G8R8A8_USCALED,
	VK_FORMAT_B8G8R8A8_SSCALED,
	VK_FORMAT_B8G8R8A8_UINT,
	VK_FORMAT_B8G8R8A8_SINT,
	VK_FORMAT_B8G8R8A8_SRGB,
	VK_FORMAT_A8B8G8R8_UNORM_PACK32,
	VK_FORMAT_A8B8G8R8_SNORM_PACK32,
	VK_FORMAT_A8B8G8R8_USCALED_PACK32,
	VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
	VK_FORMAT_A8B8G8R8_UINT_PACK32,
	VK_FORMAT_A8B8G8R8_SINT_PACK32,
	VK_FORMAT_A8B8G8R8_SRGB_PACK32,
	VK_FORMAT_A2R10G10B10_UNORM_PACK32,
	VK_FORMAT_A2R10G10B10_SNORM_PACK32,
	VK_FORMAT_A2R10G10B10_USCALED_PACK32,
	VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
	VK_FORMAT_A2R10G10B10_UINT_PACK32,
	VK_FORMAT_A2R10G10B10_SINT_PACK32,
	VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	VK_FORMAT_A2B10G10R10_SNORM_PACK32,
	VK_FORMAT_A2B10G10R10_USCALED_PACK32,
	VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
	VK_FORMAT_A2B10G10R10_UINT_PACK32,
	VK_FORMAT_A2B10G10R10_SINT_PACK32,
	VK_FORMAT_R16_UNORM,
	VK_FORMAT_R16_SNORM,
	VK_FORMAT_R16_USCALED,
	VK_FORMAT_R16_SSCALED,
	VK_FORMAT_R16_UINT,
	VK_FORMAT_R16_SINT,
	VK_FORMAT_R16_SFLOAT,
	VK_FORMAT_R16G16_UNORM,
	VK_FORMAT_R16G16_SNORM,
	VK_FORMAT_R16G16_USCALED,
	VK_FORMAT_R16G16_SSCALED,
	VK_FORMAT_R16G16_UINT,
	VK_FORMAT_R16G16_SINT,
	VK_FORMAT_R16G16_SFLOAT,
	VK_FORMAT_R16G16B16_UNORM,
	VK_FORMAT_R16G16B16_SNORM,
	VK_FORMAT_R16G16B16_USCALED,
	VK_FORMAT_R16G16B16_SSCALED,
	VK_FORMAT_R16G16B16_UINT,
	VK_FORMAT_R16G16B16_SINT,
	VK_FORMAT_R16G16B16_SFLOAT,
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R16G16B16A16_SNORM,
	VK_FORMAT_R16G16B16A16_USCALED,
	VK_FORMAT_R16G16B16A16_SSCALED,
	VK_FORMAT_R16G16B16A16_UINT,
	VK_FORMAT_R16G16B16A16_SINT,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_FORMAT_R32_UINT,
	VK_FORMAT_R32_SINT,
	VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R32G32_UINT,
	VK_FORMAT_R32G32_SINT,
	VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32G32B32_UINT,
	VK_FORMAT_R32G32B32_SINT,
	VK_FORMAT_R32G32B32_SFLOAT,
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R32G32B32A32_SINT,
	VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_R64_UINT,
	VK_FORMAT_R64_SINT,
	VK_FORMAT_R64_SFLOAT,
	VK_FORMAT_R64G64_UINT,
	VK_FORMAT_R64G64_SINT,
	VK_FORMAT_R64G64_SFLOAT,
	VK_FORMAT_R64G64B64_UINT,
	VK_FORMAT_R64G64B64_SINT,
	VK_FORMAT_R64G64B64_SFLOAT,
	VK_FORMAT_R64G64B64A64_UINT,
	VK_FORMAT_R64G64B64A64_SINT,
	VK_FORMAT_R64G64B64A64_SFLOAT,
	VK_FORMAT_B10G11R11_UFLOAT_PACK32,
	VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
	VK_FORMAT_D16_UNORM,
	VK_FORMAT_X8_D24_UNORM_PACK32,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_BC1_RGB_UNORM_BLOCK,
	VK_FORMAT_BC1_RGB_SRGB_BLOCK,
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
	VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
	VK_FORMAT_BC2_UNORM_BLOCK,
	VK_FORMAT_BC2_SRGB_BLOCK,
	VK_FORMAT_BC3_UNORM_BLOCK,
	VK_FORMAT_BC3_SRGB_BLOCK,
	VK_FORMAT_BC4_UNORM_BLOCK,
	VK_FORMAT_BC4_SNORM_BLOCK,
	VK_FORMAT_BC5_UNORM_BLOCK,
	VK_FORMAT_BC5_SNORM_BLOCK,
	VK_FORMAT_BC6H_UFLOAT_BLOCK,
	VK_FORMAT_BC6H_SFLOAT_BLOCK,
	VK_FORMAT_BC7_UNORM_BLOCK,
	VK_FORMAT_BC7_SRGB_BLOCK,
	VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
	VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
	VK_FORMAT_EAC_R11_UNORM_BLOCK,
	VK_FORMAT_EAC_R11_SNORM_BLOCK,
	VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
	VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
	VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
	VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
	VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
	VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
	VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
	VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
	VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
	VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
	VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
	VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
	VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
	VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
	VK_FORMAT_G8B8G8R8_422_UNORM,
	VK_FORMAT_B8G8R8G8_422_UNORM,
	VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
	VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
	VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
	VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
	VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
	VK_FORMAT_R10X6_UNORM_PACK16,
	VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
	VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
	VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
	VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
	VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
	VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
	VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
	VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
	VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
	VK_FORMAT_R12X4_UNORM_PACK16,
	VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
	VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
	VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
	VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
	VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
	VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
	VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
	VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
	VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
	VK_FORMAT_G16B16G16R16_422_UNORM,
	VK_FORMAT_B16G16R16G16_422_UNORM,
	VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
	VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
	VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
	VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
	VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
	VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK,
	VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK,
	};

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


#pragma endregion

#pragma region Device
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

	bool Device::_release_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain) {
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

	bool Device::_recreate_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain) {
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

	VkDebugReportObjectTypeEXT Device::_convert_to_debug_report_objectType(VkObjectType p_object_type) {
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
#pragma endregion

	// TODO: do I need to use vold device to call vulkan functions!!??

#pragma region Memory

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

#pragma endregion

#pragma region Buffer

	Device::BufferID Device::buffer_create(uint64_t p_size, BitField<Device::BufferUsageBits> p_usage, Device::MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn) {
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
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format( "Can't create buffer of size: %s , error %s", std::to_string(p_size), std::to_string(err)));
		}
		else {
			VkResult err = vkCreateBuffer(vk_device, &create_info, nullptr, &vk_buffer);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't create buffer of size: %s , error %s", std::to_string(p_size) + std::to_string(err)));
			err = vmaAllocateMemoryForBuffer(allocator, vk_buffer, &alloc_create_info, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't allocate memory for buffer of size: %s, error %s .", std::to_string(p_size), std::to_string(err)));
			err = vmaBindBufferMemory2(allocator, allocation, 0, vk_buffer, nullptr);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("Can't bind memory to buffer of size: %s, error %s .", std::to_string(p_size), std::to_string(err)));
		}

		// Bookkeep.
		BufferInfo* buf_info;
		if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			void* persistent_ptr = nullptr;
			VkResult err = vmaMapMemory(allocator, allocation, &persistent_ptr);
			ERR_FAIL_COND_V_MSG(err, BufferID(), std::format("vmaMapMemory failed with error %s .", std::to_string(err)));

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

	void Device::buffer_free(BufferID p_buffer) {
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

	bool Device::buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) {
		BufferInfo* buf_info = (BufferInfo*)p_buffer.id;

		DEV_ASSERT(!buf_info->vk_view);

		VkBufferViewCreateInfo view_create_info = {};
		view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view_create_info.buffer = buf_info->vk_buffer;
		view_create_info.format = RD_TO_VK_FORMAT[p_format];
		view_create_info.range = buf_info->allocation.size;

		VkResult res = vkCreateBufferView(vk_device, &view_create_info, nullptr, &buf_info->vk_view);
		ERR_FAIL_COND_V_MSG(res, false, std::format("Unable to create buffer view, error %s .", std::to_string(res)));

		return true;
	}

	uint64_t Device::buffer_get_allocation_size(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		return buf_info->allocation.size;
	}

	uint8_t* Device::buffer_map(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		ERR_FAIL_COND_V_MSG(buf_info->is_dynamic(), nullptr, "Buffer must NOT have BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT. Use buffer_persistent_map_advance() instead.");
		void* data_ptr = nullptr;
		VkResult err = vmaMapMemory(allocator, buf_info->allocation.handle, &data_ptr);
		ERR_FAIL_COND_V_MSG(err, nullptr, std::format("vmaMapMemory failed with error %s .", std::to_string(err)));
		return (uint8_t*)data_ptr;
	}

	void Device::buffer_unmap(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		vmaUnmapMemory(allocator, buf_info->allocation.handle);
	}

	uint8_t* Device::buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) {
		BufferDynamicInfo* buf_info = (BufferDynamicInfo*)p_buffer.id;
		ERR_FAIL_COND_V_MSG(!buf_info->is_dynamic(), nullptr, "Buffer must have BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT. Use buffer_map() instead.");
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(buf_info->last_frame_mapped == p_frames_drawn, nullptr, "Buffers with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT must only be mapped once per frame. Otherwise there could be race conditions with the GPU. Amalgamate all data uploading into one map(), use an extra buffer or remove the bit.");
		buf_info->last_frame_mapped = p_frames_drawn;
#endif
		buf_info->frame_idx = (buf_info->frame_idx + 1u) % frame_count;
		return buf_info->persistent_ptr + buf_info->frame_idx * buf_info->size;
	}

	uint64_t Device::buffer_get_dynamic_offsets(std::span<BufferID> p_buffers) {
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

	void Device::buffer_flush(BufferID p_buffer) {
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

	uint64_t Device::buffer_get_device_address(BufferID p_buffer) {
		const BufferInfo* buf_info = (const BufferInfo*)p_buffer.id;
		VkBufferDeviceAddressInfo address_info = {};
		address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		address_info.pNext = nullptr;
		address_info.buffer = buf_info->vk_buffer;
		return vkGetBufferDeviceAddress(vk_device, &address_info);
	}

#pragma endregion

#pragma region Texture

	static const VkImageType RD_TEX_TYPE_TO_VK_IMG_TYPE[TEXTURE_TYPE_MAX] = {
	VK_IMAGE_TYPE_1D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_3D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_1D,
	VK_IMAGE_TYPE_2D,
	VK_IMAGE_TYPE_2D,
	};

	static const VkSampleCountFlagBits RD_TO_VK_SAMPLE_COUNT[TEXTURE_SAMPLES_MAX] = {
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT,
	};

	VkSampleCountFlagBits Device::_ensure_supported_sample_count(TextureSamples p_requested_sample_count) {
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

	Device::TextureID Device::texture_create(const TextureFormat& p_format, const TextureView& p_view) {
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
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vmaCreateImage failed with error %s .", std::to_string(err)));
		}
		else {
			VkResult err = vkCreateImage(vk_device, &create_info, nullptr, &vk_image);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImage failed with error %s", std::to_string(err)));
			err = vmaAllocateMemoryForImage(allocator, vk_image, &alloc_create_info, &allocation, &alloc_info);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("Can't allocate memory for image, error: %s", std::to_string(err)));
			err = vmaBindImageMemory2(allocator, allocation, 0, vk_image, nullptr);
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("Can't bind memory to image, error: %s", std::to_string(err)));
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
				vkDestroyImage(vk_device, vk_image, nullptr);
				vmaFreeMemory(allocator, allocation);
			}

			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error %s .", std::to_string(err)));
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
		print_line(vformat("vkCreateImageView: 0x%uX for 0x%uX", uint64_t(vk_image_view), uint64_t(vk_image)));
#endif

		return TextureID(tex_info);
	}

	uint32_t Device::get_image_format_required_size(DataFormat p_format, uint32_t p_width, uint32_t p_height, uint32_t p_depth, uint32_t p_mipmaps, uint32_t* r_blockw, uint32_t* r_blockh, uint32_t* r_depth) {
		ERR_FAIL_COND_V(p_mipmaps == 0, 0);
		uint32_t w = p_width;
		uint32_t h = p_height;
		uint32_t d = p_depth;

		uint32_t size = 0;

		uint32_t pixel_size = get_image_format_pixel_size(p_format);
		uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(p_format);
		uint32_t blockw = 0;
		uint32_t blockh = 0;
		get_compressed_image_format_block_dimensions(p_format, blockw, blockh);

		for (uint32_t i = 0; i < p_mipmaps; i++) {
			uint32_t bw = STEPIFY(w, blockw);
			uint32_t bh = STEPIFY(h, blockh);

			uint32_t s = bw * bh;

			s *= pixel_size;
			s >>= pixel_rshift;
			size += s * d;
			if (r_blockw) {
				*r_blockw = bw;
			}
			if (r_blockh) {
				*r_blockh = bh;
			}
			if (r_depth) {
				*r_depth = d;
			}
			w = MAX(blockw, w >> 1);
			h = MAX(blockh, h >> 1);
			d = MAX(1u, d >> 1);
		}

		return size;
	}

	uint32_t Device::get_image_format_pixel_size(DataFormat p_format) {
		switch (p_format) {
		case DATA_FORMAT_R4G4_UNORM_PACK8:
			return 1;
		case DATA_FORMAT_R4G4B4A4_UNORM_PACK16:
		case DATA_FORMAT_B4G4R4A4_UNORM_PACK16:
		case DATA_FORMAT_R5G6B5_UNORM_PACK16:
		case DATA_FORMAT_B5G6R5_UNORM_PACK16:
		case DATA_FORMAT_R5G5B5A1_UNORM_PACK16:
		case DATA_FORMAT_B5G5R5A1_UNORM_PACK16:
		case DATA_FORMAT_A1R5G5B5_UNORM_PACK16:
			return 2;
		case DATA_FORMAT_R8_UNORM:
		case DATA_FORMAT_R8_SNORM:
		case DATA_FORMAT_R8_USCALED:
		case DATA_FORMAT_R8_SSCALED:
		case DATA_FORMAT_R8_UINT:
		case DATA_FORMAT_R8_SINT:
		case DATA_FORMAT_R8_SRGB:
			return 1;
		case DATA_FORMAT_R8G8_UNORM:
		case DATA_FORMAT_R8G8_SNORM:
		case DATA_FORMAT_R8G8_USCALED:
		case DATA_FORMAT_R8G8_SSCALED:
		case DATA_FORMAT_R8G8_UINT:
		case DATA_FORMAT_R8G8_SINT:
		case DATA_FORMAT_R8G8_SRGB:
			return 2;
		case DATA_FORMAT_R8G8B8_UNORM:
		case DATA_FORMAT_R8G8B8_SNORM:
		case DATA_FORMAT_R8G8B8_USCALED:
		case DATA_FORMAT_R8G8B8_SSCALED:
		case DATA_FORMAT_R8G8B8_UINT:
		case DATA_FORMAT_R8G8B8_SINT:
		case DATA_FORMAT_R8G8B8_SRGB:
		case DATA_FORMAT_B8G8R8_UNORM:
		case DATA_FORMAT_B8G8R8_SNORM:
		case DATA_FORMAT_B8G8R8_USCALED:
		case DATA_FORMAT_B8G8R8_SSCALED:
		case DATA_FORMAT_B8G8R8_UINT:
		case DATA_FORMAT_B8G8R8_SINT:
		case DATA_FORMAT_B8G8R8_SRGB:
			return 3;
		case DATA_FORMAT_R8G8B8A8_UNORM:
		case DATA_FORMAT_R8G8B8A8_SNORM:
		case DATA_FORMAT_R8G8B8A8_USCALED:
		case DATA_FORMAT_R8G8B8A8_SSCALED:
		case DATA_FORMAT_R8G8B8A8_UINT:
		case DATA_FORMAT_R8G8B8A8_SINT:
		case DATA_FORMAT_R8G8B8A8_SRGB:
		case DATA_FORMAT_B8G8R8A8_UNORM:
		case DATA_FORMAT_B8G8R8A8_SNORM:
		case DATA_FORMAT_B8G8R8A8_USCALED:
		case DATA_FORMAT_B8G8R8A8_SSCALED:
		case DATA_FORMAT_B8G8R8A8_UINT:
		case DATA_FORMAT_B8G8R8A8_SINT:
		case DATA_FORMAT_B8G8R8A8_SRGB:
			return 4;
		case DATA_FORMAT_A8B8G8R8_UNORM_PACK32:
		case DATA_FORMAT_A8B8G8R8_SNORM_PACK32:
		case DATA_FORMAT_A8B8G8R8_USCALED_PACK32:
		case DATA_FORMAT_A8B8G8R8_SSCALED_PACK32:
		case DATA_FORMAT_A8B8G8R8_UINT_PACK32:
		case DATA_FORMAT_A8B8G8R8_SINT_PACK32:
		case DATA_FORMAT_A8B8G8R8_SRGB_PACK32:
		case DATA_FORMAT_A2R10G10B10_UNORM_PACK32:
		case DATA_FORMAT_A2R10G10B10_SNORM_PACK32:
		case DATA_FORMAT_A2R10G10B10_USCALED_PACK32:
		case DATA_FORMAT_A2R10G10B10_SSCALED_PACK32:
		case DATA_FORMAT_A2R10G10B10_UINT_PACK32:
		case DATA_FORMAT_A2R10G10B10_SINT_PACK32:
		case DATA_FORMAT_A2B10G10R10_UNORM_PACK32:
		case DATA_FORMAT_A2B10G10R10_SNORM_PACK32:
		case DATA_FORMAT_A2B10G10R10_USCALED_PACK32:
		case DATA_FORMAT_A2B10G10R10_SSCALED_PACK32:
		case DATA_FORMAT_A2B10G10R10_UINT_PACK32:
		case DATA_FORMAT_A2B10G10R10_SINT_PACK32:
			return 4;
		case DATA_FORMAT_R16_UNORM:
		case DATA_FORMAT_R16_SNORM:
		case DATA_FORMAT_R16_USCALED:
		case DATA_FORMAT_R16_SSCALED:
		case DATA_FORMAT_R16_UINT:
		case DATA_FORMAT_R16_SINT:
		case DATA_FORMAT_R16_SFLOAT:
			return 2;
		case DATA_FORMAT_R16G16_UNORM:
		case DATA_FORMAT_R16G16_SNORM:
		case DATA_FORMAT_R16G16_USCALED:
		case DATA_FORMAT_R16G16_SSCALED:
		case DATA_FORMAT_R16G16_UINT:
		case DATA_FORMAT_R16G16_SINT:
		case DATA_FORMAT_R16G16_SFLOAT:
			return 4;
		case DATA_FORMAT_R16G16B16_UNORM:
		case DATA_FORMAT_R16G16B16_SNORM:
		case DATA_FORMAT_R16G16B16_USCALED:
		case DATA_FORMAT_R16G16B16_SSCALED:
		case DATA_FORMAT_R16G16B16_UINT:
		case DATA_FORMAT_R16G16B16_SINT:
		case DATA_FORMAT_R16G16B16_SFLOAT:
			return 6;
		case DATA_FORMAT_R16G16B16A16_UNORM:
		case DATA_FORMAT_R16G16B16A16_SNORM:
		case DATA_FORMAT_R16G16B16A16_USCALED:
		case DATA_FORMAT_R16G16B16A16_SSCALED:
		case DATA_FORMAT_R16G16B16A16_UINT:
		case DATA_FORMAT_R16G16B16A16_SINT:
		case DATA_FORMAT_R16G16B16A16_SFLOAT:
			return 8;
		case DATA_FORMAT_R32_UINT:
		case DATA_FORMAT_R32_SINT:
		case DATA_FORMAT_R32_SFLOAT:
			return 4;
		case DATA_FORMAT_R32G32_UINT:
		case DATA_FORMAT_R32G32_SINT:
		case DATA_FORMAT_R32G32_SFLOAT:
			return 8;
		case DATA_FORMAT_R32G32B32_UINT:
		case DATA_FORMAT_R32G32B32_SINT:
		case DATA_FORMAT_R32G32B32_SFLOAT:
			return 12;
		case DATA_FORMAT_R32G32B32A32_UINT:
		case DATA_FORMAT_R32G32B32A32_SINT:
		case DATA_FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case DATA_FORMAT_R64_UINT:
		case DATA_FORMAT_R64_SINT:
		case DATA_FORMAT_R64_SFLOAT:
			return 8;
		case DATA_FORMAT_R64G64_UINT:
		case DATA_FORMAT_R64G64_SINT:
		case DATA_FORMAT_R64G64_SFLOAT:
			return 16;
		case DATA_FORMAT_R64G64B64_UINT:
		case DATA_FORMAT_R64G64B64_SINT:
		case DATA_FORMAT_R64G64B64_SFLOAT:
			return 24;
		case DATA_FORMAT_R64G64B64A64_UINT:
		case DATA_FORMAT_R64G64B64A64_SINT:
		case DATA_FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		case DATA_FORMAT_B10G11R11_UFLOAT_PACK32:
		case DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32:
			return 4;
		case DATA_FORMAT_D16_UNORM:
			return 2;
		case DATA_FORMAT_X8_D24_UNORM_PACK32:
			return 4;
		case DATA_FORMAT_D32_SFLOAT:
			return 4;
		case DATA_FORMAT_S8_UINT:
			return 1;
		case DATA_FORMAT_D16_UNORM_S8_UINT:
			return 4;
		case DATA_FORMAT_D24_UNORM_S8_UINT:
			return 4;
		case DATA_FORMAT_D32_SFLOAT_S8_UINT:
			return 5; // ?
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
			return 1;
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
			return 1;
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
			return 1;
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
			return 1;
		case DATA_FORMAT_G8B8G8R8_422_UNORM:
		case DATA_FORMAT_B8G8R8G8_422_UNORM:
			return 4;
		case DATA_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case DATA_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case DATA_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case DATA_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case DATA_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
			return 4;
		case DATA_FORMAT_R10X6_UNORM_PACK16:
		case DATA_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case DATA_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case DATA_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case DATA_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case DATA_FORMAT_R12X4_UNORM_PACK16:
		case DATA_FORMAT_R12X4G12X4_UNORM_2PACK16:
		case DATA_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case DATA_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case DATA_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
			return 2;
		case DATA_FORMAT_G16B16G16R16_422_UNORM:
		case DATA_FORMAT_B16G16R16G16_422_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case DATA_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case DATA_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
			return 8;
		default: {
			ERR_PRINT("Format not handled, bug");
		}
		}

		return 1;
	}

	uint32_t Device::get_compressed_image_format_pixel_rshift(DataFormat p_format) {
		switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK: // These formats are half byte size, so rshift is 1.
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
			return 1;
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK: {
			return 2;
		}
		default: {
		}
		}

		return 0;
	}

	void Device::get_compressed_image_format_block_dimensions(DataFormat p_format, uint32_t& r_w, uint32_t& r_h) {
		switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: // Again, not sure about astc.
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK: {
			r_w = 4;
			r_h = 4;
		} break;
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK: // Unsupported
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SFLOAT_BLOCK: {
			r_w = 4;
			r_h = 4;
		} break;
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK: {
			r_w = 8;
			r_h = 8;
		} break;
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK: // Unsupported
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
			r_w = 4;
			r_h = 4;
			return;
		default: {
			r_w = 1;
			r_h = 1;
		}
		}
	}

	Device::TextureID Device::texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps) {
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
			ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error %s .",std::to_string(err)));
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

	Device::TextureID Device::texture_create_shared(TextureID p_original_texture, const TextureView& p_view) {
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
		ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error %s .", std::to_string(err)));

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

	Device::TextureID Device::texture_create_shared_from_slice(TextureID p_original_texture, const TextureView& p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
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
		ERR_FAIL_COND_V_MSG(err, TextureID(), std::format("vkCreateImageView failed with error %s .", std::to_string(err)));

		// Bookkeep.

		TextureInfo* tex_info = new TextureInfo;
		*tex_info = *owner_tex_info;
		tex_info->vk_view = new_vk_image_view;
		tex_info->vk_view_create_info = image_view_create_info;
		tex_info->allocation = {};

#if PRINT_NATIVE_COMMANDS
		LOGI(std::format("vkCreateImageView: 0x%uX for 0x%uX (%d %d %d %d)", uint64_t(new_vk_image_view), uint64_t(owner_tex_info->vk_view_create_info.image), p_mipmap, p_mipmaps, p_layer, p_layers));
#endif

		return TextureID(tex_info);
	}

	void Device::texture_free(TextureID p_texture) {
		TextureInfo* tex_info = (TextureInfo*)p_texture.id;
		vkDestroyImageView(vk_device, tex_info->vk_view, nullptr);
		if (tex_info->allocation.handle) {
			if (false/*!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()*/) {
				vmaDestroyImage(allocator, tex_info->vk_view_create_info.image, tex_info->allocation.handle);
			}
			else {
				vkDestroyImage(vk_device, tex_info->vk_image, nullptr);
				vmaFreeMemory(allocator, tex_info->allocation.handle);
			}
		}
		delete tex_info;
	}

	uint64_t Device::texture_get_allocation_size(TextureID p_texture) {
		const TextureInfo* tex_info = (const TextureInfo*)p_texture.id;
		return tex_info->allocation.info.size;
	}

	void Device::texture_get_copyable_layout(TextureID p_texture, const TextureSubresource& p_subresource, TextureCopyableLayout* r_layout) {
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

	std::vector<uint8_t> Device::texture_get_data(TextureID p_texture, uint32_t p_layer) {
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
		ERR_FAIL_COND_V_MSG(err, std::vector<uint8_t>(), std::format("vmaMapMemory failed with error %s .", std::to_string(err)));

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

	uint32_t Device::get_compressed_image_format_block_byte_size(DataFormat p_format) const {
		switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
			return 8;
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
			return 16;
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
			return 16;
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
			return 8;
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
			return 16;
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: // Again, not sure about astc.
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
			return 16;
		default: {
		}
		}
		return 1;
	}

	BitField<TextureUsageBits> Device::texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) {
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

	bool Device::texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool& r_raw_reinterpretation) {
		r_raw_reinterpretation = false;
		return true;
	}

#pragma endregion

}
