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

	Error ImGuiDevice::initialize(const uint32_t p_device_index, const uint32_t p_surface_id,
		const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const VkRenderPass p_render_pass, 
		const uint32_t subpass)
	{
		ImGui::CreateContext();

		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 0;
		for (VkDescriptorPoolSize& pool_size : pool_sizes)
			pool_info.maxSets += pool_size.descriptorCount;
		pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		auto err = vkCreateDescriptorPool(vulkan_driver->vulkan_device_get(), &pool_info, nullptr, &descriptor_pool);
		check_vk_result(err);

		ImGui_ImplVulkan_InitInfo init_info = {};
		//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
		init_info.Instance = vulkan_context->instance_get();
		init_info.PhysicalDevice = vulkan_context->physical_device_get(p_device_index);
		init_info.Device = vulkan_driver->vulkan_device_get();
		init_info.QueueFamily = vulkan_driver->command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT, p_surface_id).id - 1; // Since 0 is a valid index and we use 0 as the error case
		init_info.Queue = vulkan_driver->_get_vk_queue(init_info.QueueFamily, 0);
		init_info.PipelineCache = vulkan_driver->pipelines_cache.vk_cache;
		init_info.DescriptorPool = descriptor_pool;
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


	void ImGuiDevice::poll_event(SDL_Event* event)
	{
		ImGui_ImplSDL3_ProcessEvent(event);
	}

	void ImGuiDevice::begin_frame()
	{
		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiDevice::render()
	{
		ImGui::Render();
	}

	void ImGuiDevice::end_frame()
	{

	}

	void ImGuiDevice::finalize()
	{
		auto err = vkDeviceWaitIdle(vulkan_driver->vulkan_device_get());
		check_vk_result(err);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		if (descriptor_pool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(vulkan_driver->vulkan_device_get(), descriptor_pool, nullptr);
			descriptor_pool = VK_NULL_HANDLE;
		}
	}

}

