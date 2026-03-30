/*****************************************************************//**
 * \file   vulkan_device.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include <mutex>
#include <span>
#include <map>
#include "vulkan_context.h"
#include "vma/vk_mem_alloc.h"
#include "util/typedefs.h"
#include "util/bit_field.h"
#include "re-spirv/re-spirv.h"
#include "math/rect2i.h"
#include "shader_container.h"
#include "rendering/rendering_device_driver.h"

namespace Vulkan
{

	using namespace ::Rendering;
	class RenderingDeviceDriverVulkan : public Rendering::RenderingDeviceDriver
	{
	
		// Keep the enum values in sync with the `SHADER_UNIFORM_NAMES` values (file rendering_device.cpp).
		
		static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;
		struct CommandQueue;
		struct SwapChain;
		struct CommandBufferInfo;
		struct RenderPassInfo;
		struct Framebuffer;

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

		//struct SubgroupCapabilities {
		//	uint32_t size = 0;
		//	uint32_t min_size = 0;
		//	uint32_t max_size = 0;
		//	VkShaderStageFlags supported_stages = 0;
		//	VkSubgroupFeatureFlags supported_operations = 0;
		//	VkBool32 quad_operations_in_all_stages = false;
		//	bool size_control_is_supported = false;

		//	uint32_t supported_stages_flags_rd() const;
		//	String supported_stages_desc() const;
		//	uint32_t supported_operations_flags_rd() const;
		//	String supported_operations_desc() const;
		//};

		struct StorageBufferCapabilities {
			bool storage_buffer_16_bit_access_is_supported = false;
			bool uniform_and_storage_buffer_16_bit_access_is_supported = false;
			bool storage_push_constant_16_is_supported = false;
			bool storage_input_output_16 = false;
		};

		struct AccelerationStructureCapabilities {
			bool acceleration_structure_support = false;
			uint32_t min_acceleration_structure_scratch_offset_alignment = 0;
		};

		struct RaytracingCapabilities {
			bool raytracing_pipeline_support = false;
			uint32_t shader_group_handle_size = 0;
			uint32_t shader_group_handle_alignment = 0;
			uint32_t shader_group_handle_size_aligned = 0;
			uint32_t shader_group_base_alignment = 0;
			bool validation = false;
		};

		
	private:

		using VertexAttributeBindingsMap = std::unordered_map<uint32_t, VertexAttributeBinding>;

		struct ShaderCapabilities {
			bool shader_float16_is_supported = false;
			bool shader_int8_is_supported = false;
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
		private:
		struct SwapChain {
			VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
			RenderingContextDriverVulkan::SurfaceID surface = RenderingContextDriverVulkan::SurfaceID();
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

		struct VertexFormatInfo {
			std::vector<VkVertexInputBindingDescription> vk_bindings;
			std::vector<VkVertexInputAttributeDescription> vk_attributes;
			VkPipelineVertexInputStateCreateInfo vk_create_info = {};
		};

		struct DescriptorSetPoolKey {
			uint16_t uniform_type[UNIFORM_TYPE_MAX] = {};

			bool operator<(const DescriptorSetPoolKey& p_other) const {
				return memcmp(uniform_type, p_other.uniform_type, sizeof(uniform_type)) < 0;
			}
		};

		using DescriptorSetPools = std::map<DescriptorSetPoolKey, std::unordered_map<VkDescriptorPool, uint32_t>>;

		struct UniformSetInfo {
			VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
			VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
			VkDescriptorPool vk_linear_descriptor_pool = VK_NULL_HANDLE;
			DescriptorSetPools::iterator pool_sets_it;
			std::vector<BufferInfo const*/*, uint32_t*/> dynamic_buffers;
		};

	public:

		static const bool command_pool_reset_enabled = false;		//TODO; chose correct option

		static int caching_instance_count;

		static const int32_t ATTACHMENT_UNUSED = -1;

		RenderingDeviceDriverVulkan(RenderingContextDriverVulkan* p_context_driver);
		virtual ~RenderingDeviceDriverVulkan();

		Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override;

		void finalize() ;

		BufferID buffer_create(uint64_t p_size, BitField<RenderingDeviceDriverVulkan::BufferUsageBits> p_usage, RenderingDeviceDriverVulkan::MemoryAllocationType p_allocation_type,
			uint64_t p_frames_drawn) override;

		void buffer_free(BufferID p_buffer) override;

		bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override;

		uint64_t buffer_get_allocation_size(BufferID p_buffer) override;

		uint8_t* buffer_map(BufferID p_buffer) override;

		void buffer_unmap(BufferID p_buffer) override;

		uint8_t* buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) override;

		uint64_t buffer_get_dynamic_offsets(std::span<BufferID> p_buffers) override;

		void buffer_flush(BufferID p_buffer) override ;

		uint64_t buffer_get_device_address(BufferID p_buffer) override;


		TextureID texture_create(const TextureFormat& p_format, const TextureView& p_view) override;

		RenderingDeviceDriverVulkan::TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, 
			bool p_depth_stencil, uint32_t p_mipmaps) override;

		RenderingDeviceDriverVulkan::TextureID texture_create_shared(TextureID p_original_texture, const TextureView& p_view) override;

		RenderingDeviceDriverVulkan::TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView& p_view, TextureSliceType p_slice_type, 
			uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) override;

		void texture_free(TextureID p_texture) override;

		uint64_t texture_get_allocation_size(TextureID p_texture) override;

		void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource& p_subresource, TextureCopyableLayout* r_layout) override;

		std::vector<uint8_t> texture_get_data(TextureID p_texture, uint32_t p_layer) override;

		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override;

		bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool& r_raw_reinterpretation) override;

		RenderingDeviceDriverVulkan::SamplerID sampler_create(const SamplerState& p_state) override;

		void sampler_free(SamplerID p_sampler) override;

		bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override;

		RenderingDeviceDriverVulkan::VertexFormatID vertex_format_create(std::span<VertexAttribute> p_vertex_attribs, const VertexAttributeBindingsMap& p_vertex_bindings) override;

		void vertex_format_free(VertexFormatID p_vertex_format) override;

		void command_pipeline_barrier(CommandBufferID p_cmd_buffer, BitField<PipelineStageBits> p_src_stages, BitField<PipelineStageBits> p_dst_stages, 
			std::span<MemoryAccessBarrier> p_memory_barriers, std::span<BufferBarrier> p_buffer_barriers, std::span<TextureBarrier> p_texture_barriers, 
			std::span<AccelerationStructureBarrier> p_acceleration_structure_barriers) override;

		RenderingDeviceDriverVulkan::FenceID fence_create() override;

		Error fence_wait(FenceID p_fence) override;

		void fence_free(FenceID p_fence) override;

		RenderingDeviceDriverVulkan::SemaphoreID semaphore_create() override;

		void semaphore_free(SemaphoreID p_semaphore) override;
		
		RenderingDeviceDriverVulkan::SwapChainID swap_chain_create(RenderingContextDriverVulkan::SurfaceID p_surface) override;

		Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override;

		RenderingDeviceDriverVulkan::FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool& r_resize_required) override;

		RenderingDeviceDriverVulkan::RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override;

		int swap_chain_get_pre_rotation_degrees(SwapChainID p_swap_chain) override;

		DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override;

		ColorSpace swap_chain_get_color_space(SwapChainID p_swap_chain) override;

		void swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) override;

		void swap_chain_free(SwapChainID p_swap_chain) override;

		RenderingDeviceDriverVulkan::CommandQueueFamilyID command_queue_family_get(BitField<RenderingDeviceDriverVulkan::CommandQueueFamilyBits> p_cmd_queue_family_bits, 
			RenderingContextDriverVulkan::SurfaceID p_surface) override;

		RenderingDeviceDriverVulkan::CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue) override;

		Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, std::span<SemaphoreID> p_wait_semaphores, 
			std::span<CommandBufferID> p_cmd_buffers, std::span<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, 
			std::span<SwapChainID> p_swap_chains) override;

		void command_queue_free(CommandQueueID p_cmd_queue) override;

		RenderingDeviceDriverVulkan::CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override;

		bool command_pool_reset(CommandPoolID p_cmd_pool) override;

		void command_pool_free(CommandPoolID p_cmd_pool) override;

		RenderingDeviceDriverVulkan::CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override;

		bool command_buffer_begin(CommandBufferID p_cmd_buffer) override;

		bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override;

		void command_buffer_end(CommandBufferID p_cmd_buffer) override;

		void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, std::span<CommandBufferID> p_secondary_cmd_buffers) override;

		RenderingDeviceDriverVulkan::FramebufferID framebuffer_create(RenderPassID p_render_pass, std::span<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override;

		void framebuffer_free(FramebufferID p_framebuffer) override;

		void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override;

		void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, std::span<BufferCopyRegion> p_regions) override;

		void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, 
			TextureID p_dst_texture, TextureLayout p_dst_texture_layout, std::span<TextureCopyRegion> p_regions) override;

		void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, 
			uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, 
			uint32_t p_dst_layer, uint32_t p_dst_mipmap) override;

		void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, 
			const Color& p_color, const TextureSubresourceRange& p_subresources) override;

		void command_clear_depth_stencil_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, 
			float p_depth, uint8_t p_stencil, const TextureSubresourceRange& p_subresources) override;

		void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, 
			TextureLayout p_dst_texture_layout, std::span<BufferTextureCopyRegion> p_regions) override;

		void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout,
			BufferID p_dst_buffer, std::span<BufferTextureCopyRegion> p_regions) override;

		void pipeline_free(PipelineID p_pipeline) override;

		void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, std::span<uint32_t> p_data) override;

		bool pipeline_cache_create(const std::vector<uint8_t>& p_data) override;

		void pipeline_cache_free() override;

		size_t pipeline_cache_query_size() override;

		std::vector<uint8_t> pipeline_cache_serialize() override;

		RenderingDeviceDriverVulkan::RenderPassID render_pass_create(std::span<Attachment> p_attachments, std::span<Subpass> p_subpasses,
			std::span<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment) override;

		void render_pass_free(RenderPassID p_render_pass) override
			;
		virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer,
			CommandBufferType p_cmd_buffer_type, const Rect2i& p_rect, std::span<RenderPassClearValue> p_clear_values) override;

		void command_end_render_pass(CommandBufferID p_cmd_buffer) override;

		void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override;

		void command_render_set_viewport(CommandBufferID p_cmd_buffer, std::span<Rect2i> p_viewports) override;

		void command_render_set_scissor(CommandBufferID p_cmd_buffer, std::span<Rect2i> p_scissors) override;

		void command_render_clear_attachments(CommandBufferID p_cmd_buffer, std::span<AttachmentClear> p_attachment_clears, std::span<Rect2i> p_rects) override;

		void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override;

		void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, std::span<UniformSetID> p_uniform_sets, ShaderID p_shader, 
			uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) override;

		void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, 
			uint32_t p_first_instance) override;

		void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, 
			uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override;

		void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset,
			uint32_t p_draw_count, uint32_t p_stride) override;


		void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, 
			BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override;

		void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, 
			uint32_t p_draw_count, uint32_t p_stride) override;

		void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset,
			BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override;

		void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID* p_buffers, 
			const uint64_t* p_offsets, uint64_t p_dynamic_offsets) override;

		void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override;

		void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color& p_constants) override;

		void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override;

		RenderingDeviceDriverVulkan::PipelineID render_pipeline_create(ShaderID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive,
			PipelineRasterizationState p_rasterization_state, PipelineMultisampleState p_multisample_state, PipelineDepthStencilState p_depth_stencil_state, 
			PipelineColorBlendState p_blend_state, std::span<int32_t> p_color_attachments, BitField<PipelineDynamicStateFlags> p_dynamic_state,
			RenderPassID p_render_pass, uint32_t p_render_subpass, std::span<PipelineSpecializationConstant> p_specialization_constants = std::span<PipelineSpecializationConstant>()) override;

		void print_lost_device_info();

		std::string get_vulkan_result(VkResult err);

		RenderingDeviceDriverVulkan::QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override;

		void timestamp_query_pool_free(QueryPoolID p_pool_id) override;

		void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t* r_results) override;

		uint64_t timestamp_query_result_to_time(uint64_t p_result) override;

		void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override;

		void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override;

		void command_begin_label(CommandBufferID p_cmd_buffer, const char* p_label_name, const Color& p_color) override;

		void command_end_label(CommandBufferID p_cmd_buffer) override;

		const RenderingShaderContainerFormat& get_shader_container_format() const override;

		RenderingDeviceDriverVulkan::UniformSetID uniform_set_create(std::span<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) override;

		void uniform_set_free(UniformSetID p_uniform_set) override;

		bool uniform_sets_have_linear_pools() const override;

		uint32_t uniform_sets_get_dynamic_offsets(std::span<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) const override;

		void linear_uniform_set_pools_reset(int p_linear_pool_index) override;

		void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override;

		ShaderID shader_create_from_container(const RenderingShaderContainer* p_shader_container, const std::vector<ImmutableSampler>& p_immutable_samplers) override;

		void shader_free(ShaderID p_shader) override;

		void shader_destroy_modules(ShaderID p_shader) override;

		virtual bool has_feature(Features p_feature) override final;

		virtual uint64_t limit_get(Limit p_limit) override final;

		VkDevice vulkan_device_get() const {
			return vk_device;
		}
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
		bool _determine_swap_chain_format(RenderingContextDriverVulkan::SurfaceID p_surface, VkFormat& r_format, VkColorSpaceKHR& r_color_space);
		void _swap_chain_release(SwapChain* p_swap_chain);
		VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);
		//Device::ShaderID shader_create_from_container(const RenderingShaderContainer* p_shader_container, const std::vector<ImmutableSampler>& p_immutable_samplers);
		VkDescriptorPool _descriptor_set_pool_create(const DescriptorSetPoolKey& p_key, bool p_linear_pool);
		void _descriptor_set_pool_unreference(DescriptorSetPools::iterator p_pool_sets_it, VkDescriptorPool p_vk_descriptor_pool, int p_linear_pool_index);
		VkSampleCountFlagBits _ensure_supported_sample_count(TextureSamples p_requested_sample_count) ;


	private:
		VkDevice vk_device = VK_NULL_HANDLE;
		RenderingContextDriverVulkan* context_driver = nullptr;
		RenderingContextDriverVulkan::Device context_device = {};
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

		bool linear_descriptor_pools_enabled = true;

		DescriptorSetPools descriptor_set_pools;
		uint32_t max_descriptor_sets_per_pool = 0;
		std::unordered_map<int, DescriptorSetPools> linear_descriptor_set_pools;

		// Global flag to toggle usage of immutable sampler when creating pipeline layouts.
		// It cannot change after creating the PSOs, since we need to skipping samplers when creating uniform sets.
		bool immutable_samplers_enabled = true;
		RenderingShaderContainerFormatVulkan shader_container_format;

	};
}
