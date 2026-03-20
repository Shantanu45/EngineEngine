#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"
#include "wsi_platform.h"

namespace Rendering
{
	// display server windows
	struct WindowData {
		WindowPlatformData platfform_data;
	};

	class WSI
	{
	public:
		//WSI(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error& r_error);
		WSI();
		Error initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window);
		void set_platform(WSIPlatform* platform);

		bool begin_frame();

		void draw_viewport(bool p_swap_buffers);

		bool end_frame();

		void set_program(const std::vector<std::string> programs);
		void pipeline_create();
		void pipeline_create_default();
		RID get_current_pipeline();
		RenderingShaderContainerFormat* create_shader_container_format();

		RenderingDevice* get_rendering_device() { return rendering_device; }

		void teardown();

		~WSI();

	private:

		Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver = "vulkan");
		void _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);

		void free_pending_resources(int p_frame);
		WSIPlatform* platform = nullptr;

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


	};
}
