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
		void pipeline_create();
		void set_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride);
		void pipeline_create_default();
		RID get_current_pipeline();
		RenderingShaderContainerFormat* create_shader_container_format();

		void create_triangle();
		void bind();
		RenderingDevice* get_rendering_device() { return rendering_device; }

		void teardown();

		~WSI();

		void set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data);
	private:

		Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver = "vulkan");
		void _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);

		void free_pending_resources(int p_frame);

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

	};
}
