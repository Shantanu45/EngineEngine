/*****************************************************************//**
 * \file   application.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include <iostream>
#include <Windows.h>
#include "application.h"
#include "libassert/assert.hpp"
#include "rendering/rendering_context_driver.h"

namespace EE
{
	static bool CreateConsole() {
		// 1. Attempt to allocate a console
		if (!AllocConsole()) {
			return false;
		}

		// 2. Redirect C-style streams (printf/fprintf)
		FILE* fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONIN$", "r", stdin);

		// 3. Sync C++ streams (std::cout/std::cerr)
		std::cout.clear();
		std::clog.clear();
		std::cerr.clear();
		std::cin.clear();

		// 4. Enable "Virtual Terminal Processing" (Modern ANSI Colors)
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut != INVALID_HANDLE_VALUE) {
			DWORD dwMode = 0;
			if (GetConsoleMode(hOut, &dwMode)) {
				dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				SetConsoleMode(hOut, dwMode);
			}
		}

		printf("Console Subsystem: [READY]\n");
		return true;
	}

	Application::Application()
	{
		CreateConsole();
		logger = std::make_unique<::Util::StdSpdLogger>();
		application_wsi = std::make_unique<Rendering::WSI>();
		::Util::set_logger_iface(logger.get());
	}

	Application::~Application()
	{
		teardown_wsi();
		FreeConsole();
	}

	bool Application::on_init(DisplayServerEnums::WindowID p_window, Rendering::WindowData* p_window_data)
	{
		application_wsi->set_wsi_platform_data(p_window, *p_window_data);

		if (init_wsi())
		{
			return true;
		}
		return false;
	}

	bool Application::init_wsi()
	{
		DEBUG_ASSERT(application_wsi->initialize("vulkan", DisplayServerEnums::WINDOW_MODE_WINDOWED, DisplayServerEnums::VSYNC_DISABLED, 0, {}, {}, 0, DisplayServerEnums::CONTEXT_ENGINE, 0) == OK);

		return true;
	}

	void Application::pre_frame()
	{
		application_wsi->pre_frame_loop();
	}

	void Application::run_frame(double frame_time, double elapsed_time)
	{
		application_wsi->pre_begin_frame();
		if (!application_wsi->begin_frame())
			return;

		render_frame(frame_time, elapsed_time);

		application_wsi->end_frame(true);
		application_wsi->post_end_frame();
	}

	void Application::post_frame()
	{
		application_wsi->post_frame_loop();
	}

	void Application::render_early_loading(double frame_time, double elapsed_time)
	{
	}

	void Application::render_loading(double frame_time, double elapsed_time)
	{
	}

	void Application::teardown_wsi()
	{
		application_wsi->teardown();
		ready_modules = false;
		ready_pipelines = false;
	}

	void Application::_check_initialization_progress()
	{
	}

	void Application::_draw()
	{
	}
}
