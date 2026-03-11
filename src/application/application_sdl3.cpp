#include <memory>
#include "application.h"
#include "application_entry/application_entry.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "util/logger.h"
#include "util/timer.h"
#include "volk.h"
#include "input/input.h"
#include "vulkan/vulkan_context.h"

namespace EE
{
	class WSIPlatformSDL /*: public WSIPlatform*/
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

			if (!Vulkan::Context::init_loader(
				reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr())))
			{
				LOGE("Failed to initialize Vulkan loader.\n");
				return false;
			}

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

			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				return false;

			case SDL_EVENT_WINDOW_RESIZED:
				break;
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_MOTION:
				if (input) input->on_sdl_event(e);
				break;
			
			default:
				break;
			}

			return true;
		}

		void run_message_loop()
		{
			SDL_Event e;
			while (async_loop_alive && SDL_WaitEvent(&e))		// SDL_WaitEvent (Blocking), CPU Usage: Very low (OS puts thread to sleep), Best for: Applications that don't need continuous updates (editors, menus, idle apps)
			{
				input->update();
				if (!process_sdl_event(e))
					break;
			}
		}

		bool iterate_message_loop()
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))			// SDL_PollEvent (Non-blocking), CPU Usage: High (busy-waiting), Best for: Games that need constant 60+ FPS rendering
			{
				input->update();
				if (!process_sdl_event(e))
					return false;
			}
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

		std::vector<const char*> get_instance_extensions()
		{
			uint32_t count;
			const char* const* ext = SDL_Vulkan_GetInstanceExtensions(&count);
			return { ext, ext + count };
		}

		VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice)
		{
			VkSurfaceKHR surface = VK_NULL_HANDLE;
			if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
				return VK_NULL_HANDLE;

			int actual_width, actual_height;
			SDL_GetWindowSizeInPixels(window, &actual_width, &actual_height);
			width = unsigned(actual_width);
			height = unsigned(actual_height);
			return surface;
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


