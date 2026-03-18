#include "rendering_device.h"
#include <set>

namespace Rendering
{

	Rendering::RenderingDevice::VertexFormatID RenderingDevice::vertex_format_create(const std::vector<VertexAttribute>& p_vertex_descriptions)
	{
		VertexDescriptionKey key;
		key.vertex_formats = p_vertex_descriptions;

		VertexFormatID* idptr = &vertex_format_cache[key];
		if (idptr) {
			return *idptr;
		}

		VertexAttributeBindingsMap bindings;
		bool has_implicit = false;
		bool has_explicit = false;
		std::vector<VertexAttribute> vertex_descriptions = p_vertex_descriptions;
		std::set<int> used_locations;

		for (int i = 0; i < vertex_descriptions.size(); i++) {
			VertexAttribute& attr = vertex_descriptions[i];
			ERR_CONTINUE(attr.format >= DATA_FORMAT_MAX);
			ERR_FAIL_COND_V(used_locations.contains(attr.location), INVALID_ID);

			ERR_FAIL_COND_V_MSG(get_format_vertex_size(attr.format) == 0, INVALID_ID,
				std::format("Data format for attribute (%d), '%s', is not valid for a vertex array.", attr.location, std::string(FORMAT_NAMES[attr.format])));

			if (attr.binding == UINT32_MAX) {
				attr.binding = i; // Implicitly assigned binding
				has_implicit = true;
			}
			else {
				has_explicit = true;
			}
			ERR_FAIL_COND_V_MSG(!(has_implicit ^ has_explicit), INVALID_ID, "Vertex attributes must use either all explicit or all implicit bindings.");

			auto it = bindings.find(attr.binding);
			if (it == bindings.end()) {
				// Insert new binding
				bindings.insert({ attr.binding, VertexAttributeBinding(attr.stride, attr.frequency) });
			}
			else {
				// Validate existing binding
				const VertexAttributeBinding* existing = &it->second;

				ERR_FAIL_COND_V_MSG(
					existing->stride != attr.stride,
					INVALID_ID,
					std::format("Vertex attributes with binding ({}) have an inconsistent stride.", attr.binding)
				);

				ERR_FAIL_COND_V_MSG(
					existing->frequency != attr.frequency,
					INVALID_ID,
					std::format("Vertex attributes with binding ({}) have an inconsistent frequency.", attr.binding)
				);
			}

			used_locations.insert(attr.location);
		}

		RDD::VertexFormatID driver_id = driver->vertex_format_create(vertex_descriptions, bindings);
		ERR_FAIL_COND_V(!driver_id, 0);

		VertexFormatID id = (vertex_format_cache.size() | ((int64_t)ID_TYPE_VERTEX_FORMAT << ID_BASE_SHIFT));
		vertex_format_cache[key] = id;
		auto [it, inserted] = vertex_formats.try_emplace(id);
		VertexDescriptionCache& ce = it->second;
		ce.vertex_formats = vertex_descriptions;
		ce.bindings = std::move(bindings);
		ce.driver_id = driver_id;
		return id;

	}


	RenderingDevice::RenderingDevice()
	{

	}

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
		// creates new vulkan device/driver
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

		return OK;
	}

	void RenderingDevice::finalize() {

	}

	uint32_t RenderingDevice::_get_swap_chain_desired_count() const {
		return 2;
	}


	RID RenderingDevice::render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/, const std::vector<PipelineSpecializationConstant>& p_specialization_constants /*= std::vector<PipelineSpecializationConstant>()*/)
	{
		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

		ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

		//FramebufferFormat fb_format;
		//{
		//	//_THREAD_SAFE_METHOD_

		//		if (p_framebuffer_format == INVALID_ID) {
		//			// If nothing provided, use an empty one (no attachments).
		//			p_framebuffer_format = framebuffer_format_create(Vector<AttachmentFormat>());
		//		}
		//	ERR_FAIL_COND_V(!framebuffer_formats.has(p_framebuffer_format), RID());
		//	fb_format = framebuffer_formats[p_framebuffer_format];
		//}
	}

	Error RenderingDevice::screen_create(DisplayServerEnums::WindowID p_screen)		// swap chain resize(also frame buffer creation)
	{

		RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
		ERR_FAIL_COND_V_MSG(surface == 0, ERR_CANT_CREATE, "A surface was not created for the screen.");

		auto [it, inserted] = screen_swap_chains.try_emplace(p_screen, RDD::SwapChainID());
		ERR_FAIL_COND_V_MSG(!inserted, ERR_CANT_CREATE, "A swap chain was already created for the screen.");

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

		screen_framebuffers.erase(p_screen);

		// If this frame has already queued this swap chain for presentation, we present it and remove it from the pending list.
		uint32_t to_present_index = 0;
		while (to_present_index < frames[frame].swap_chains_to_present.size()) {
			if (frames[frame].swap_chains_to_present[to_present_index] == it->second) {
				std::vector<RenderingDeviceDriver::SwapChainID> v = { it->second };
				driver->command_queue_execute_and_present(present_queue, {}, {}, {}, {}, v);
				frames[frame].swap_chains_to_present.erase(frames[frame].swap_chains_to_present.begin() + to_present_index);
			}
			else {
				to_present_index++;
			}
		}

		bool resize_required = false;
		RDD::FramebufferID framebuffer = driver->swap_chain_acquire_framebuffer(main_queue, it->second, resize_required);
		if (resize_required) {
			// Flush everything so nothing can be using the swap chain before resizing it.
			_flush_and_stall_for_all_frames();

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
	}


	void RenderingDevice::_begin_frame(bool p_presented /*= false*/)
	{

	}


	void RenderingDevice::_end_frame()
	{

	}


	void RenderingDevice::_execute_frame(bool p_present)
	{

	}


	void RenderingDevice::_flush_and_stall_for_all_frames(bool p_begin_frame /*= true*/)
	{

	}

	void RenderingDevice::swap_buffers(bool p_present)
	{
		_end_frame();

		_execute_frame(p_present);

		frame = (frame + 1) % frames.size();

		_begin_frame(true);
	}

}
