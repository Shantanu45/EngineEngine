#pragma once
#include "rendering_device_commons.h"
#include "rendering_device_driver.h"
#include "rendering_context_driver.h"
#include "util/rid_owner.h"
#include "xxhash.h"

namespace Rendering
{
	class RenderingDevice : public RenderingDeviceCommons
	{
	private:
	public:
		//base numeric ID for all types
		enum {
			INVALID_FORMAT_ID = -1
		};

		enum IDType {
			ID_TYPE_FRAMEBUFFER_FORMAT,
			ID_TYPE_VERTEX_FORMAT,
			ID_TYPE_DRAW_LIST,
			ID_TYPE_COMPUTE_LIST = 4,
			ID_TYPE_RAYTRACING_LIST = 5,
			ID_TYPE_MAX,
			ID_BASE_SHIFT = 58, // 5 bits for ID types.
			ID_MASK = (ID_BASE_SHIFT - 1),
		};
		
	public:
		typedef int64_t DrawListID;

		static RenderingDevice* get_singleton() {
			static RenderingDevice* singleton = new RenderingDevice();
			return singleton;
		};

		Error initialize(RenderingContextDriver* p_context, DisplayServerEnums::WindowID p_main_window = DisplayServerEnums::INVALID_WINDOW_ID);

		void finalize();


#pragma region Shader
		std::vector<uint8_t> shader_compile_spirv_from_source(ShaderStage p_stage, const std::string& p_source_code, 
			ShaderLanguage p_language = SHADER_LANGUAGE_GLSL, std::string* r_error = nullptr, bool p_allow_cache = true);
		std::vector<uint8_t> shader_compile_binary_from_spirv(const std::vector<ShaderStageSPIRVData>& p_spirv, const std::string& p_shader_name = "");
		RID shader_create_from_spirv(const std::vector<ShaderStageSPIRVData>& p_spirv, const std::string& p_shader_name = "");
		RID shader_create_from_bytecode(const std::vector<uint8_t>& p_shader_binary, RID p_placeholder = RID());
		RID shader_create_placeholder();
		void shader_destroy_modules(RID p_shader);

		uint64_t shader_get_vertex_input_attribute_mask(RID p_shader);

		struct Shader : public ShaderReflection {
			std::string name; // Used for debug.
			RDD::ShaderID driver_id;
			uint32_t layout_hash = 0;
			BitField<RDD::PipelineStageBits> stage_bits = {};
			std::vector<uint32_t> set_formats;
		};

		RID_Owner<Shader, true> shader_owner;



#pragma endregion

#pragma region Pipeline
		typedef int64_t FramebufferFormatID;
		typedef int64_t VertexFormatID;
		RID render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, 
			RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state, 
			const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, 
			const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags = 0, 
			uint32_t p_for_render_pass = 0, 
			const std::vector<PipelineSpecializationConstant>& p_specialization_constants = std::vector<PipelineSpecializationConstant>());

