#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"
#include "wsi_platform.h"

namespace Rendering
{

	struct WindowData {
		WindowPlatformData platfform_data;
	};

	class WSI
	{
	public:
		WSI(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error& r_error);
		~WSI();
		void set_platform(WSIPlatform* platform);

		bool init_context();
		bool init_device();
		bool begin_frame();
		bool end_frame();

		void teardown();

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
		RenderingDeviceDriver::PipelineID pipeline;

		std::map<DisplayServerEnums::WindowID, WindowData> windows;

		std::string rendering_driver;
		bool main_window_created = false;


	};
}
