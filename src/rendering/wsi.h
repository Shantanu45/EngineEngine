/*****************************************************************//**
 * \file   wsi.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"

namespace Rendering
{
	enum class VERTEX_DATA_MODE
	{
		INTERLEVED_DATA,
		SEPERATE
	};
	// display server windows
	struct WindowData {
		WindowPlatformData platfform_data;
		Size2i window_resolution;
	};

	class WSI /*: protected RenderingDeviceCommons*/
	{
	public:
		//WSI(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error& r_error);
		WSI();
		Error initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window);

		bool pre_frame_loop();

		bool pre_begin_frame();

		bool begin_frame();

		bool end_frame();

		bool post_end_frame();

		bool post_frame_loop();

		void set_program(const std::vector<std::string> programs);

		void set_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride);

		RID get_current_pipeline();

		RenderingShaderContainerFormat* create_shader_container_format();

		void bind_vbo_and_ibo();

		RenderingDevice* get_rendering_device() { return rendering_device; }

		RID get_bound_shader() {
			return shader_program;
		};

		void set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data);

		void push_vertex_data(void* vertex_data, size_t size);
		void push_index_data(void* data, size_t size);

		void clear_vertex_data() { vertex_data.clear(); }
		void clear_index_data() { index_data.clear(); }

		void pipeline_create();
		void pipeline_create_default();

		inline void set_vertex_data_mode(VERTEX_DATA_MODE mode)
		{
			vertex_data_mode = mode;
		};

		void set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat format);

		void teardown();

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

		RID triangle_vertex_buffer;
		RID triangle_index_buffer;
		RID triangle_vertex_array;
		RID triangle_index_array;
		RenderingDevice::VertexFormatID vertex_format;

		DisplayServerEnums::WindowID active_window = DisplayServerEnums::INVALID_WINDOW_ID;

		std::vector<RenderingDeviceCommons::VertexAttribute> vertex_attributes;
		std::vector<uint8_t> vertex_data{};
		std::vector<uint8_t> index_data{};
		uint32_t index_count = 0;

		VERTEX_DATA_MODE vertex_data_mode = VERTEX_DATA_MODE::INTERLEVED_DATA;

		RenderingDeviceCommons::IndexBufferFormat index_data_format = RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32;
	};
}
