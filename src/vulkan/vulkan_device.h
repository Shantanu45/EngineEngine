#pragma once
#include <mutex>
#include "vulkan_context.h"
#include "vma/vk_mem_alloc.h"


namespace Vulkan
{
	class Device
	{
		struct ShaderCapabilities {
			bool shader_float16_is_supported = false;
			bool shader_int8_is_supported = false;
		};

		enum DeviceFamily {
			DEVICE_UNKNOWN,
			DEVICE_OPENGL,
			DEVICE_VULKAN,
			DEVICE_DIRECTX,
			DEVICE_METAL,
		};

		struct Capabilities {
			DeviceFamily device_family = DEVICE_UNKNOWN;
			uint32_t version_major = 1;
			uint32_t version_minor = 0;
		};

		struct Queue {
			VkQueue queue = VK_NULL_HANDLE;
			uint32_t virtual_count = 0;
			std::mutex submit_mutex;

		public:
			Queue() = default;

			// needed becuase mutex is non copyable, find alternative maybe
			Queue(Queue&& other) noexcept
				: queue(other.queue), virtual_count(other.virtual_count)
			{
			}

			Queue& operator=(Queue&& other) noexcept {
				queue = other.queue;
				virtual_count = other.virtual_count;
				return *this;
			}

			Queue(const Queue&) = delete;
			Queue& operator=(const Queue&) = delete;
		};

		struct PipelineCache {
			std::string file_path;
			size_t current_size = 0;
			std::vector<uint8_t> buffer; // Header then data.
			VkPipelineCache vk_cache = VK_NULL_HANDLE;
		};

		VkDevice vk_device = VK_NULL_HANDLE;

		Context* context_driver = nullptr;
		uint32_t max_descriptor_sets_per_pool = 0;
		Context::Device context_device = {};
		uint32_t frame_count = 1;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties physical_device_properties = {};
		VkPhysicalDeviceFeatures physical_device_features = {};
		VkPhysicalDeviceFeatures requested_device_features = {};

		std::unordered_map<std::string, bool> requested_device_extensions;
		std::set<std::string> enabled_device_extension_names;
		std::vector<std::vector<Queue>> queue_families;
		std::vector<VkQueueFamilyProperties> queue_family_properties;

		bool framebuffer_depth_resolve = false;


		Capabilities device_capabilities;
		ShaderCapabilities shader_capabilities;
		bool buffer_device_address_support = false;
		bool vulkan_memory_model_support = false;
		bool vulkan_memory_model_device_scope_support = false;
		bool pipeline_cache_control_support = false;
		bool device_fault_support = false;

		VmaAllocator allocator = nullptr;


		struct PipelineCacheHeader {
			uint32_t magic = 0;
			uint32_t data_size = 0;
			uint64_t data_hash = 0;
			uint32_t vendor_id = 0;
			uint32_t device_id = 0;
			uint32_t driver_version = 0;
			uint8_t uuid[VK_UUID_SIZE] = {};
			uint8_t driver_abi = 0;
		};

		PipelineCache pipelines_cache;
		std::string pipeline_cache_id;



		struct SwapChain;
		struct Fence;
		struct CommandQueue {
			std::vector<VkSemaphore> image_semaphores;
			std::vector<SwapChain*> image_semaphores_swap_chains;
			std::vector<uint32_t> pending_semaphores_for_execute;
			std::vector<uint32_t> pending_semaphores_for_fence;
			std::vector<uint32_t> free_image_semaphores;
			std::vector<std::pair<Fence*, uint32_t>> image_semaphores_for_fences;
			uint32_t queue_family = 0;
			uint32_t queue_index = 0;
		};

		struct Fence {
			VkFence vk_fence = VK_NULL_HANDLE;
			CommandQueue* queue_signaled_from = nullptr;
		};

		struct SwapChain {
			VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
			Context::SurfaceID surface = Context::SurfaceID();
			VkFormat format = VK_FORMAT_UNDEFINED;
			VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			std::vector<VkImage> images;
			std::vector<VkImageView> image_views;
			std::vector<VkSemaphore> present_semaphores;
			std::vector<FramebufferID> framebuffers;
			std::vector<CommandQueue*> command_queues_acquired;
			std::vector<uint32_t> command_queues_acquired_semaphores;
			RenderPassID render_pass;
			int pre_transform_rotation_degrees = 0;
			uint32_t image_index = 0;

		};

		bool _determine_swap_chain_format(Context::SurfaceID p_surface, VkFormat& r_format, VkColorSpaceKHR& r_color_space);
		void _swap_chain_release(SwapChain* p_swap_chain);

		private:
			/****************/
			/**** MEMORY ****/
			/****************/

			VmaAllocator allocator = nullptr;
			std::unordered_map<uint32_t, VmaPool> small_allocs_pools;

			VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);



	public:
		Device(Context* p_context_driver);
		Error initialize(uint32_t p_device_index, uint32_t p_frame_count);
		virtual ~Device();

	private:
		void _register_requested_device_extension(const std::string& p_extension_name, bool p_required);
		Error _initialize_device_extensions();
		Error _check_device_features();
		Error _check_device_capabilities();
		Error _add_queue_create_info(std::vector<VkDeviceQueueCreateInfo>& r_queue_create_info);
		Error _initialize_device(const std::vector<VkDeviceQueueCreateInfo>& p_queue_create_info);
		Error _initialize_allocator();
		Error _initialize_pipeline_cache();
		VkResult _create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2* p_create_info, const VkAllocationCallbacks* p_allocator, VkRenderPass* p_render_pass);
		VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);
	};
}
