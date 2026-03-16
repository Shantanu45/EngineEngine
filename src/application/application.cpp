#include <iostream>
#include <Windows.h>
#include "application.h"
#include "libassert/assert.hpp"

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
		::Util::set_logger_iface(logger.get());
	}

	Application::~Application()
	{
		teardown_wsi();
		FreeConsole();
	}

	bool Application::init_platform(std::unique_ptr<Vulkan::WSIPlatform> new_platform)
	{
		platform = std::move(new_platform);
		application_wsi.set_platform(platform.get());
		return true;
	}

	bool Application::init_wsi()
	{
		DEBUG_ASSERT(application_wsi.init_context());
		DEBUG_ASSERT(application_wsi.init_device());
		return true;
	}

	void Application::teardown_wsi()
	{
		application_wsi.teardown();
		ready_modules = false;
		ready_pipelines = false;
	}

	void Application::post_frame()
	{
	}

	void Application::render_early_loading(double frame_time, double elapsed_time)
	{
	}

	void Application::render_loading(double frame_time, double elapsed_time)
	{
	}

	bool Application::poll()
	{
		auto& wsi = get_wsi();
		if (!get_platform().alive(wsi))
			return false;
		return true;
	}

	void Application::run_frame()
	{
		if (!application_wsi.begin_frame())
		{
			return;
		}

		render_frame(0, 0);

		application_wsi.end_frame();
	}

	void Application::check_initialization_progress()
	{
	}
}
