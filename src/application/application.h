/*****************************************************************//**
 * \file   application.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once
#include <memory>
#include <string>
#include "util/logger.h"
#include "rendering/wsi.h"
#include "service_locator.h"
#include "rendering/wsi.h"

namespace EE
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

		bool on_init(DisplayServerEnums::WindowID p_window, Rendering::WindowData* p_window_data);

		bool init_wsi();

		virtual void pre_frame();

		/**
		 * application must overload at least this function. runs in loop between begin and end frame
		 * 
		 * \param frame_time
		 * \param elapsed_time
		 */
		virtual void render_frame(double frame_time, double elapsed_time) = 0;

		void run_frame(double frame_time, double elapsed_time);

		virtual void post_frame();

		virtual void teardown_application() = 0;

		virtual void render_early_loading(double frame_time, double elapsed_time);

		virtual void render_loading(double frame_time, double elapsed_time);

		virtual void app_poll(void* e);

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

		void teardown_wsi();


	protected:
		void request_shutdown()
		{
			requested_shutdown = true;
		}

	private:
		bool requested_shutdown = false;

		// Ready state for deferred device initialization.
		bool ready_modules = false;
		bool ready_pipelines = false;
		void _check_initialization_progress();
		void _draw();

		std::unique_ptr<::Util::Logger> logger = nullptr;

		std::unique_ptr<Rendering::WSI> application_wsi;
	};
}