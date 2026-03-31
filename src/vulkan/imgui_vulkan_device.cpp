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

	ImGuiDevice::ImGuiDevice(WindowPlatformData p_platform_data, RenderingContextDriver* p_vulkan_context, RenderingDeviceDriver* p_vulkan_driver) :
		vulkan_context(static_cast<RenderingContextDriverVulkan*>(p_vulkan_context)),
		vulkan_driver(static_cast<RenderingDeviceDriverVulkan*>(p_vulkan_driver)),
		platform_data(p_platform_data)
	{

	}


	ImGuiDevice::~ImGuiDevice()
	{

	}

	Error ImGuiDevice::initialize(const uint32_t p_device_index, const uint32_t p_queue_family,
		const uint32_t p_min_image_count, const uint32_t p_swapchain_image_count, const RenderingDeviceCommons::DataFormat p_swapchain_format,
		std::span<RenderingDeviceDriver::TextureID> p_attachments, uint32_t width, uint32_t height)
	{
		ImGui::CreateContext();

		ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(platform_data.sdl.window));

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


		renderbuffer = _create_render_pass(vulkan_driver->vulkan_device_get(), RD_TO_VK_FORMAT[p_swapchain_format]);

		render_pass_device_info.vk_render_pass = vk_renderpass;

		ImGui_ImplVulkan_InitInfo init_info = {};
		//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
		init_info.Instance = vulkan_context->instance_get();
		init_info.PhysicalDevice = vulkan_context->physical_device_get(p_device_index);
		init_info.Device = vulkan_driver->vulkan_device_get();
		init_info.QueueFamily = p_queue_family;// vulkan_driver->command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT, p_surface_id).id - 1; // Since 0 is a valid index and we use 0 as the error case
		init_info.Queue = vulkan_driver->_get_vk_queue(init_info.QueueFamily, 0);
		init_info.PipelineCache = vulkan_driver->pipelines_cache.vk_cache;
		init_info.DescriptorPool = descriptor_pool;
		init_info.MinImageCount = p_min_image_count;
		init_info.ImageCount = p_swapchain_image_count;
		init_info.Allocator = nullptr;
		init_info.PipelineInfoMain.RenderPass = vk_renderpass;// ((RenderingDeviceDriverVulkan::RenderPassInfo*)(renderbuffer.id))->vk_render_pass;
		init_info.PipelineInfoMain.Subpass = 0;
		init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info);

		framebuffer = _create_imgui_framebuffers(vk_renderpass, p_attachments, width, height);
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

	void ImGuiDevice::show_demo_window()
	{
		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		bool show_demo_window;
		bool show_another_window;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}
	}

	void ImGuiDevice::execute(void* p_draw_data, RenderingDeviceDriverVulkan::CommandBufferID p_command_buffer, RenderingDeviceDriverVulkan::PipelineID p_pipeline)
	{
		RenderingDeviceDriverVulkan::CommandBufferInfo* command_buffer = (RenderingDeviceDriverVulkan::CommandBufferInfo*)(p_command_buffer.id);
		VkPipeline pipeline = (VkPipeline)(p_pipeline.id);
		ImGui_ImplVulkan_RenderDrawData((ImDrawData*)p_draw_data, command_buffer->vk_command_buffer, pipeline);
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

	RenderingDeviceDriver::RenderPassID ImGuiDevice::_create_render_pass(VkDevice device, VkFormat swapchainFormat)
	{
		VkAttachmentDescription color_attachment{};
		color_attachment.format = swapchainFormat;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // IMPORTANT: keep existing image
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// This layout is specifically optimised for images being written to as a color render target — i.e. when the image is bound as a color attachment in a framebuffer during a render pass.
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; will be transitioned by the barrier

		VkAttachmentReference color_ref{};
		color_ref.attachment = 0;
		color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Subpass
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_ref;

		// Subpass dependency (external -> imgui pass)
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
									VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;;
		dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
									VK_ACCESS_SHADER_READ_BIT;

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &render_pass_info, nullptr, &vk_renderpass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui render pass");
		}
		DEBUG_ASSERT(vk_renderpass != VK_NULL_HANDLE);

		RenderingDeviceDriverVulkan::RenderPassInfo render_pass_device_info;
		render_pass_device_info.vk_render_pass = vk_renderpass;
		RenderingDeviceDriver::RenderPassID render_pass_id(&render_pass_device_info);

		return render_pass_id;
	}

	RenderingDeviceDriver::FramebufferID ImGuiDevice::_create_imgui_framebuffers(VkRenderPass p_render_pass, std::span<RenderingDeviceDriver::TextureID> p_attachments, uint32_t p_width, uint32_t p_height)
	{
		RenderingDeviceDriver::RenderPassID render_pass_id(&render_pass_device_info);
		return vulkan_driver->framebuffer_create(render_pass_id, p_attachments, p_width, p_height);

	}



}

