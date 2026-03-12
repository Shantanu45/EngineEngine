#pragma once
#include <mutex>
#include <span>
#include "vulkan_context.h"
#include "vma/vk_mem_alloc.h"
#include "util/typedefs.h"
#include "util/bit_field.h"

namespace Vulkan
{
	class Device
	{
	public:
		struct ID {
			uint64_t id = 0;
			_ALWAYS_INLINE_ ID() = default;
			_ALWAYS_INLINE_ ID(uint64_t p_id) :
				id(p_id) {
			}
		};

#define DEFINE_ID(m_name) \
	struct m_name##ID : public ID { \
		_ALWAYS_INLINE_ explicit operator bool() const { \
			return id != 0; \
		} \
		_ALWAYS_INLINE_ m_name##ID &operator=(m_name##ID p_other) { \
			id = p_other.id; \
			return *this; \
		} \
		_ALWAYS_INLINE_ bool operator<(const m_name##ID &p_other) const { \
			return id < p_other.id; \
		} \
		_ALWAYS_INLINE_ bool operator==(const m_name##ID &p_other) const { \
			return id == p_other.id; \
		} \
		_ALWAYS_INLINE_ bool operator!=(const m_name##ID &p_other) const { \
			return id != p_other.id; \
		} \
		_ALWAYS_INLINE_ m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {} \
		_ALWAYS_INLINE_ explicit m_name##ID(uint64_t p_int) : ID(p_int) {} \
		_ALWAYS_INLINE_ explicit m_name##ID(void *p_ptr) : ID((uint64_t)p_ptr) {} \
		_ALWAYS_INLINE_ m_name##ID() = default; \
	};

		// Id types declared before anything else to prevent cyclic dependencies between the different concerns.
		DEFINE_ID(Buffer);
		DEFINE_ID(Texture);
		DEFINE_ID(Sampler);
		DEFINE_ID(VertexFormat);
		DEFINE_ID(CommandQueue);
		DEFINE_ID(CommandQueueFamily);
		DEFINE_ID(CommandPool);
		DEFINE_ID(CommandBuffer);
		DEFINE_ID(SwapChain);
		DEFINE_ID(Framebuffer);
		DEFINE_ID(Shader);
		DEFINE_ID(UniformSet);
		DEFINE_ID(Pipeline);
		DEFINE_ID(RenderPass);
		DEFINE_ID(QueryPool);
		DEFINE_ID(Fence);
		DEFINE_ID(Semaphore);

		/****************/
		/**** MEMORY ****/
		/****************/

		enum MemoryAllocationType {
			MEMORY_ALLOCATION_TYPE_CPU, // For images, CPU allocation also means linear, GPU is tiling optimal.
			MEMORY_ALLOCATION_TYPE_GPU,
		};

		/*****************/
		/**** BUFFERS ****/
		/*****************/

		enum BufferUsageBits {
			BUFFER_USAGE_TRANSFER_FROM_BIT = (1 << 0),
			BUFFER_USAGE_TRANSFER_TO_BIT = (1 << 1),
			BUFFER_USAGE_TEXEL_BIT = (1 << 2),
			BUFFER_USAGE_UNIFORM_BIT = (1 << 4),
			BUFFER_USAGE_STORAGE_BIT = (1 << 5),
			BUFFER_USAGE_INDEX_BIT = (1 << 6),
			BUFFER_USAGE_VERTEX_BIT = (1 << 7),
			BUFFER_USAGE_INDIRECT_BIT = (1 << 8),
			BUFFER_USAGE_SHADER_BINDING_TABLE_BIT = (1 << 10),
			BUFFER_USAGE_DEVICE_ADDRESS_BIT = (1 << 17),
			BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT = (1 << 19),
			BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT = (1 << 20),
			// There are no Vulkan-equivalent. Try to use unused/unclaimed bits.
			BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT = (1 << 31),
		};

		enum {
			BUFFER_WHOLE_SIZE = ~0ULL
		};
	private:
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

			/*****************/
			/**** BUFFERS ****/
			/*****************/
			struct BufferInfo {
				VkBuffer vk_buffer = VK_NULL_HANDLE;
				struct {
					VmaAllocation handle = nullptr;
					uint64_t size = UINT64_MAX;
				} allocation;
				uint64_t size = 0;
				VkBufferView vk_view = VK_NULL_HANDLE; // For texel buffers.
				// If dynamic buffer, then its range is [0; RenderingDeviceDriverVulkan::frame_count)
				// else it's UINT32_MAX.
				uint32_t frame_idx = UINT32_MAX;

				bool is_dynamic() const { return frame_idx != UINT32_MAX; }
			};

			struct BufferDynamicInfo : BufferInfo {
				uint8_t* persistent_ptr = nullptr;
#ifdef DEBUG_ENABLED
				// For tracking that a persistent buffer isn't mapped twice in the same frame.
				uint64_t last_frame_mapped = 0;
#endif
			};

			struct TextureFormat {
				DataFormat format = DATA_FORMAT_R8_UNORM;
				uint32_t width = 1;
				uint32_t height = 1;
				uint32_t depth = 1;
				uint32_t array_layers = 1;
				uint32_t mipmaps = 1;
				TextureType texture_type = TEXTURE_TYPE_2D;
				TextureSamples samples = TEXTURE_SAMPLES_1;
				uint32_t usage_bits = 0;
				std::vector<DataFormat> shareable_formats;
				bool is_resolve_buffer = false;
				bool is_discardable = false;

				bool operator==(const TextureFormat& b) const {
					if (format != b.format) {
						return false;
					}
					else if (width != b.width) {
						return false;
					}
					else if (height != b.height) {
						return false;
					}
					else if (depth != b.depth) {
						return false;
					}
					else if (array_layers != b.array_layers) {
						return false;
					}
					else if (mipmaps != b.mipmaps) {
						return false;
					}
					else if (texture_type != b.texture_type) {
						return false;
					}
					else if (samples != b.samples) {
						return false;
					}
					else if (usage_bits != b.usage_bits) {
						return false;
					}
					else if (shareable_formats != b.shareable_formats) {
						return false;
					}
					else if (is_resolve_buffer != b.is_resolve_buffer) {
						return false;
					}
					else if (is_discardable != b.is_discardable) {
						return false;
					}
					else {
						return true;
					}
				}
			};

			enum TextureSwizzle {
				TEXTURE_SWIZZLE_IDENTITY,
				TEXTURE_SWIZZLE_ZERO,
				TEXTURE_SWIZZLE_ONE,
				TEXTURE_SWIZZLE_R,
				TEXTURE_SWIZZLE_G,
				TEXTURE_SWIZZLE_B,
				TEXTURE_SWIZZLE_A,
				TEXTURE_SWIZZLE_MAX
			};

			enum TextureSliceType {
				TEXTURE_SLICE_2D,
				TEXTURE_SLICE_CUBEMAP,
				TEXTURE_SLICE_3D,
				TEXTURE_SLICE_2D_ARRAY,
				TEXTURE_SLICE_MAX
			};

			struct TextureView {
				DataFormat format = DATA_FORMAT_MAX;
				TextureSwizzle swizzle_r = TEXTURE_SWIZZLE_R;
				TextureSwizzle swizzle_g = TEXTURE_SWIZZLE_G;
				TextureSwizzle swizzle_b = TEXTURE_SWIZZLE_B;
				TextureSwizzle swizzle_a = TEXTURE_SWIZZLE_A;
			};

			struct TextureInfo {
				VkImage vk_image = VK_NULL_HANDLE;
				VkImageView vk_view = VK_NULL_HANDLE;
				DataFormat rd_format = DATA_FORMAT_MAX;
				VkImageCreateInfo vk_create_info = {};
				VkImageViewCreateInfo vk_view_create_info = {};
				struct {
					VmaAllocation handle = nullptr;
					VmaAllocationInfo info = {};
				} allocation; // All 0/null if just a view.
#ifdef DEBUG_ENABLED
				bool created_from_extension = false;
				bool transient = false;
#endif
			};

			struct TextureSubresource {
				TextureAspect aspect = TEXTURE_ASPECT_COLOR;
				uint32_t layer = 0;
				uint32_t mipmap = 0;
			};

			struct TextureSubresourceLayers {
				BitField<TextureAspectBits> aspect = {};
				uint32_t mipmap = 0;
				uint32_t base_layer = 0;
				uint32_t layer_count = 0;
			};

			struct TextureSubresourceRange {
				BitField<TextureAspectBits> aspect = {};
				uint32_t base_mipmap = 0;
				uint32_t mipmap_count = 0;
				uint32_t base_layer = 0;
				uint32_t layer_count = 0;
			};

			struct TextureCopyableLayout {
				uint64_t size = 0;
				uint64_t row_pitch = 0;
			};

			Device::BufferID buffer_create(uint64_t p_size, BitField<Device::BufferUsageBits> p_usage, Device::MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn);

			void buffer_free(BufferID p_buffer);

			bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format);

			uint64_t buffer_get_allocation_size(BufferID p_buffer);

			uint8_t* buffer_map(BufferID p_buffer);

			void buffer_unmap(BufferID p_buffer);

			uint8_t* buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn);

			uint64_t buffer_get_dynamic_offsets(std::span<BufferID> p_buffers);

			void buffer_flush(BufferID p_buffer);
			uint64_t buffer_get_device_address(BufferID p_buffer);

			VkSampleCountFlagBits _ensure_supported_sample_count(TextureSamples p_requested_sample_count);

			Device::TextureID texture_create(const TextureFormat& p_format, const TextureView& p_view);

			static uint32_t get_image_format_required_size(DataFormat p_format, uint32_t p_width, uint32_t p_height, uint32_t p_depth, uint32_t p_mipmaps, uint32_t* r_blockw = nullptr, uint32_t* r_blockh = nullptr, uint32_t* r_depth = nullptr);

			static uint32_t get_image_format_pixel_size(DataFormat p_format);


			static uint32_t get_compressed_image_format_pixel_rshift(DataFormat p_format);

			static void get_compressed_image_format_block_dimensions(DataFormat p_format, uint32_t& r_w, uint32_t& r_h);

			Device::TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps);

			Device::TextureID texture_create_shared(TextureID p_original_texture, const TextureView& p_view);

			Device::TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView& p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps);

			void texture_free(TextureID p_texture);

			uint64_t texture_get_allocation_size(TextureID p_texture);

			void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource& p_subresource, TextureCopyableLayout* r_layout);

			std::vector<uint8_t> texture_get_data(TextureID p_texture, uint32_t p_layer);

			uint32_t get_compressed_image_format_block_byte_size(DataFormat p_format) const;

			BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable);

			bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool& r_raw_reinterpretation);

			struct PendingFlushes {
				std::vector<VmaAllocation> allocations;
				std::vector<VkDeviceSize> offsets;
				std::vector<VkDeviceSize> sizes;
			};

			PendingFlushes pending_flushes;

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
		bool _release_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain);
		bool _recreate_image_semaphore(CommandQueue* p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain);
		VkDebugReportObjectTypeEXT _convert_to_debug_report_objectType(VkObjectType p_object_type);
	};
}