		bool render_pipeline_is_valid(RID p_pipeline);

#pragma endregion

#pragma region Screen
		Error screen_create(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);
		Error screen_prepare_for_drawing(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);
		int screen_get_width(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;
		int screen_get_height(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;
		int screen_get_pre_rotation_degrees(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;
		FramebufferFormatID screen_get_framebuffer_format(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;
		ColorSpace screen_get_color_space(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID) const;
		Error screen_free(DisplayServerEnums::WindowID p_screen = DisplayServerEnums::MAIN_WINDOW_ID);
#pragma endregion

	private:
		uint32_t _get_swap_chain_desired_count() const;

		struct Frame {
			// The command pool used by the command buffer.
			RenderingDeviceDriver::CommandPoolID command_pool;

			// The command buffer used by the main thread when recording the frame.
			RenderingDeviceDriver::CommandBufferID command_buffer;

			// Signaled by the command buffer submission. Present must wait on this semaphore.
			RenderingDeviceDriver::SemaphoreID semaphore;

			// Signaled by the command buffer submission. Must wait on this fence before beginning command recording for the frame.
			RenderingDeviceDriver::FenceID fence;
			bool fence_signaled = false;

			// Semaphores the frame must wait on before executing the command buffer.
			std::vector<RenderingDeviceDriver::SemaphoreID> semaphores_to_wait_on;
			//  Swap chains prepared for drawing during the frame that must be presented.
			std::vector<RenderingDeviceDriver::SwapChainID> swap_chains_to_present;

			// Semaphores the transfer workers can use to wait before rendering the frame.
			// This must have the same size of the transfer worker pool.
			std::vector<RenderingDeviceDriver::SemaphoreID> transfer_worker_semaphores;

			// Extra command buffer pool used for driver workarounds or to reduce GPU bubbles by
			// splitting the final render pass to the swapchain into its own cmd buffer.
			//Device::CommandBufferPool command_buffer_pool;

			uint64_t index = 0;
		};
	protected:
		void execute_chained_cmds(bool p_present_swap_chain,
			RenderingDeviceDriver::FenceID p_draw_fence,
			RenderingDeviceDriver::SemaphoreID p_dst_draw_semaphore_to_signal);

	public:
		//void _free_internal(RID p_id);
		void _begin_frame(bool p_presented = false);
		void _end_frame();
		void _execute_frame(bool p_present);
		void _stall_for_frame(uint32_t p_frame);
		void _stall_for_previous_frames();
		void _flush_and_stall_for_all_frames(bool p_begin_frame = true);
		void swap_buffers(bool p_present);
		void submit();
		void sync();

		enum MemoryType {
			MEMORY_TEXTURES,
			MEMORY_BUFFERS,
			MEMORY_TOTAL
		};

		RenderingDevice* create_local_device();

	private:

		//RID _texture_create(const RDTextureFormat* p_format, const RDTextureView* p_view, const  std::vector<PackedByteArray>& p_data = {});
		//RID _texture_create_shared(const RDTextureView* p_view, RID p_with_texture);
		//RID _texture_create_shared_from_slice(const RDTextureView* p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps = 1, TextureSliceType p_slice_type = TEXTURE_SLICE_2D);
		//RDTextureFormat* _texture_get_format(RID p_rd_texture);

		//FramebufferFormatID _framebuffer_format_create(const std::vector<RDAttachmentFormat>& p_attachments, uint32_t p_view_count);
		//FramebufferFormatID _framebuffer_format_create_multipass(const std::vector<RDAttachmentFormat>& p_attachments, const std::vector<RDFramebufferPass>& p_passes, uint32_t p_view_count);
		//RID _framebuffer_create(const std::vector<RID>& p_textures, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);
		//RID _framebuffer_create_multipass(const std::vector<RID>& p_textures, const std::vector<RDFramebufferPass>& p_passes, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);
		struct VertexDescriptionKey {
			std::vector<VertexAttribute> vertex_formats;

			bool operator==(const VertexDescriptionKey& p_key) const {
				int vdc = vertex_formats.size();
				int vdck = p_key.vertex_formats.size();

				if (vdc != vdck) {
					return false;
				}
				else {
					const VertexAttribute* a_ptr = vertex_formats.data();
					const VertexAttribute* b_ptr = p_key.vertex_formats.data();
					for (int i = 0; i < vdc; i++) {
						const VertexAttribute& a = a_ptr[i];
						const VertexAttribute& b = b_ptr[i];

						if (a.location != b.location) {
							return false;
						}
						if (a.offset != b.offset) {
							return false;
						}
						if (a.format != b.format) {
							return false;
						}
						if (a.stride != b.stride) {
							return false;
						}
						if (a.frequency != b.frequency) {
							return false;
						}
					}
					return true; // They are equal.
				}
			}

			uint32_t hash() const {
				int vdc = vertex_formats.size();

				XXH32_state_t* state = XXH32_createState();
				XXH32_reset(state, 0);

				XXH32_update(state, &vdc, sizeof(vdc));

				const VertexAttribute* ptr = vertex_formats.data();
				for (int i = 0; i < vdc; i++) {
					const VertexAttribute& vd = ptr[i];

					XXH32_update(state, &vd.location, sizeof(vd.location));
					XXH32_update(state, &vd.offset, sizeof(vd.offset));
					XXH32_update(state, &vd.format, sizeof(vd.format));
					XXH32_update(state, &vd.stride, sizeof(vd.stride));
					XXH32_update(state, &vd.frequency, sizeof(vd.frequency));
				}

				uint32_t h = XXH32_digest(state);
				XXH32_freeState(state);
				return h;
			}
		};

		struct VertexDescriptionHash {
			std::size_t operator()(const VertexDescriptionKey& p_key) const {
				return p_key.hash();
			}
		};

		// This is a cache and it's never freed, it ensures that
		// ID used for a specific format always remain the same.
		std::unordered_map<VertexDescriptionKey, VertexFormatID, VertexDescriptionHash> vertex_format_cache;

		struct VertexDescriptionCache {
			std::vector<VertexAttribute> vertex_formats;
			VertexAttributeBindingsMap bindings;
			RDD::VertexFormatID driver_id;
		};

		std::unordered_map<VertexFormatID, VertexDescriptionCache> vertex_formats;


		//RID _sampler_create(const RDSamplerState* p_state);
		VertexFormatID vertex_format_create(const std::vector<VertexAttribute>& p_vertex_descriptions);
		//RenderingDeviceDriver::VertexFormatID _vertex_format_create(const std::vector<RDVertexAttribute>& p_vertex_formats);
		//RID _vertex_array_create(uint32_t p_vertex_count, RenderingDeviceDriver::VertexFormatID p_vertex_format, const std::vector<RID>& p_src_buffers, const std::vector<int64_t>& p_offsets = std::vector<int64_t>());
		//void _draw_list_bind_vertex_buffers_format(DrawListID p_list, RenderingDeviceDriver::VertexFormatID p_vertex_format, uint32_t p_vertex_count, const  std::vector<RID>& p_vertex_buffers, const  std::vector<int64_t>& p_offsets = std::vector<int64_t>());

		RenderingDevice();
		~RenderingDevice();

		bool is_main_instance = false;

		RenderingContextDriver* context = nullptr;
		RenderingDeviceDriver* driver = nullptr;
		RenderingContextDriver::Device device;

		RDD::CommandQueueFamilyID main_queue_family;
		RDD::CommandQueueFamilyID transfer_queue_family;
		RDD::CommandQueueFamilyID present_queue_family;
		RDD::CommandQueueID main_queue;
		RDD::CommandQueueID transfer_queue;
		RDD::CommandQueueID present_queue;

		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID> screen_swap_chains;
		std::unordered_map<DisplayServerEnums::WindowID, RDD::FramebufferID> screen_framebuffers;

		std::vector<Frame> frames;
		int frame = 0;
	};
}
