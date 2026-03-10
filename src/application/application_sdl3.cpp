#include "application.h"
#include "application_entry/application_entry.h"
#include <iostream>
#include "SDL3/SDL.h"
#include "util/logger.h"
#include <memory>
#include "spdlog/sinks/stdout_color_sinks.h"

namespace EE
{

	class WSIPlatformSDL
	{
	public:
		~WSIPlatformSDL()
		{
			
		}
		struct Options
		{
			unsigned override_width = 0;
			unsigned override_height = 0;
			bool fullscreen = false;
			bool threaded = true;
		};

		explicit WSIPlatformSDL(const Options& options_)
		{
		}

		bool init(const std::string& name, unsigned width_, unsigned height_)
		{
			width = width_;
			height = height_;
			return true;
		}

		void run_loop(Application* app)
		{
			while (true);
		}
	
	private:
		SDL_Window* window = nullptr;
		unsigned width = 0;
		unsigned height = 0;
	};
}

namespace EE
{
	int application_main(Application* (*create_application)(int, char**), int argc, char* argv[])
	{

		WSIPlatformSDL::Options options;
		int exit_code;

		auto app = std::unique_ptr<Application>(create_application(argc, argv));
		int ret;

		if (app)
		{
			auto platform = std::make_unique<WSIPlatformSDL>(options);
			auto* platform_handle = platform.get();

			if (!platform->init(app->get_name(), app->get_default_width(), app->get_default_height()))
				return 1;

			if (!app->init_platform())
				return 1;

			if (!app->init_wsi())
			{
				return 1;
			}

			platform_handle->run_loop(app.get());

			app.reset();
			ret = EXIT_SUCCESS;
		}
		else
		{
			ret = EXIT_FAILURE;
		}
		return ret;
	}
}


