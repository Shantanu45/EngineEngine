#include "imgui_vulkan_device.h"

namespace Vulkan
{
	static void check_vk_result(VkResult err)
	{
		if (err == VK_SUCCESS)
			return;

		LOGE("[imgui] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	ImGuiDevice::ImGuiDevice(SDL_Window* p_window, RenderingContextDriverVulkan* p_vulkan_context, RenderingDeviceDriverVulkan* p_vulkan_driver) :
		vulkan_context(p_vulkan_context),
		window(p_window),
		vulkan_driver(p_vulkan_driver)
	{

	}


	ImGuiDevice::~ImGuiDevice()
	{

	}

	Error ImGuiDevice::initaialize(const uint32_t p_device_index, const uint32_t p_surface_id, const VkQueue p_queue, 
		const VkPipelineCache p_pipeline_cache, const VkDescriptorPool p_descriptor_pool,
		const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const VkRenderPass p_render_pass, 
		const uint32_t subpass)
	{
		ImGui_ImplVulkan_InitInfo init_info = {};
		//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
		init_info.Instance = vulkan_context->instance_get();
		init_info.PhysicalDevice = vulkan_context->physical_device_get(p_device_index);
		init_info.Device = vulkan_driver->vulkan_device_get();
		init_info.QueueFamily = vulkan_driver->command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT, p_surface_id).id - 1; // Since 0 is a valid index and we use 0 as the error case
		init_info.Queue = p_queue;
		init_info.PipelineCache = p_pipeline_cache;
		init_info.DescriptorPool = p_descriptor_pool;
		init_info.MinImageCount = p_min_image_count;
		init_info.ImageCount = p_swapchain_image_count;
		init_info.Allocator = nullptr;
		init_info.PipelineInfoMain.RenderPass = p_render_pass;
		init_info.PipelineInfoMain.Subpass = subpass;
		init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info);
		return OK;
	}

}

