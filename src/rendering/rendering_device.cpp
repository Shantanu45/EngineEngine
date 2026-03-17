#include "rendering_device.h"

namespace Rendering
{
	RenderingDevice::~RenderingDevice() {
		finalize();
	}

	Error RenderingDevice::initialize(RenderingContextDriver* p_context, DisplayServerEnums::WindowID p_main_window)
	{
		Error err;
		RenderingContextDriver::SurfaceID main_surface = 0;

		is_main_instance = (get_singleton() == this) && (p_main_window != DisplayServerEnums::INVALID_WINDOW_ID);

		if (p_main_window != DisplayServerEnums::INVALID_WINDOW_ID) {
			// Retrieve the surface from the main window if it was specified.
			main_surface = p_context->surface_get_from_window(p_main_window);
			ERR_FAIL_COND_V(main_surface == 0, FAILED);
		}

		context = p_context;
		driver = context->driver_create();

		LOGI("Devices:");
		//int32_t device_index = Engine::get_singleton()->get_gpu_index();
		//const uint32_t device_count = context->device_get_count();
		//const bool detect_device = (device_index < 0) || (device_index >= int32_t(device_count));
		//uint32_t device_type_score = 0;
		//for (uint32_t i = 0; i < device_count; i++) {
		//	RenderingContextDriver::Device device_option = context->device_get(i);
		//	String name = device_option.name;
		//	String vendor = _get_device_vendor_name(device_option);
		//	String type = _get_device_type_name(device_option);
		//	bool present_supported = main_surface != 0 ? context->device_supports_present(i, main_surface) : false;
		//	print_verbose("  #" + itos(i) + ": " + vendor + " " + name + " - " + (present_supported ? "Supported" : "Unsupported") + ", " + type);
		//	if (detect_device && (present_supported || main_surface == 0)) {
		//		// If a window was specified, present must be supported by the device to be available as an option.
		//		// Assign a score for each type of device and prefer the device with the higher score.
		//		uint32_t option_score = _get_device_type_score(device_option);
		//		if (option_score > device_type_score) {
		//			device_index = i;
		//			device_type_score = option_score;
		//		}
		//	}
		//}

		uint32_t frame_count = 2;

		//device = context->device_get(device_index);
		err = driver->initialize(0/*device_index*/, frame_count);

		BitField<RDD::CommandQueueFamilyBits> main_queue_bits = {};
		main_queue_bits.set_flag(RDD::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);
		main_queue_bits.set_flag(RDD::COMMAND_QUEUE_FAMILY_COMPUTE_BIT);

#if !FORCE_SEPARATE_PRESENT_QUEUE
		// Needing to use a separate queue for presentation is an edge case that remains to be seen what hardware triggers it at all.
		main_queue_family = driver->command_queue_family_get(main_queue_bits, main_surface);
		if (!main_queue_family && (main_surface != 0))
#endif
		{
			// If it was not possible to find a main queue that supports the surface, we attempt to get two different queues instead.
			main_queue_family = driver->command_queue_family_get(main_queue_bits);
			present_queue_family = driver->command_queue_family_get(BitField<RDD::CommandQueueFamilyBits>(), main_surface);
			ERR_FAIL_COND_V(!present_queue_family, FAILED);
		}

		ERR_FAIL_COND_V(!main_queue_family, FAILED);

		// Create the main queue.
		main_queue = driver->command_queue_create(main_queue_family, true);
		ERR_FAIL_COND_V(!main_queue, FAILED);
		//TODO: transfer_queue_family

		if (present_queue_family) {
			// Create the present queue.
			present_queue = driver->command_queue_create(present_queue_family);
			ERR_FAIL_COND_V(!present_queue, FAILED);
		}
		else {
			// Use main queue as the present queue.
			present_queue = main_queue;
			present_queue_family = main_queue_family;
		}

		frames.resize(frame_count);

		bool frame_failed = false;
		for (uint32_t i = 0; i < frames.size(); i++) {
			frames[i].index = 0;
			frames[i].command_pool = driver->command_pool_create(main_queue_family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
			if (!frames[i].command_pool) {
				frame_failed = true;
				break;
			}
			frames[i].command_buffer = driver->command_buffer_create(frames[i].command_pool);
			if (!frames[i].command_buffer) {
				frame_failed = true;
				break;
			}
			frames[i].semaphore = driver->semaphore_create();
			if (!frames[i].semaphore) {
				frame_failed = true;
				break;
			}
			frames[i].fence = driver->fence_create();
			if (!frames[i].fence) {
				frame_failed = true;
				break;
			}
			frames[i].fence_signaled = false;
		}
		if (frame_failed) {
			// Clean up created data.
			for (uint32_t i = 0; i < frames.size(); i++) {
				if (frames[i].command_pool) {
					driver->command_pool_free(frames[i].command_pool);
				}
				if (frames[i].semaphore) {
					driver->semaphore_free(frames[i].semaphore);
				}
				if (frames[i].fence) {
					driver->fence_free(frames[i].fence);
				}
				//if (frames[i].timestamp_pool) {
				//	device_ptr->timestamp_query_pool_free(frames[i].timestamp_pool);
				//}
				//for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
				//	if (frames[i].transfer_worker_semaphores[j]) {
				//		device_ptr->semaphore_free(frames[i].transfer_worker_semaphores[j]);
				//	}
				//}
			}
			frames.clear();
			ERR_FAIL_V_MSG(FAILED, "Failed to create frame data.");
		}

		return Error();
	}

	void RenderingDevice::finalize() {

	}

	uint32_t RenderingDevice::_get_swap_chain_desired_count() const {
		return 2;
	}

	Error RenderingDevice::screen_create(DisplayServerEnums::WindowID p_screen)
	{
		RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
		ERR_FAIL_COND_V_MSG(surface == 0, ERR_CANT_CREATE, "A surface was not created for the screen.");

		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it != screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was already created for the screen.");

		RDD::SwapChainID swap_chain = driver->swap_chain_create(surface);
		ERR_FAIL_COND_V_MSG(swap_chain.id == 0, ERR_CANT_CREATE, "Unable to create swap chain.");

		screen_swap_chains[p_screen] = swap_chain;

		bool resize_required = false;
		RDD::FramebufferID framebuffer = driver->swap_chain_acquire_framebuffer(main_queue, it->second, resize_required);
		if (resize_required) {
			Error err = driver->swap_chain_resize(main_queue, it->second, _get_swap_chain_desired_count());
			if (err != OK) {
				// Resize is allowed to fail silently because the window can be minimized.
				return err;
			}

			framebuffer = driver->swap_chain_acquire_framebuffer(main_queue, it->second, resize_required);
		}

		if (framebuffer.id == 0) {
			// Some drivers like NVIDIA are fast enough to invalidate the swap chain between resizing and acquisition (GH-94104).
			// This typically occurs during continuous window resizing operations, especially if done quickly.
			// Allow this to fail silently since it has no visual consequences.
			return ERR_CANT_CREATE;
		}

		// Store the framebuffer that will be used next to draw to this screen.
		screen_framebuffers[p_screen] = framebuffer;
		frames[frame].swap_chains_to_present.push_back(it->second);

		return OK;
	}
	Error RenderingDevice::screen_prepare_for_drawing(DisplayServerEnums::WindowID p_screen)
	{
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was not created for the screen.");
	}
	void RenderingDevice::swap_buffers(bool p_present)
	{
		_end_frame();

		_execute_frame(p_present);

		frame = (frame + 1) % frames.size();

		_begin_frame(true);
	}
}
