#include "application.h"
#include <Windows.h>
#include <iostream>

namespace EE
{

	bool CreateConsole() {
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
		FreeConsole();
	}

	bool Application::init_platform()
	{
		return true;
	}

	bool Application::init_wsi()
	{
		vulkan_context.initialize();
		return true;
	}

	void Application::teardown_wsi()
	{
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
		return false;
	}

	void Application::run_frame()
	{
	}

	void Application::check_initialization_progress()
	{
	}
}
