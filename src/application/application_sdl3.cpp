#include "application.h"
#include "application_entry/application_entry.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "util/logger.h"
#include <memory>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "util/timer.h"
#include "volk.h"

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

			Util::Timer tmp_timer;
			tmp_timer.start();
			if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO))
			{
				LOGE("Failed to init SDL.\n");
				return false;
			}
			LOGI("SDL_Init took %.3f seconds.\n", tmp_timer.end());

			SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, false);
			SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, false);

			if (!SDL_Vulkan_LoadLibrary(nullptr))
			{
				LOGE("Failed to load Vulkan library.\n");
				return false;
			}

			//if (!Vulkan::Context::init_loader(
			//	reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr())))
			//{
			//	LOGE("Failed to initialize Vulkan loader.\n");
			//	return false;
			//}

			application.name = name;
			if (application.name.empty())
				application.name = "EngineEngine";

			window = SDL_CreateWindow(application.name.empty() ? "SDL Window" : application.name.c_str(),
				int(width), int(height), SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
			if (!window)
			{
				LOGE("Failed to create SDL window.\n");
				return false;
			}

			application.info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application.info.pEngineName = "Maker";
			application.info.pApplicationName = application.name.empty() ? "Maker" : application.name.c_str();
			application.info.apiVersion = VK_API_VERSION_1_1;

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

		struct
		{
			VkApplicationInfo info = {};
			std::string name;
		} application;
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


