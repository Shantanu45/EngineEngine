/*****************************************************************//**
 * \file   application_sdl3.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include <memory>
#include "application.h"
#include "application_entry/application_entry.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "util/logger.h"
#include "util/timer.h"
#include "volk.h"
#include "input/input.h"

namespace EE
{
	class IPlatform
	{
	public:
		virtual ~IPlatform() = default;

		virtual int run(Application* app) = 0;

		virtual std::unique_ptr<Rendering::WindowData> create_window_data() = 0;
		virtual std::vector<const char*> get_device_extensions()
		{
			return { "VK_KHR_swapchain" };
		}

		virtual uint32_t get_surface_width() = 0;
		virtual uint32_t get_surface_height() = 0;
		virtual bool alive(/*WSI& wsi*/) = 0;
		virtual void poll_input() = 0;

		virtual WindowPlatformData get_window_platform_data(DisplayServerEnums::WindowID p_window_id) = 0;

		virtual void release_resources()
		{
		}

	protected:
		unsigned current_swapchain_width = 0;
		unsigned current_swapchain_height = 0;

	};

	class WSIPlatformSDL : public IPlatform
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

		int run(Application* app) override
		{
			init(app->get_name(), app->get_default_width(), app->get_default_height());
			auto data = create_window_data();
			
			app->on_init(DisplayServerEnums::MAIN_WINDOW_ID, data.get());
			app->pre_frame();
			run_loop(app);
			app->post_frame();
			return EXIT_SUCCESS;
		}

		std::unique_ptr<Rendering::WindowData> create_window_data() override
		{
			std::unique_ptr wd = std::make_unique<Rendering::WindowData>();
			wd->platfform_data = get_window_platform_data(DisplayServerEnums::MAIN_WINDOW_ID);
			wd->window_resolution = { get_surface_width(), get_surface_height() };
			return wd;
		}

		explicit WSIPlatformSDL(const Options& options_)
		{
		}

		bool init(const std::string& name, unsigned width_, unsigned height_)
		{
			request_tear_down.store(false);

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

			if (input->just_released(Key::Escape))
			{
				SDL_Event quit_event;
				quit_event.type = SDL_EVENT_QUIT;
				SDL_PushEvent(&quit_event);
			}

			return true;
		}

		void poll_input() override
		{
			if (/*!options.threaded && */!iterate_message_loop())
				request_tear_down = true;
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
			thread_main(app);
		}

		WindowPlatformData get_window_platform_data(DisplayServerEnums::WindowID p_window_id) override
		{
			//WindowData& wd = windows[p_window_id];
			WindowPlatformData platfform_data;
			platfform_data.platform = WindowPlatformData::Platform::SDL3;
			platfform_data.sdl.window = window;

			// TODO: move it from here
			int actual_width, actual_height;
			SDL_GetWindowSizeInPixels(window, &actual_width, &actual_height);
			width = unsigned(actual_width);
			height = unsigned(actual_height);

			return platfform_data;
		}

		uint32_t get_surface_width() override
		{
			return width;
		}

		uint32_t get_surface_height() override
		{
			return height;
		}

		void thread_main(Application* app/*, Global::GlobalManagersHandle ctx*/)
		{
			while (alive())
			{
				poll_input();

				app->run_frame();

			}
		}

		bool alive(/*Vulkan::WSI&*/) override
		{
			//std::lock_guard<std::mutex> holder{ get_input_tracker().get_lock() };
			//flush_deferred_input_events();
			//process_events_async_thread();
			//process_events_async_thread_non_pollable();
			return !request_tear_down.load();
		}

		void notify_close()
		{
			request_tear_down.store(true);
		}
	
	private:
		std::unique_ptr<InputSystem> input;
		uint32_t wake_event_type = 0;
		bool async_loop_alive = true;  // TODO:

		std::atomic_bool request_tear_down;

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

		Locator::ServiceLocator& locator = Services::get();

		// Register real implementations
		locator.provide<FilesystemInterface>(std::make_shared<Filesystem>());
		std::shared_ptr<FilesystemInterface> fs = locator.get<FilesystemInterface>();
		FileSystem::Filesystem::setup_default_filesystem(static_cast<Filesystem*>(fs.get()), "D:/Code/CG/EngineEngine/assets");
		//auto fs = Services::get().get<FilesystemInterface>();

		// creates application
		auto app = std::unique_ptr<Application>(create_application(argc, argv));
		int ret;

		if (app)
		{
			// creates platform 
			auto platform = std::make_unique<WSIPlatformSDL>(options);
			return platform->run(app.get());

		}
		else
		{
			ret = EXIT_FAILURE;
		}
		return ret;
	}
}


