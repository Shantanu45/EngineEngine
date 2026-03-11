#include "application.h"
#include "application_entry/application_entry.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "util/logger.h"
#include <memory>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "util/timer.h"
#include "volk.h"
#include "input/input.h"

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

			wake_event_type = SDL_RegisterEvents(1);

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

			// SHAN: test
			std::unique_ptr is = std::make_unique<InputSystem>();
			set_input_handler(std::move(is));

			return true;
		}

		void set_input_handler(std::unique_ptr<InputSystem> input) { this->input = std::move(input); }

		bool process_sdl_event(const SDL_Event& e)
		{
			//if (e.type == wake_event_type)
			//{
			//	LOGI("Processing events main thread.\n");
			//	process_events_main_thread();
			//	return true;
			//}

			//const auto dispatcher = [this](std::function<void()> func) {
			//	push_task_to_async_thread(std::move(func));
			//	};

			//if (pad.process_sdl_event(e, get_input_tracker(), dispatcher))
			//	return true;

			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				return false;

			case SDL_EVENT_WINDOW_RESIZED:
				//if (e.window.windowID == SDL_GetWindowID(window))
				//	notify_resize(e.window.data1, e.window.data2);
				break;
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
				if (input) input->on_sdl_event(e);

			
			default:
				break;
			}

			return true;
		}

		void run_message_loop()
		{
			SDL_Event e;
			while (async_loop_alive && SDL_WaitEvent(&e))
				if (!process_sdl_event(e))
					break;
		}

		bool iterate_message_loop()
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))
				if (!process_sdl_event(e))
					return false;
			return true;
		}

		template <typename Op>
		void push_task_to_main_thread(Op&& op)
		{
			//push_task_to_list(task_list_main, std::forward<Op>(op));
			//SDL_Event wake_event = {};
			//wake_event.type = wake_event_type;
			//SDL_PushEvent(&wake_event);
		}

		void run_loop(Application* app)
		{
			run_message_loop();
		}
	
	private:
		std::unique_ptr<InputSystem> input;
		uint32_t wake_event_type = 0;
		bool async_loop_alive = true;  // TODO:

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


