/*****************************************************************//**
 * \file   wsi.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma  once

#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"
#include "rendering/gltf_loader.h"
#include "math/rect2.h"


namespace Rendering
{
	class RendererCompositor;

	// display server windows
	struct WindowData {
		WindowPlatformData platfform_data;
		Size2i window_resolution;
	};

	class WSI
	{
		struct MeshRange {
			uint32_t vertexOffset;  // offset into the big vertex buffer
			uint64_t vertex_byte_offset;
			uint32_t indexOffset;   // offset into the big index buffer
			uint32_t index_count;
		};

		enum class VERTEX_DATA_MODE
		{
			INTERLEVED_DATA,
			SEPERATE
		};

	public:
		WSI();
		Error initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window);

		bool pre_frame_loop();

		void blit_render_target_to_screen(RID texture);

		bool pre_begin_frame();

		bool begin_frame();

		bool end_render_pass(RDD::CommandBufferID cmd);
		bool end_frame(bool p_present);

		bool post_end_frame();

		bool post_frame_loop();

		void set_program(const std::vector<std::string> programs);

		void set_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride);

		RID get_current_pipeline();

		RenderingShaderContainerFormat* create_shader_container_format();

		void bind_and_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer);
		RenderingDevice* get_rendering_device() { return rendering_device; }

		RID get_bound_shader() {
			return shader_program;
		};

		void set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data);

		Error load_gltf(std::string path);
		void set_default_vertex_attribute();
		void push_vertex_data(void* vertex_data, size_t size);
		void push_index_data(void* data, size_t size);

		void clear_vertex_data() { vertex_data.clear(); }
		void clear_index_data() { index_data.clear(); }

		void pipeline_create();

		//void pipeline_create_default();

		inline void set_vertex_data_mode(VERTEX_DATA_MODE mode)
		{
			vertex_data_mode = mode;
		};

		void set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat format);

		void teardown();

		RDD::RenderPassID get_current_render_pass()
		{
			return render_pass;
		}

		RDD::FramebufferID get_current_frame_buffer()
		{
			return frame_buffer;
		}

		RID get_texture_fb()
		{
			return texture_fb;
		}

		~WSI();
	private:

		Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver = "vulkan");
		void _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);

		void _free_pending_resources(int p_frame);
		std::vector<uint8_t> _get_attrib_interleaved(const std::vector<RenderingDeviceCommons::VertexAttribute>& attribs, std::vector<uint8_t> vertex_data);

		void _create_vertex_and_index_buffers();
		std::unique_ptr<RenderingContextDriver> rendering_context = nullptr;
		RenderingDevice* rendering_device = nullptr;

		RenderingContextDriver::SurfaceID surface;
		RenderingDeviceDriver::SwapChainID swapchain;

		uint32_t frame_count = 0;
		uint32_t curr_frame = 0;
		RenderingDeviceDriver::CommandQueueID main_queue;
		RID pipeline;

		std::map<DisplayServerEnums::WindowID, WindowData> windows;

		std::string rendering_driver;
		bool main_window_created = false;
		RID shader_program;

		RenderingDevice::VertexFormatID vertex_format;

		DisplayServerEnums::WindowID active_window = DisplayServerEnums::INVALID_WINDOW_ID;

		std::vector<RenderingDeviceCommons::VertexAttribute> vertex_attributes;
		std::vector<uint8_t> vertex_data{};
		std::vector<uint8_t> index_data{};
		uint32_t index_count = 0;

		VERTEX_DATA_MODE vertex_data_mode = VERTEX_DATA_MODE::INTERLEVED_DATA;

		RenderingDeviceCommons::IndexBufferFormat index_data_format = RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT16;

		RID_Owner<MeshPrimitive, true> mesh_owner;

		std::unordered_map<RID, MeshRange> primitives;

		std::unique_ptr<GltfLoader> gltf_loader = nullptr;

		uint32_t total_vertices = 0;
		uint32_t total_indices = 0;

		std::unordered_map<RID, RID> vertex_arrays;		// prim to array
		std::unordered_map<RID, RID> index_arrays;

		std::unique_ptr<RendererCompositor> rd;

		RDD::RenderPassID render_pass; 
		RDD::FramebufferID frame_buffer;
		RID texture_fb;
	};
}
