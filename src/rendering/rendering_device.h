#pragma once
#include "rendering_device_commons.h"
#include "rendering_device_driver.h"
#include "rendering_context_driver.h"

namespace Rendering
{
	class RenderingDevice : public RenderingDeviceCommons
	{
	private:
		
	public:
		typedef int64_t DrawListID;

		static RenderingDevice* get_singleton() {
			static RenderingDevice* singleton = new RenderingDevice();
			return singleton;
		};

		Error initialize(RenderingContextDriver* p_context, DisplayServerEnums::WindowID p_main_window = DisplayServerEnums::INVALID_WINDOW_ID);

		void finalize();


#pragma region Shader
		std::vector<uint8_t> shader_compile_spirv_from_source(ShaderStage p_stage, const std::string& p_source_code, ShaderLanguage p_language = SHADER_LANGUAGE_GLSL, std::string* r_error = nullptr, bool p_allow_cache = true);
		std::vector<uint8_t> shader_compile_binary_from_spirv(const std::vector<ShaderStageSPIRVData>& p_spirv, const std::string& p_shader_name = "");
		RenderingDeviceDriver::ShaderID shader_create_from_spirv(const std::vector<ShaderStageSPIRVData>& p_spirv, const  std::string& p_shader_name = "");
		//RenderingDeviceDriver::ShaderID shader_create_from_bytecode(const std::vector<uint8_t>& p_shader_binary, RID p_placeholder = RID());
		void shader_destroy_modules(RenderingDeviceDriver::ShaderID p_shader);
		uint64_t shader_get_vertex_input_attribute_mask(RenderingDeviceDriver::ShaderID p_shader);

#pragma endregion
#pragma region Pipeline
		typedef int64_t FramebufferFormatID;
		RenderingDeviceDriver::PipelineID render_pipeline_create(RenderingDeviceDriver::PipelineID p_shader, FramebufferFormatID p_framebuffer_format, RenderingDeviceDriver::VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags = 0, uint32_t p_for_render_pass = 0, const std::vector<PipelineSpecializationConstant>& p_specialization_constants = std::vector<PipelineSpecializationConstant>());
		bool render_pipeline_is_valid(RenderingDeviceDriver::PipelineID p_pipeline);

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

		//RID _sampler_create(const RDSamplerState* p_state);

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
