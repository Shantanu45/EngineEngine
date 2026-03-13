#pragma once
#include <mutex>
#include <span>
#include "vulkan_context.h"
#include "vma/vk_mem_alloc.h"
#include "util/typedefs.h"
#include "util/bit_field.h"
#include "re-spirv/re-spirv.h"
#include "math/rect2i.h"


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
		DEFINE_ID(AccelerationStructure);

	private:

		enum MemoryAllocationType {
			MEMORY_ALLOCATION_TYPE_CPU, // For images, CPU allocation also means linear, GPU is tiling optimal.
			MEMORY_ALLOCATION_TYPE_GPU,
		};

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


		enum DeviceFamily {
			DEVICE_UNKNOWN,
			DEVICE_OPENGL,
			DEVICE_VULKAN,
			DEVICE_DIRECTX,
			DEVICE_METAL,
		};

		enum SamplerFilter {
			SAMPLER_FILTER_NEAREST,
			SAMPLER_FILTER_LINEAR,
		};

		enum SamplerRepeatMode {
			SAMPLER_REPEAT_MODE_REPEAT,
			SAMPLER_REPEAT_MODE_MIRRORED_REPEAT,
			SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE,
			SAMPLER_REPEAT_MODE_CLAMP_TO_BORDER,
			SAMPLER_REPEAT_MODE_MIRROR_CLAMP_TO_EDGE,
			SAMPLER_REPEAT_MODE_MAX
		};

		enum SamplerBorderColor {
			SAMPLER_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			SAMPLER_BORDER_COLOR_INT_TRANSPARENT_BLACK,
			SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
			SAMPLER_BORDER_COLOR_INT_OPAQUE_BLACK,
			SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			SAMPLER_BORDER_COLOR_INT_OPAQUE_WHITE,
			SAMPLER_BORDER_COLOR_MAX
		};

		enum AttachmentLoadOp {
			ATTACHMENT_LOAD_OP_LOAD = 0,
			ATTACHMENT_LOAD_OP_CLEAR = 1,
			ATTACHMENT_LOAD_OP_DONT_CARE = 2,
		};

		enum AttachmentStoreOp {
			ATTACHMENT_STORE_OP_STORE = 0,
			ATTACHMENT_STORE_OP_DONT_CARE = 1,
		};

		enum PolygonCullMode {
			POLYGON_CULL_DISABLED,
			POLYGON_CULL_FRONT,
			POLYGON_CULL_BACK,
			POLYGON_CULL_MAX
		};

		enum PolygonFrontFace {
			POLYGON_FRONT_FACE_CLOCKWISE,
			POLYGON_FRONT_FACE_COUNTER_CLOCKWISE,
		};

		enum StencilOperation {
			STENCIL_OP_KEEP,
			STENCIL_OP_ZERO,
			STENCIL_OP_REPLACE,
			STENCIL_OP_INCREMENT_AND_CLAMP,
			STENCIL_OP_DECREMENT_AND_CLAMP,
			STENCIL_OP_INVERT,
			STENCIL_OP_INCREMENT_AND_WRAP,
			STENCIL_OP_DECREMENT_AND_WRAP,
			STENCIL_OP_MAX
		};

		enum LogicOperation {
			LOGIC_OP_CLEAR,
			LOGIC_OP_AND,
			LOGIC_OP_AND_REVERSE,
			LOGIC_OP_COPY,
			LOGIC_OP_AND_INVERTED,
			LOGIC_OP_NO_OP,
			LOGIC_OP_XOR,
			LOGIC_OP_OR,
			LOGIC_OP_NOR,
			LOGIC_OP_EQUIVALENT,
			LOGIC_OP_INVERT,
			LOGIC_OP_OR_REVERSE,
			LOGIC_OP_COPY_INVERTED,
			LOGIC_OP_OR_INVERTED,
			LOGIC_OP_NAND,
			LOGIC_OP_SET,
			LOGIC_OP_MAX
		};

		enum BlendFactor {
			BLEND_FACTOR_ZERO,
			BLEND_FACTOR_ONE,
			BLEND_FACTOR_SRC_COLOR,
			BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
			BLEND_FACTOR_DST_COLOR,
			BLEND_FACTOR_ONE_MINUS_DST_COLOR,
			BLEND_FACTOR_SRC_ALPHA,
			BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			BLEND_FACTOR_DST_ALPHA,
			BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
			BLEND_FACTOR_CONSTANT_COLOR,
			BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
			BLEND_FACTOR_CONSTANT_ALPHA,
			BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
			BLEND_FACTOR_SRC_ALPHA_SATURATE,
			BLEND_FACTOR_SRC1_COLOR,
			BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
			BLEND_FACTOR_SRC1_ALPHA,
			BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			BLEND_FACTOR_MAX
		};

		enum BlendOperation {
			BLEND_OP_ADD,
			BLEND_OP_SUBTRACT,
			BLEND_OP_REVERSE_SUBTRACT,
			BLEND_OP_MINIMUM,
			BLEND_OP_MAXIMUM, // Yes, this one is an actual operator.
			BLEND_OP_MAX
		};

		enum PipelineDynamicStateFlags {
			DYNAMIC_STATE_LINE_WIDTH = (1 << 0),
			DYNAMIC_STATE_DEPTH_BIAS = (1 << 1),
			DYNAMIC_STATE_BLEND_CONSTANTS = (1 << 2),
			DYNAMIC_STATE_DEPTH_BOUNDS = (1 << 3),
			DYNAMIC_STATE_STENCIL_COMPARE_MASK = (1 << 4),
			DYNAMIC_STATE_STENCIL_WRITE_MASK = (1 << 5),
			DYNAMIC_STATE_STENCIL_REFERENCE = (1 << 6),
		};

		enum PipelineSpecializationConstantType {
			PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL,
			PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT,
			PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT,
		};

		enum ShaderStage {
			SHADER_STAGE_VERTEX,
			SHADER_STAGE_FRAGMENT,
			SHADER_STAGE_TESSELATION_CONTROL,
			SHADER_STAGE_TESSELATION_EVALUATION,
			SHADER_STAGE_COMPUTE,
			SHADER_STAGE_RAYGEN,
			SHADER_STAGE_ANY_HIT,
			SHADER_STAGE_CLOSEST_HIT,
			SHADER_STAGE_MISS,
			SHADER_STAGE_INTERSECTION,
			SHADER_STAGE_MAX,
			SHADER_STAGE_VERTEX_BIT = (1 << SHADER_STAGE_VERTEX),
			SHADER_STAGE_FRAGMENT_BIT = (1 << SHADER_STAGE_FRAGMENT),
			SHADER_STAGE_TESSELATION_CONTROL_BIT = (1 << SHADER_STAGE_TESSELATION_CONTROL),
			SHADER_STAGE_TESSELATION_EVALUATION_BIT = (1 << SHADER_STAGE_TESSELATION_EVALUATION),
			SHADER_STAGE_COMPUTE_BIT = (1 << SHADER_STAGE_COMPUTE),
			SHADER_STAGE_RAYGEN_BIT = (1 << SHADER_STAGE_RAYGEN),
			SHADER_STAGE_ANY_HIT_BIT = (1 << SHADER_STAGE_ANY_HIT),
			SHADER_STAGE_CLOSEST_HIT_BIT = (1 << SHADER_STAGE_CLOSEST_HIT),
			SHADER_STAGE_MISS_BIT = (1 << SHADER_STAGE_MISS),
			SHADER_STAGE_INTERSECTION_BIT = (1 << SHADER_STAGE_INTERSECTION),
		};

		enum ShaderLanguage {
			SHADER_LANGUAGE_GLSL,
			SHADER_LANGUAGE_HLSL,
		};

		enum ShaderLanguageVersion {
			SHADER_LANGUAGE_VULKAN_VERSION_1_0 = (1 << 22),
			SHADER_LANGUAGE_VULKAN_VERSION_1_1 = (1 << 22) | (1 << 12),
			SHADER_LANGUAGE_VULKAN_VERSION_1_2 = (1 << 22) | (2 << 12),
			SHADER_LANGUAGE_VULKAN_VERSION_1_3 = (1 << 22) | (3 << 12),
			SHADER_LANGUAGE_VULKAN_VERSION_1_4 = (1 << 22) | (4 << 12),
			SHADER_LANGUAGE_OPENGL_VERSION_4_5_0 = 450,
		};

		enum ShaderSpirvVersion {
			SHADER_SPIRV_VERSION_1_0 = (1 << 16),
			SHADER_SPIRV_VERSION_1_1 = (1 << 16) | (1 << 8),
			SHADER_SPIRV_VERSION_1_2 = (1 << 16) | (2 << 8),
			SHADER_SPIRV_VERSION_1_3 = (1 << 16) | (3 << 8),
			SHADER_SPIRV_VERSION_1_4 = (1 << 16) | (4 << 8),
			SHADER_SPIRV_VERSION_1_5 = (1 << 16) | (5 << 8),
			SHADER_SPIRV_VERSION_1_6 = (1 << 16) | (6 << 8),
		};

	public:
		enum CommandBufferType {
			COMMAND_BUFFER_TYPE_PRIMARY,
			COMMAND_BUFFER_TYPE_SECONDARY,
		};

		enum PipelineStageBits {
			PIPELINE_STAGE_TOP_OF_PIPE_BIT = (1 << 0),
			PIPELINE_STAGE_DRAW_INDIRECT_BIT = (1 << 1),
			PIPELINE_STAGE_VERTEX_INPUT_BIT = (1 << 2),
			PIPELINE_STAGE_VERTEX_SHADER_BIT = (1 << 3),
			PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = (1 << 4),
			PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = (1 << 5),
			PIPELINE_STAGE_GEOMETRY_SHADER_BIT = (1 << 6),
			PIPELINE_STAGE_FRAGMENT_SHADER_BIT = (1 << 7),
			PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = (1 << 8),
			PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = (1 << 9),
			PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = (1 << 10),
			PIPELINE_STAGE_COMPUTE_SHADER_BIT = (1 << 11),
			PIPELINE_STAGE_COPY_BIT = (1 << 12),
			PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = (1 << 13),
			PIPELINE_STAGE_RESOLVE_BIT = (1 << 14),
			PIPELINE_STAGE_ALL_GRAPHICS_BIT = (1 << 15),
			PIPELINE_STAGE_ALL_COMMANDS_BIT = (1 << 16),
			PIPELINE_STAGE_CLEAR_STORAGE_BIT = (1 << 17),
			PIPELINE_STAGE_RAY_TRACING_SHADER_BIT = (1 << 21),
			PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = (1 << 22),
			PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT = (1 << 23),
			PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT = (1 << 25),
		};

		enum TextureLayout {
			TEXTURE_LAYOUT_UNDEFINED,
			TEXTURE_LAYOUT_GENERAL,
			TEXTURE_LAYOUT_STORAGE_OPTIMAL,
			TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			TEXTURE_LAYOUT_COPY_SRC_OPTIMAL,
			TEXTURE_LAYOUT_COPY_DST_OPTIMAL,
			TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL,
			TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL,
			TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL,
			TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL,
			TEXTURE_LAYOUT_MAX
		};

		enum BarrierAccessBits {
			BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT = (1 << 0),
			BARRIER_ACCESS_INDEX_READ_BIT = (1 << 1),
			BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = (1 << 2),
			BARRIER_ACCESS_UNIFORM_READ_BIT = (1 << 3),
			BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT = (1 << 4),
			BARRIER_ACCESS_SHADER_READ_BIT = (1 << 5),
			BARRIER_ACCESS_SHADER_WRITE_BIT = (1 << 6),
			BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT = (1 << 7),
			BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = (1 << 8),
			BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = (1 << 9),
			BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = (1 << 10),
			BARRIER_ACCESS_COPY_READ_BIT = (1 << 11),
			BARRIER_ACCESS_COPY_WRITE_BIT = (1 << 12),
			BARRIER_ACCESS_HOST_READ_BIT = (1 << 13),
			BARRIER_ACCESS_HOST_WRITE_BIT = (1 << 14),
			BARRIER_ACCESS_MEMORY_READ_BIT = (1 << 15),
			BARRIER_ACCESS_MEMORY_WRITE_BIT = (1 << 16),
			BARRIER_ACCESS_ACCELERATION_STRUCTURE_READ_BIT = (1 << 21),
			BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT = (1 << 22),
			BARRIER_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT = (1 << 23),
			BARRIER_ACCESS_FRAGMENT_DENSITY_MAP_ATTACHMENT_READ_BIT = (1 << 24),
			BARRIER_ACCESS_RESOLVE_READ_BIT = (1 << 25),
			BARRIER_ACCESS_RESOLVE_WRITE_BIT = (1 << 26),
			BARRIER_ACCESS_STORAGE_CLEAR_BIT = (1 << 27),
		};


		enum CommandQueueFamilyBits {
			COMMAND_QUEUE_FAMILY_GRAPHICS_BIT = 0x1,
			COMMAND_QUEUE_FAMILY_COMPUTE_BIT = 0x2,
			COMMAND_QUEUE_FAMILY_TRANSFER_BIT = 0x4
		};

		enum RenderPrimitive {
			RENDER_PRIMITIVE_POINTS,
			RENDER_PRIMITIVE_LINES,
			RENDER_PRIMITIVE_LINES_WITH_ADJACENCY,
			RENDER_PRIMITIVE_LINESTRIPS,
			RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY,
			RENDER_PRIMITIVE_TRIANGLES,
			RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY,
			RENDER_PRIMITIVE_TRIANGLE_STRIPS,
			RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY, // TODO: Fix typo in "ADJACENCY" (in 5.0).
			RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX,
			RENDER_PRIMITIVE_TESSELATION_PATCH,
			RENDER_PRIMITIVE_MAX
		};

	private:

		struct ShaderCapabilities {
			bool shader_float16_is_supported = false;
			bool shader_int8_is_supported = false;
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

		//TODO: temporrily public
		public:
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
			// The swap chain's surface format can vary per window/monitor/display, so each swap chain needs its own render pass tied to its specific VkFormat. 
			// That's why render_pass is stored on the swap chain struct itself.
			RenderPassID render_pass;
			int pre_transform_rotation_degrees = 0;
			uint32_t image_index = 0;

		};
		private:
		struct RenderPassInfo {
			VkRenderPass vk_render_pass = VK_NULL_HANDLE;
			bool uses_fragment_density_map = false;
		};

		struct Framebuffer {
			VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;

			// Only filled in if the framebuffer uses a fragment density map with offsets. Unused otherwise.
			uint32_t fragment_density_map_offsets_layers = 0;

			// Only filled in by a framebuffer created by a swap chain. Unused otherwise.
			VkImage swap_chain_image = VK_NULL_HANDLE;
			VkImageSubresourceRange swap_chain_image_subresource_range = {};
			bool swap_chain_acquired = false;
		};

		VmaAllocator allocator = nullptr;
		std::unordered_map<uint32_t, VmaPool> small_allocs_pools;


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

		struct SamplerState {
			SamplerFilter mag_filter = SAMPLER_FILTER_NEAREST;
			SamplerFilter min_filter = SAMPLER_FILTER_NEAREST;
			SamplerFilter mip_filter = SAMPLER_FILTER_NEAREST;
			SamplerRepeatMode repeat_u = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
			SamplerRepeatMode repeat_v = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
			SamplerRepeatMode repeat_w = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
			float lod_bias = 0.0f;
			bool use_anisotropy = false;
			float anisotropy_max = 1.0f;
			bool enable_compare = false;
			CompareOperator compare_op = COMPARE_OP_ALWAYS;
			float min_lod = 0.0f;
			float max_lod = 1e20; // Something very large should do.
			SamplerBorderColor border_color = SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			bool unnormalized_uvw = false;
		};

		struct PendingFlushes {
			std::vector<VmaAllocation> allocations;
			std::vector<VkDeviceSize> offsets;
			std::vector<VkDeviceSize> sizes;
		};

		struct CommandBufferInfo {
			VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;
			Framebuffer* active_framebuffer = nullptr;
			RenderPassInfo* active_render_pass = nullptr;
		};

		struct CommandPool {
			VkCommandPool vk_command_pool = VK_NULL_HANDLE;
			CommandBufferType buffer_type = COMMAND_BUFFER_TYPE_PRIMARY;
			std::vector<CommandBufferInfo*> command_buffers_created;
		};

		// https://github.com/godotengine/godot/pull/110360 - "MemoryBarrier" conflicts with Windows header defines
		struct MemoryAccessBarrier {
			BitField<BarrierAccessBits> src_access = {};
			BitField<BarrierAccessBits> dst_access = {};
		};

		struct BufferBarrier {
			BufferID buffer;
			BitField<BarrierAccessBits> src_access = {};
			BitField<BarrierAccessBits> dst_access = {};
			uint64_t offset = 0;
			uint64_t size = 0;
		};

		struct TextureBarrier {
			TextureID texture;
			BitField<BarrierAccessBits> src_access = {};
			BitField<BarrierAccessBits> dst_access = {};
			TextureLayout prev_layout = TEXTURE_LAYOUT_UNDEFINED;
			TextureLayout next_layout = TEXTURE_LAYOUT_UNDEFINED;
			TextureSubresourceRange subresources;
		};

		struct AccelerationStructureBarrier {
			AccelerationStructureID acceleration_structure;
			BitField<BarrierAccessBits> src_access;
			BitField<BarrierAccessBits> dst_access;
			uint64_t offset = 0;
			uint64_t size = 0;
		};

		struct ShaderInfo {
			std::string name;
			VkShaderStageFlags vk_push_constant_stages = 0;
			std::vector<VkPipelineShaderStageCreateInfo> vk_stages_create_info;
			std::vector<VkRayTracingShaderGroupCreateInfoKHR> vk_groups_create_info;
			std::vector<VkDescriptorSetLayout> vk_descriptor_set_layouts;
			std::vector<respv::Shader> respv_stage_shaders;
			std::vector<std::vector<uint8_t>> spirv_stage_bytes;
			std::vector<uint64_t> original_stage_size;
			VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
			// Used to update the shader binding table buffer.
			//RaytracingShaderRegionCount region_count;
		};

		struct Attachment {
			DataFormat format = DATA_FORMAT_MAX;
			TextureSamples samples = TEXTURE_SAMPLES_MAX;
			AttachmentLoadOp load_op = ATTACHMENT_LOAD_OP_DONT_CARE;
			AttachmentStoreOp store_op = ATTACHMENT_STORE_OP_DONT_CARE;
			AttachmentLoadOp stencil_load_op = ATTACHMENT_LOAD_OP_DONT_CARE;
			AttachmentStoreOp stencil_store_op = ATTACHMENT_STORE_OP_DONT_CARE;
			TextureLayout initial_layout = TEXTURE_LAYOUT_UNDEFINED;
			TextureLayout final_layout = TEXTURE_LAYOUT_UNDEFINED;
		};

		struct ShaderStageSPIRVData {
			ShaderStage shader_stage = SHADER_STAGE_MAX;
			std::vector<uint8_t> spirv;
			std::vector<uint64_t> dynamic_buffers;
		};

		struct VertexFormatInfo {
			std::vector<VkVertexInputBindingDescription> vk_bindings;
			std::vector<VkVertexInputAttributeDescription> vk_attributes;
			VkPipelineVertexInputStateCreateInfo vk_create_info = {};
		};

	public:
		union RenderPassClearValue {
			Color color = {};
			struct {
				float depth;
				uint32_t stencil;
			};

			RenderPassClearValue() {}
		};

		struct AttachmentReference {
			static constexpr uint32_t UNUSED = 0xffffffff;
			uint32_t attachment = UNUSED;
			TextureLayout layout = TEXTURE_LAYOUT_UNDEFINED;
			BitField<TextureAspectBits> aspect = {};
		};

		struct AttachmentClear {
			BitField<TextureAspectBits> aspect = {};
			uint32_t color_attachment = 0xffffffff;
			RenderPassClearValue value;
		};
	private:

		struct Subpass {
			std::vector<AttachmentReference> input_references;
			std::vector<AttachmentReference> color_references;
			AttachmentReference depth_stencil_reference;
			AttachmentReference depth_resolve_reference;
			std::vector<AttachmentReference> resolve_references;
			std::vector<uint32_t> preserve_attachments;
			AttachmentReference fragment_shading_rate_reference;
			Size2i fragment_shading_rate_texel_size;
		};

		struct SubpassDependency {
			uint32_t src_subpass = 0xffffffff;
			uint32_t dst_subpass = 0xffffffff;
			BitField<PipelineStageBits> src_stages = {};
			BitField<PipelineStageBits> dst_stages = {};
			BitField<BarrierAccessBits> src_access = {};
			BitField<BarrierAccessBits> dst_access = {};
		};

		struct PipelineRasterizationState {
			bool enable_depth_clamp = false;
			bool discard_primitives = false;
			bool wireframe = false;
			PolygonCullMode cull_mode = POLYGON_CULL_DISABLED;
			PolygonFrontFace front_face = POLYGON_FRONT_FACE_CLOCKWISE;
			bool depth_bias_enabled = false;
			float depth_bias_constant_factor = 0.0f;
			float depth_bias_clamp = 0.0f;
			float depth_bias_slope_factor = 0.0f;
			float line_width = 1.0f;
			uint32_t patch_control_points = 1;
		};

		struct PipelineMultisampleState {
			TextureSamples sample_count = TEXTURE_SAMPLES_1;
			bool enable_sample_shading = false;
			float min_sample_shading = 0.0f;
			std::vector<uint32_t> sample_mask;
			bool enable_alpha_to_coverage = false;
			bool enable_alpha_to_one = false;
		};

		struct PipelineDepthStencilState {
			bool enable_depth_test = false;
			bool enable_depth_write = false;
			CompareOperator depth_compare_operator = COMPARE_OP_ALWAYS;
			bool enable_depth_range = false;
			float depth_range_min = 0;
			float depth_range_max = 0;
			bool enable_stencil = false;

			struct StencilOperationState {
				StencilOperation fail = STENCIL_OP_ZERO;
				StencilOperation pass = STENCIL_OP_ZERO;
				StencilOperation depth_fail = STENCIL_OP_ZERO;
				CompareOperator compare = COMPARE_OP_ALWAYS;
				uint32_t compare_mask = 0;
				uint32_t write_mask = 0;
				uint32_t reference = 0;
			};

			StencilOperationState front_op;
			StencilOperationState back_op;
		};

		struct PipelineColorBlendState {
			bool enable_logic_op = false;
			LogicOperation logic_op = LOGIC_OP_CLEAR;

			struct Attachment {
				bool enable_blend = false;
				BlendFactor src_color_blend_factor = BLEND_FACTOR_ZERO;
				BlendFactor dst_color_blend_factor = BLEND_FACTOR_ZERO;
				BlendOperation color_blend_op = BLEND_OP_ADD;
				BlendFactor src_alpha_blend_factor = BLEND_FACTOR_ZERO;
				BlendFactor dst_alpha_blend_factor = BLEND_FACTOR_ZERO;
				BlendOperation alpha_blend_op = BLEND_OP_ADD;
				bool write_r = true;
				bool write_g = true;
				bool write_b = true;
				bool write_a = true;
			};

			static PipelineColorBlendState create_disabled(int p_attachments = 1) {
				PipelineColorBlendState bs;
				for (int i = 0; i < p_attachments; i++) {
					bs.attachments.push_back(Attachment());
				}
				return bs;
			}

			static PipelineColorBlendState create_blend(int p_attachments = 1) {
				PipelineColorBlendState bs;
				for (int i = 0; i < p_attachments; i++) {
					Attachment ba;
					ba.enable_blend = true;
					ba.src_color_blend_factor = BLEND_FACTOR_SRC_ALPHA;
					ba.dst_color_blend_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					ba.src_alpha_blend_factor = BLEND_FACTOR_SRC_ALPHA;
					ba.dst_alpha_blend_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

					bs.attachments.push_back(ba);
				}
				return bs;
			}

			std::vector<Attachment> attachments; // One per render target texture.
			Color blend_constant;
		};

		struct PipelineSpecializationConstant {
			PipelineSpecializationConstantType type = {};
			uint32_t constant_id = 0xffffffff;
			union {
				uint32_t int_value = 0;
				float float_value;
				bool bool_value;
			};
		};

		struct ShaderSpecializationConstant : public PipelineSpecializationConstant {
			BitField<ShaderStage> stages = {};

			bool operator<(const ShaderSpecializationConstant& p_other) const { return constant_id < p_other.constant_id; }
		};



	public:

		static const bool command_pool_reset_enabled = false;		//TODO; chose correct option

		static int caching_instance_count;

		static const int32_t ATTACHMENT_UNUSED = -1;

		Device(Context* p_context_driver);
		virtual ~Device() = default;

		Error initialize(uint32_t p_device_index, uint32_t p_frame_count);

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

		Device::SamplerID sampler_create(const SamplerState& p_state);

		void sampler_free(SamplerID p_sampler);

		bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter);

		void command_pipeline_barrier(CommandBufferID p_cmd_buffer, BitField<PipelineStageBits> p_src_stages, BitField<PipelineStageBits> p_dst_stages, std::span<MemoryAccessBarrier> p_memory_barriers, std::span<BufferBarrier> p_buffer_barriers, std::span<TextureBarrier> p_texture_barriers, std::span<AccelerationStructureBarrier> p_acceleration_structure_barriers);

		Device::FenceID fence_create();

		Error fence_wait(FenceID p_fence);

		void fence_free(FenceID p_fence);

		Device::SemaphoreID semaphore_create();

		void semaphore_free(SemaphoreID p_semaphore);
		
		Device::SwapChainID swap_chain_create(Context::SurfaceID p_surface);

		Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count);

		Device::FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool& r_resize_required);

		Device::RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain);

		int swap_chain_get_pre_rotation_degrees(SwapChainID p_swap_chain);

		DataFormat swap_chain_get_format(SwapChainID p_swap_chain);

		ColorSpace swap_chain_get_color_space(SwapChainID p_swap_chain);

		void swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps);

		void swap_chain_free(SwapChainID p_swap_chain);

		Device::CommandQueueFamilyID command_queue_family_get(BitField<Device::CommandQueueFamilyBits> p_cmd_queue_family_bits, Context::SurfaceID p_surface);

		Device::CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue);

		Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, std::span<SemaphoreID> p_wait_semaphores, std::span<CommandBufferID> p_cmd_buffers, std::span<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, std::span<SwapChainID> p_swap_chains);

		void command_queue_free(CommandQueueID p_cmd_queue);

		Device::CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type);

		bool command_pool_reset(CommandPoolID p_cmd_pool);

		void command_pool_free(CommandPoolID p_cmd_pool);

		Device::CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool);

		bool command_buffer_begin(CommandBufferID p_cmd_buffer);

		bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer);

		void command_buffer_end(CommandBufferID p_cmd_buffer);

		void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, std::span<CommandBufferID> p_secondary_cmd_buffers);

		Device::FramebufferID framebuffer_create(RenderPassID p_render_pass, std::span<TextureID> p_attachments, uint32_t p_width, uint32_t p_height);

		void framebuffer_free(FramebufferID p_framebuffer);

		void pipeline_free(PipelineID p_pipeline);

		void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, std::span<uint32_t> p_data);

		bool pipeline_cache_create(const std::vector<uint8_t>& p_data);

		void pipeline_cache_free();

		size_t pipeline_cache_query_size();

		std::vector<uint8_t> pipeline_cache_serialize();

		Device::RenderPassID render_pass_create(std::span<Attachment> p_attachments, std::span<Subpass> p_subpasses, std::span<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment);

		void render_pass_free(RenderPassID p_render_pass);

		void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i& p_rect, std::vector<RenderPassClearValue> p_clear_values);

		void command_end_render_pass(CommandBufferID p_cmd_buffer);

		Device::PipelineID render_pipeline_create(ShaderID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, PipelineRasterizationState p_rasterization_state, PipelineMultisampleState p_multisample_state, PipelineDepthStencilState p_depth_stencil_state, PipelineColorBlendState p_blend_state, std::span<int32_t> p_color_attachments, BitField<PipelineDynamicStateFlags> p_dynamic_state, RenderPassID p_render_pass, uint32_t p_render_subpass, std::span<PipelineSpecializationConstant> p_specialization_constants);

		void print_lost_device_info();

		std::string get_vulkan_result(VkResult err);


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
		bool _determine_swap_chain_format(Context::SurfaceID p_surface, VkFormat& r_format, VkColorSpaceKHR& r_color_space);
		void _swap_chain_release(SwapChain* p_swap_chain);
		VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);

	private:
		VkDevice vk_device = VK_NULL_HANDLE;
		Context* context_driver = nullptr;
		Context::Device context_device = {};
		uint32_t max_descriptor_sets_per_pool = 0;
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
		std::unordered_map<uint64_t, bool> has_comp_alpha;
		Capabilities device_capabilities;
		ShaderCapabilities shader_capabilities;
		bool buffer_device_address_support = false;
		bool vulkan_memory_model_support = false;
		bool vulkan_memory_model_device_scope_support = false;
		bool pipeline_cache_control_support = false;
		bool device_fault_support = false;
		PendingFlushes pending_flushes;

	};
}
