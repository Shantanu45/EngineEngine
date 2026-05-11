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
#include "application_options.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "util/logger.h"
#include "util/timer.h"
#include "util/profiler.h"
#include "util/small_vector.h"
#include "volk.h"
#include "input/input.h"
#include "application_events.h"
#include "util/renderdoc_helpers.h"
#include <CLI/CLI.hpp>

namespace EE
{
	class IPlatform
	{
	public:
		virtual ~IPlatform() = default;

		virtual int run(Application* app) = 0;

		virtual std::unique_ptr<Rendering::WindowData> create_window_data() = 0;
		virtual Util::SmallVector<const char*> get_device_extensions()
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
			_app = app;
			unsigned w = options.override_width  ? options.override_width  : app->get_default_width();
			unsigned h = options.override_height ? options.override_height : app->get_default_height();
			ERR_FAIL_COND_V_MSG(!init(app->get_name(), w, h, options.fullscreen), EXIT_FAILURE, "SDL initialization failed");
			auto data = create_window_data();
			
			ERR_FAIL_COND_V_MSG(!app->on_init(DisplayServerEnums::MAIN_WINDOW_ID, data.get()), EXIT_FAILURE, "on init failed");
			ERR_FAIL_COND_V_MSG(!app->pre_frame(), EXIT_FAILURE, "pre frame failed");

			event_manager = Services::get().get<EventManager>().get();

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
			: options(options_)
		{
		}

		bool init(const std::string& name, unsigned width_, unsigned height_, bool fullscreen_)
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

			SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
			if (fullscreen_)
				window_flags |= SDL_WINDOW_FULLSCREEN;
			window = SDL_CreateWindow(application.name.empty() ? "SDL Window" : application.name.c_str(),
				int(width), int(height), window_flags);
			if (!window)
			{
				LOGE("Failed to create SDL window.\n");
				return false;
			}

			application.info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application.info.pEngineName = "Maker";
			application.info.pApplicationName = application.name.empty() ? "Maker" : application.name.c_str();
			application.info.apiVersion = VK_API_VERSION_1_1;

			auto is = Services::get().get<InputSystemInterface>();
			set_input_handler(is);

			get_frame_timer()->reset();
			return true;
		}

		void set_input_handler(std::shared_ptr<InputSystemInterface> input) { this->input = input; }

		bool process_sdl_event(const SDL_Event& e)
		{

			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				return false;

			case SDL_EVENT_WINDOW_RESIZED:
			{
				int w = e.window.data1;
				int h = e.window.data2;
				event_manager->enqueue<WindowResizeEvent>(w, h);
				break;
			}
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
			ZoneScopedN("poll_input");
			if (/*!options.threaded && */!iterate_message_loop())
				request_tear_down = true;
		}

		void run_message_loop()
		{
			SDL_Event e;

			input->update();
			while (async_loop_alive && SDL_WaitEvent(&e))		// SDL_WaitEvent (Blocking), CPU Usage: Very low (OS puts thread to sleep), Best for: Applications that don't need continuous updates (editors, menus, idle apps)
			{
				_app->app_poll(&e);
				if (!process_sdl_event(e))
					break;
			}
		}

		bool iterate_message_loop()
		{
			SDL_Event e;

			input->update();
			while (SDL_PollEvent(&e))			// SDL_PollEvent (Non-blocking), CPU Usage: High (busy-waiting), Best for: Games that need constant 60+ FPS rendering
			{
				_app->app_poll(&e);
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
#ifdef TRACY_ENABLE
			tracy::SetThreadName("Main");
#endif
			while (alive())
			{
				frame_time = get_frame_timer()->frame();
				elapsed_time = get_frame_timer()->get_elapsed();

				poll_input();

				event_manager->dispatch();

				app->run_frame(frame_time, elapsed_time);
				FrameMark;
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
	
		Util::FrameTimer* get_frame_timer()
		{
			return Services::get().get<Util::FrameTimer>().get();
		}

	private:
		Options options;
		std::shared_ptr<InputSystemInterface> input;
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

		Util::FrameTimer* timer;

		Application* _app;

		double frame_time;
		double elapsed_time;

		EventManager* event_manager;
	};
}

namespace EE
{
	int application_main(Application* (*create_application)(int, char**), int argc, char* argv[])
	{
		AppOptions opts;
		CLI::App cli{"EngineEngine"};
		cli.add_option("--assets",     opts.assets_path, "Asset directory path (default: <exe>/../../../assets)");
		cli.add_option("--width",  opts.width,  "Window width  (default: application default)");
		cli.add_option("--height", opts.height, "Window height (default: application default)");
		cli.add_flag  ("--fullscreen", opts.fullscreen,  "Fullscreen mode");
		cli.add_flag  ("--attach-rdoc", opts.attatch_rdoc,  "Attach render doc");
		cli.add_option("--rdoc-folder", opts.rdoc_folder,  "Renderdoc folder (deafult: C:/Program Files/RenderDoc/)");
		cli.add_option("--capture-path", opts.rdoc_capture_path,  "Path captures will be safer (deafult: temp/)");
		cli.add_flag  ("--no-vsync{false}", opts.vsync,  "Disable vsync");
		cli.add_option("--render-mode,--lighting-model", opts.render_mode,
			"Initial lighting model: regular or pbr")
			->check(CLI::IsMember({ "regular", "pbr" }));
		CLI11_PARSE(cli, argc, argv);

		if (opts.attatch_rdoc)
		{
			Util::init_render_doc(opts.rdoc_folder, opts.rdoc_capture_path);
		}

		WSIPlatformSDL::Options wsi_options;
		wsi_options.override_width  = opts.width;
		wsi_options.override_height = opts.height;
		wsi_options.fullscreen      = opts.fullscreen;

		Locator::ServiceLocator& locator = Services::get();

		// Register real implementations
		locator.provide<AppOptions>(std::make_shared<AppOptions>(opts));
		locator.provide<InputSystemInterface>(std::make_shared<InputSystem>());

		locator.provide<FilesystemInterface>(std::make_shared<Filesystem>());
		locator.provide<Util::FrameTimer>(std::make_shared<Util::FrameTimer>());
		locator.provide<EE::EventManager>(std::make_shared<EE::EventManager>());
		std::shared_ptr<FilesystemInterface> fs = locator.get<FilesystemInterface>();
		const std::string exe_path = Path::get_executable_path();
		const std::string assets_dir = opts.assets_path.empty()
			? Path::join(exe_path, "../../../assets")
			: opts.assets_path;
		FileSystem::Filesystem::setup_default_filesystem(static_cast<Filesystem*>(fs.get()), assets_dir.c_str());


		//auto fs = Services::get().get<FilesystemInterface>();

		// creates application
		auto app = std::unique_ptr<Application>(create_application(argc, argv));
		int ret = EXIT_FAILURE;

		if (app)
		{
			// creates platform 
			auto platform = std::make_unique<WSIPlatformSDL>(wsi_options);
			int result = platform->run(app.get());

			// Tear down the Vulkan device/context before unloading RenderDoc. RenderDoc
			// asserts if its Vulkan resource map still contains live objects at detach.
			app.reset();

			ret = result;
		}
		
		if (opts.attatch_rdoc && Util::rdoc != nullptr)
		{
			Util::shutdown_render_doc();
		}

		return ret;
	}
}


