#pragma once
#include <memory>
#include <string>
#include "util/logger.h"
#include "rendering/wsi.h"

namespace EE
{
	class Application
	{
	public:
		Application();
		virtual ~Application();
		virtual void render_frame(double frame_time, double elapsed_time) = 0;
		bool init_platform(std::unique_ptr<Rendering::WSIPlatform> new_platform);
		bool init_wsi();
		void teardown_wsi();

		virtual void post_frame();

		virtual void render_early_loading(double frame_time, double elapsed_time);

		virtual void render_loading(double frame_time, double elapsed_time);

		virtual std::string get_name()
		{
			return "maker";
		}

		virtual unsigned get_version()
		{
			return 0;
		}

		virtual unsigned get_default_width()
		{
			return 1280;
		}

		virtual unsigned get_default_height()
		{
			return 720;
		}

		Rendering::WSI* get_wsi() const
		{
			return application_wsi.get();
		}

		Rendering::WSIPlatform* get_platform() const
		{
			return platform.get();
		}

		bool poll();
		void run_frame();
		//void show_message_box(const std::string& str, Vulkan::WSIPlatform::MessageType type);

	protected:
		void request_shutdown()
		{
			requested_shutdown = true;
		}

		//void poll_input_tracker_async(InputTrackerHandler* override_handler);

	private:
		bool requested_shutdown = false;

		// Ready state for deferred device initialization.
		bool ready_modules = false;
		bool ready_pipelines = false;
		void _check_initialization_progress();
		void _draw();

		std::unique_ptr<::Util::Logger> logger = nullptr;

		//Vulkan::RenderingContextDriverVulkan vulkan_context;
		//std::unique_ptr<Vulkan::RenderingDeviceDriverVulkan> vulkan_device_ptr = nullptr;

		std::unique_ptr<Rendering::WSIPlatform> platform;
		std::unique_ptr<Rendering::WSI> application_wsi;
	};
}