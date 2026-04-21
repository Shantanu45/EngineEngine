/*****************************************************************//**
 * \file   rendering_device.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "rendering_device.h"
#include "vulkan/shader_container.h"
#include "application/service_locator.h"
#include <set>
#include "math\helpers.h"
 //TODO: add abstraction for imgui
#include "vulkan/imgui_vulkan_device.h"
#include "util/timer.h"
#include "cache.h"


namespace Rendering
{
	static const char* SHADER_UNIFORM_NAMES[RenderingDevice::UNIFORM_TYPE_MAX] = {
	"Sampler",
	"CombinedSampler", // UNIFORM_TYPE_SAMPLER_WITH_TEXTURE
	"Texture",
	"Image",
	"TextureBuffer",
	"SamplerTextureBuffer",
	"ImageBuffer",
	"UniformBuffer",
	"StorageBuffer",
	"InputAttachment",
	"UniformBufferDynamic",
	"StorageBufferDynamic",
	};

	static _ALWAYS_INLINE_ void _copy_region(uint8_t const* __restrict p_src, uint8_t* __restrict p_dst, uint32_t p_src_x, uint32_t p_src_y, uint32_t p_src_w, uint32_t p_src_h, uint32_t p_src_full_w, uint32_t p_dst_pitch, uint32_t p_unit_size) {
		uint32_t src_offset = (p_src_y * p_src_full_w + p_src_x) * p_unit_size;
		uint32_t dst_offset = 0;
		for (uint32_t y = p_src_h; y > 0; y--) {
			uint8_t const* __restrict src = p_src + src_offset;
			uint8_t* __restrict dst = p_dst + dst_offset;
			for (uint32_t x = p_src_w * p_unit_size; x > 0; x--) {
				*dst = *src;
				src++;
				dst++;
			}
			src_offset += p_src_full_w * p_unit_size;
			dst_offset += p_dst_pitch;
		}
	}

	static _ALWAYS_INLINE_ void _copy_region_block_or_regular(const uint8_t* p_read_ptr, uint8_t* p_write_ptr, uint32_t p_x, uint32_t p_y, uint32_t p_width, uint32_t p_region_w, uint32_t p_region_h, uint32_t p_block_w, uint32_t p_block_h, uint32_t p_dst_pitch, uint32_t p_pixel_size, uint32_t p_block_size) {
		if (p_block_w != 1 || p_block_h != 1) {
			// Block format.
			uint32_t xb = p_x / p_block_w;
			uint32_t yb = p_y / p_block_h;
			uint32_t wb = p_width / p_block_w;
			uint32_t region_wb = p_region_w / p_block_w;
			uint32_t region_hb = p_region_h / p_block_h;
			_copy_region(p_read_ptr, p_write_ptr, xb, yb, region_wb, region_hb, wb, p_dst_pitch, p_block_size);
		}
		else {
			// Regular format.
			_copy_region(p_read_ptr, p_write_ptr, p_x, p_y, p_region_w, p_region_h, p_width, p_dst_pitch, p_pixel_size);
		}
	}

	uint32_t greatest_common_denominator(uint32_t a, uint32_t b) {
		// Euclidean algorithm.
		uint32_t t;
		while (b != 0) {
			t = b;
			b = a % b;
			a = t;
		}

		return a;
	}

	uint32_t least_common_multiple(uint32_t a, uint32_t b) {
		if (a == 0 || b == 0) {
			return 0;
		}

		return (a / greatest_common_denominator(a, b)) * b;
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
		
		// Create the transfer queue.
		transfer_queue_family = driver->command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
		if (!transfer_queue_family) {
			// Use main queue family if transfer queue family is not supported.
			transfer_queue_family = main_queue_family;
		}

		// Create the transfer queue.
		transfer_queue = driver->command_queue_create(transfer_queue_family);
		ERR_FAIL_COND_V(!transfer_queue, FAILED);

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

		// Use the processor count as the max amount of transfer workers that can be created.
		transfer_worker_pool_max_size = 1;//TODO: OS::get_singleton()->get_processor_count();

		// Pre-allocate to avoid locking a mutex when indexing into them.
		transfer_worker_pool.resize(transfer_worker_pool_max_size);
		transfer_worker_operation_used_by_draw.resize(transfer_worker_pool_max_size);

		frames.resize(frame_count);

		max_timestamp_query_elements = 256;

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

			frames[i].timestamp_pool = driver->timestamp_query_pool_create(max_timestamp_query_elements);
			frames[i].timestamp_names.resize(max_timestamp_query_elements);
			frames[i].timestamp_cpu_values.resize(max_timestamp_query_elements);
			frames[i].timestamp_count = 0;
			frames[i].timestamp_result_names.resize(max_timestamp_query_elements);
			frames[i].timestamp_cpu_result_values.resize(max_timestamp_query_elements);
			frames[i].timestamp_result_values.resize(max_timestamp_query_elements);
			frames[i].timestamp_result_count = 0;

			// Create the semaphores for the transfer workers.
			frames[i].transfer_worker_semaphores.resize(transfer_worker_pool_max_size);
			for (uint32_t j = 0; j < transfer_worker_pool_max_size; j++) {
				frames[i].transfer_worker_semaphores[j] = driver->semaphore_create();
				if (!frames[i].transfer_worker_semaphores[j]) {
					frame_failed = true;
					break;
				}
			}

			frames[i].command_buffer_pool.pool = frames[i].command_pool;
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
				if (frames[i].timestamp_pool) {
					driver->timestamp_query_pool_free(frames[i].timestamp_pool);
				}
				for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
					if (frames[i].transfer_worker_semaphores[j]) {
						driver->semaphore_free(frames[i].transfer_worker_semaphores[j]);
					}
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

		frames_drawn = frames.size();
		frames_drawn++;
		for (uint32_t i = 0; i < frames.size(); i++) {
			driver->command_buffer_begin(frames[i].command_buffer);

			// Reset all queries in a query pool before doing any operations with them..
			driver->command_timestamp_query_pool_reset(frames[i].command_buffer, frames[i].timestamp_pool, max_timestamp_query_elements);
			driver->command_buffer_end(frames[i].command_buffer);

		}


		// Convert block size from KB.
		//upload_staging_buffers.block_size = GLOBAL_GET("rendering/rendering_device/staging_buffer/block_size_kb");
		upload_staging_buffers.block_size = MAX(4u, upload_staging_buffers.block_size);
		upload_staging_buffers.block_size *= 1024;

		// Convert staging buffer size from MB.
		//upload_staging_buffers.max_size = GLOBAL_GET("rendering/rendering_device/staging_buffer/max_size_mb");
		upload_staging_buffers.max_size = MAX(1u, upload_staging_buffers.max_size);
		upload_staging_buffers.max_size *= 1024 * 1024;
		upload_staging_buffers.max_size = MAX(upload_staging_buffers.max_size, upload_staging_buffers.block_size * 4);

		// Copy the sizes to the download staging buffers.
		download_staging_buffers.block_size = upload_staging_buffers.block_size;
		download_staging_buffers.max_size = upload_staging_buffers.max_size;

		//texture_upload_region_size_px = GLOBAL_GET("rendering/rendering_device/staging_buffer/texture_upload_region_size_px");
		texture_upload_region_size_px = math_helpers::nearest_power_of_two(texture_upload_region_size_px);

		//texture_download_region_size_px = GLOBAL_GET("rendering/rendering_device/staging_buffer/texture_download_region_size_px");
		texture_download_region_size_px = math_helpers::nearest_power_of_two(texture_download_region_size_px);

		// Ensure current staging block is valid and at least one per frame exists.
		upload_staging_buffers.current = 0;
		upload_staging_buffers.used = false;
		upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;

		download_staging_buffers.current = 0;
		download_staging_buffers.used = false;
		download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;

		for (uint32_t i = 0; i < frames.size(); i++) {
			// Staging was never used, create the blocks.
			err = _insert_staging_block(upload_staging_buffers);
			ERR_FAIL_COND_V(err, FAILED);

			err = _insert_staging_block(download_staging_buffers);
			ERR_FAIL_COND_V(err, FAILED);
			//download_staging_buffers.current++;
			//upload_staging_buffers.current++;
		}
		upload_staging_buffers.current = 0;

		download_staging_buffers.current = 0;

		fb_cache = std::make_unique<FramebufferCache>(this);
		tex_cache = std::make_unique<TransientTextureCache>(this);
		return OK;
	}

	void RenderingDevice::on_poll(void* e)
	{
		imgui_device->poll_event(e);
	}

	void RenderingDevice::finalize() {

		if (!frames.empty()) {
			// Wait for all frames to have finished rendering.
			_flush_and_stall_for_all_frames(false);
		}
		_submit_transfer_workers();
		_wait_for_transfer_workers();

		imgui_device->finalize();
		// Delete all shader modules in cache;
		for (auto s: shader_cache)
		{
			free_rid(s.second);
		}
		shader_cache.clear();

		for (auto& [state, rid] : sampler_cache)
			free_rid(rid);
		sampler_cache.clear();

		tex_cache->flush(this);
		fb_cache->clear();

		// Free all resources.
		_free_rids(render_pipeline_owner, "Pipeline");
		//_free_rids(compute_pipeline_owner, "Compute");
		_free_rids(uniform_set_owner, "UniformSet");
		_free_rids(texture_buffer_owner, "TextureBuffer");
		_free_rids(storage_buffer_owner, "StorageBuffer");
		//_free_rids(instances_buffer_owner, "InstancesBuffer");
		_free_rids(uniform_buffer_owner, "UniformBuffer");
		_free_rids(shader_owner, "Shader");
		_free_rids(index_array_owner, "IndexArray");
		_free_rids(index_buffer_owner, "IndexBuffer");
		_free_rids(vertex_array_owner, "VertexArray");
		_free_rids(vertex_buffer_owner, "VertexBuffer");
		_free_rids(framebuffer_owner, "Framebuffer");
		_free_rids(sampler_owner, "Sampler");


		_free_transfer_workers();

		// Free everything pending.
		for (uint32_t i = 0; i < frames.size(); i++) {
			int f = (frame + i) % frames.size();
			_free_pending_resources(f);
			driver->command_pool_free(frames[i].command_pool);
			driver->timestamp_query_pool_free(frames[i].timestamp_pool);
			driver->semaphore_free(frames[i].semaphore);
			driver->fence_free(frames[i].fence);

			CommandBufferPool& buffer_pool = frames[i].command_buffer_pool;
			for (uint32_t j = 0; j < buffer_pool.buffers.size(); j++) {
				driver->semaphore_free(buffer_pool.semaphores[j]);
			}

			for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
				driver->semaphore_free(frames[i].transfer_worker_semaphores[j]);
			}
		}

		if (pipeline_cache_enabled) {
			update_pipeline_cache(true);
			driver->pipeline_cache_free();
		}

		frames.clear();

		for (int i = 0; i < upload_staging_buffers.blocks.size(); i++) {
			driver->buffer_unmap(upload_staging_buffers.blocks[i].driver_id);
			driver->buffer_free(upload_staging_buffers.blocks[i].driver_id);
		}

		for (int i = 0; i < download_staging_buffers.blocks.size(); i++) {
			driver->buffer_unmap(download_staging_buffers.blocks[i].driver_id);
			driver->buffer_free(download_staging_buffers.blocks[i].driver_id);
		}

		while (vertex_formats.size()) {
			std::unordered_map<VertexFormatID, VertexDescriptionCache>::iterator temp = vertex_formats.begin();
			driver->vertex_format_free(temp->second.driver_id);
			vertex_formats.erase(temp);
		}

		for (std::pair<FramebufferFormatID, FramebufferFormat> E : framebuffer_formats) {
			driver->render_pass_free(E.second.render_pass);
		}
		framebuffer_formats.clear();

		// Delete the swap chains created for the screens.
		for (const std::pair<DisplayServerEnums::WindowID, RDD::SwapChainID> it : screen_swap_chains) {
			driver->swap_chain_free(it.second);
		}

		screen_swap_chains.clear();

		// Delete the command queues.
		if (present_queue) {
			if (main_queue != present_queue) {
				// Only delete the present queue if it's unique.
				driver->command_queue_free(present_queue);
			}

			present_queue = RDD::CommandQueueID();
		}

		if (transfer_queue) {
			if (main_queue != transfer_queue) {
				// Only delete the transfer queue if it's unique.
				driver->command_queue_free(transfer_queue);
			}

			transfer_queue = RDD::CommandQueueID();
		}

		if (main_queue) {
			driver->command_queue_free(main_queue);
			main_queue = RDD::CommandQueueID();
		}

		// Delete the driver once everything else has been deleted.
		if (driver != nullptr) {
			context->driver_free(driver);
			driver = nullptr;
		}

		// All these should be clear at this point.
		//ERR_FAIL_COND(dependency_map.size());
		//ERR_FAIL_COND(reverse_dependency_map.size());
	}

#pragma region Shader

	RID RenderingDevice::create_program(const std::string& p_shader_name, const std::vector<std::string> programs)
	{
		auto hash = hash_xxhash_strings_32(programs);
		if (shader_cache.contains(hash))
		{
			return shader_cache[hash];
		}
		else
		{
			RDShaderSource* shaders = new RDShaderSource();
			shaders->set_language(RenderingDeviceCommons::SHADER_LANGUAGE_GLSL);
			for (auto shader_path : programs)
			{
				auto stage = shader_stage_from_compiler_stage(Compiler::stage_from_path(shader_path));
				ERR_FAIL_COND_V_MSG(stage == RenderingDeviceCommons::SHADER_STAGE_MAX, RID(), "could not evaluate shader stage from path!!");
				shaders->set_stage_source(stage, shader_path);
			}
			auto shader_rid =  shader_create_from_spirv(shader_compile_spirv_from_shader_source(shaders), p_shader_name);

			shader_cache[hash] = shader_rid;
			return shader_rid;
		}
	}

	Rendering::RDShaderSPIRV* RenderingDevice::shader_compile_spirv_from_shader_source(const RDShaderSource* p_source, bool p_allow_cache /*= true*/)
	{
		//ERR_FAIL_COND_V(p_source == nullptr, &RDShaderSPIRV());

		RDShaderSPIRV* bytecode = new RDShaderSPIRV;
		for (int i = 0; i < RenderingDeviceCommons::SHADER_STAGE_MAX; i++) {
			std::string error;

			ShaderStage stage = ShaderStage(i);
			std::string source = p_source->get_stage_source(stage);

			if (!source.empty()) {
				std::vector<uint8_t> spirv = shader_compile_spirv_from_source_file(stage, source, p_source->get_language(), &error, p_allow_cache);
				bytecode->set_stage_bytecode(stage, spirv);
				bytecode->set_stage_compile_error(stage, error);
			}
		}
		return bytecode;
	}

	RID RenderingDevice::shader_create_from_spirv(const RDShaderSPIRV* p_spirv, const std::string& p_shader_name /*= ""*/)
	{
		ERR_FAIL_COND_V(p_spirv == nullptr, RID());

		std::vector<ShaderStageSPIRVData> stage_data;
		for (int i = 0; i < RenderingDeviceCommons::SHADER_STAGE_MAX; i++) {
			ShaderStage stage = ShaderStage(i);
			ShaderStageSPIRVData sd;
			sd.shader_stage = stage;
			std::string error = p_spirv->get_stage_compile_error(stage);
			ERR_FAIL_COND_V_MSG(!error.empty(), RID(), std::format("Can't create a shader from an errored bytecode: \n Stage: {} \n Error: {}. Check errors in source bytecode.", (int)stage, error));
			sd.spirv = p_spirv->get_stage_bytecode(stage);
			if (sd.spirv.empty()) {
				continue;
			}
			stage_data.push_back(sd);
		}

		const RenderingShaderContainerFormat& container_format = driver->get_shader_container_format();
		RenderingShaderContainer* shader_container = container_format.create_container();
		//ERR_FAIL_COND_V(shader_container == nullptr, std::vector<uint8_t>());
		bool code_compiled = shader_container->set_code_from_spirv(p_shader_name, stage_data);
		//ERR_FAIL_COND_V_MSG(!code_compiled, std::vector<uint8_t>(), std::format("Failed to compile code to native for SPIR-V."));
		std::vector<PipelineImmutableSampler> immutable_samplers;
		auto id = shader_create_from_container_with_samplers(shader_container, RID(), immutable_samplers);
		shader_name_rid_map[p_shader_name] = id;
		return id;//_shader_create_from_spirv(stage_data);
	}

	std::vector<uint8_t> RenderingDevice::shader_compile_spirv_from_source_file(ShaderStage p_stage, const std::string& p_source_code_file, ShaderLanguage p_language /*= SHADER_LANGUAGE_GLSL*/, std::string* r_error /*= nullptr*/, bool p_allow_cache /*= true*/)
	{
		switch (p_language) {
			//#ifdef MODULE_GLSLANG_ENABLED
		case ShaderLanguage::SHADER_LANGUAGE_GLSL: {
			//ShaderLanguageVersion language_version = driver->get_shader_container_format().get_shader_language_version();
			//ShaderSpirvVersion spirv_version = driver->get_shader_container_format().get_shader_spirv_version();
			compiler->set_source_from_file(p_source_code_file, compiler_stage_from_shader_stage(p_stage));
			compiler->preprocess();
			std::vector<uint32_t> spirv_compiled = compiler->compile(*r_error, {});
			std::vector<uint8_t> bytes_spirv(spirv_compiled.size() * sizeof(uint32_t));
			std::memcpy(bytes_spirv.data(), spirv_compiled.data(), bytes_spirv.size());
			return bytes_spirv;
		}
												 //#endif
		default:
			ERR_FAIL_V_MSG(std::vector<uint8_t>(), "Shader language is not supported.");
		}
	}

	RID RenderingDevice::shader_create_from_container_with_samplers(RenderingShaderContainer* shader_container, RID p_placeholder, const std::vector<PipelineImmutableSampler>& p_immutable_samplers)
	{
		//RenderingShaderContainer* shader_container = driver->get_shader_container_format().create_container();
		ERR_FAIL_COND_V(shader_container == nullptr, RID());

		//bool parsed_container = shader_container->from_shader_stage_spirv_data(p_shader);
		//ERR_FAIL_COND_V_MSG(!parsed_container, RID(), "Failed to parse shader container from binary.");

		std::vector<RDD::ImmutableSampler> driver_immutable_samplers;
		for (const PipelineImmutableSampler& source_sampler : p_immutable_samplers) {
			RDD::ImmutableSampler driver_sampler;
			driver_sampler.type = source_sampler.uniform_type;
			driver_sampler.binding = source_sampler.binding;

			for (uint32_t j = 0; j < source_sampler.get_id_count(); j++) {
				RDD::SamplerID* sampler_driver_id = sampler_owner.get_or_null(source_sampler.get_id(j));
				driver_sampler.ids.push_back(*sampler_driver_id);
			}

			driver_immutable_samplers.push_back(driver_sampler);
		}

		RDD::ShaderID shader_id = driver->shader_create_from_container(shader_container, driver_immutable_samplers);
		ERR_FAIL_COND_V(!shader_id, RID());

		// All good, let's create modules.

		RID id;
		if (p_placeholder.is_null()) {
			id = shader_owner.make_rid();
		}
		else {
			id = p_placeholder;
		}

		Shader* shader = shader_owner.get_or_null(id);
		ERR_FAIL_NULL_V(shader, RID());

		*((ShaderReflection*)shader) = shader_container->get_shader_reflection();
		shader->name.clear();
		shader->name.append(shader_container->shader_name);
		shader->driver_id = shader_id;
		shader->layout_hash = driver->shader_get_layout_hash(shader_id);

		for (int i = 0; i < shader->uniform_sets.size(); i++) {
			uint32_t format = 0; // No format, default.

			if (shader->uniform_sets[i].size()) {
				// Sort and hash.

				std::sort(shader->uniform_sets[i].begin(), shader->uniform_sets[i].end());

				UniformSetFormat usformat;
				usformat.uniforms = shader->uniform_sets[i];
				std::map<UniformSetFormat, uint32_t>::iterator i = uniform_set_format_cache.find(usformat);
				if (i != uniform_set_format_cache.end()) {
					format = i->second;
				}
				else {
					format = uniform_set_format_cache.size() + 1;
					uniform_set_format_cache.emplace(usformat, format);
				}
			}

			shader->set_formats.push_back(format);
		}

		for (ShaderStage stage : shader->stages_vector) {
			switch (stage) {
			case SHADER_STAGE_VERTEX:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT);
				break;
			case SHADER_STAGE_FRAGMENT:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
				break;
			case SHADER_STAGE_TESSELATION_CONTROL:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
				break;
			case SHADER_STAGE_TESSELATION_EVALUATION:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
				break;
			case SHADER_STAGE_COMPUTE:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
				break;
			case SHADER_STAGE_GEOMETRY:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
				break;
			case SHADER_STAGE_RAYGEN:
			case SHADER_STAGE_ANY_HIT:
			case SHADER_STAGE_CLOSEST_HIT:
			case SHADER_STAGE_MISS:
			case SHADER_STAGE_INTERSECTION:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);
				break;
			default:
				DEV_ASSERT(false && "Unknown shader stage.");
				break;
			}
		}

#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID:{}", id.get_id()));
#endif
		return id;
	}

#pragma endregion

	RID RenderingDevice::render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format,
		VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, 
		const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, 
		const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state, 
		BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/, 
		const std::vector<PipelineSpecializationConstant>& p_specialization_constants /*= std::vector<PipelineSpecializationConstant>()*/)
	{
		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

		ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

		FramebufferFormat fb_format;
		{
			//_THREAD_SAFE_METHOD_

			if (p_framebuffer_format == INVALID_ID) {
				// If nothing provided, use an empty one (no attachments).
				p_framebuffer_format = framebuffer_format_create(std::vector<AttachmentFormat>());
			}
			ERR_FAIL_COND_V(!framebuffer_formats.contains(p_framebuffer_format), RID());
			fb_format = framebuffer_formats[p_framebuffer_format];
		}

		// Validate shader vs. framebuffer.
		{
			ERR_FAIL_COND_V_MSG(p_for_render_pass >= uint32_t(fb_format.E->first.passes.size()), RID(), std::format("Render pass requested for pipeline creation {} is out of bounds", std::to_string(p_for_render_pass)));
			const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
			uint32_t output_mask = 0;
			for (int i = 0; i < pass.color_attachments.size(); i++) {
				if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
					output_mask |= 1 << i;
				}
			}
			ERR_FAIL_COND_V_MSG(shader->fragment_output_mask != output_mask, RID(),
				std::format("Mismatch fragment shader output mask {} and framebuffer color output mask {} when binding both in render pipeline.", std::to_string(shader->fragment_output_mask), std::to_string(output_mask)));
		}

		RDD::VertexFormatID driver_vertex_format;
		if (p_vertex_format != INVALID_ID) {
			// Uses vertices, else it does not.
			ERR_FAIL_COND_V(!vertex_formats.contains(p_vertex_format), RID());
			const VertexDescriptionCache& vd = vertex_formats[p_vertex_format];
			driver_vertex_format = vertex_formats[p_vertex_format].driver_id;

			// Validate with inputs.
			for (uint32_t i = 0; i < 64; i++) {
				if (!(shader->vertex_input_mask & ((uint64_t)1) << i)) {
					continue;
				}
				bool found = false;
				for (int j = 0; j < vd.vertex_formats.size(); j++) {
					if (vd.vertex_formats[j].location == i) {
						found = true;
						break;
					}
				}

				ERR_FAIL_COND_V_MSG(!found, RID(),
					std::format("Shader vertex input location {} not provided in vertex input description for pipeline creation.", std::to_string(i)));
			}

		}
		else {
			ERR_FAIL_COND_V_MSG(shader->vertex_input_mask != 0, RID(),
				std::format("Shader contains vertex inputs, but no vertex input description was provided for pipeline creation."));
		}

		ERR_FAIL_INDEX_V(p_render_primitive, RENDER_PRIMITIVE_MAX, RID());

		ERR_FAIL_INDEX_V(p_rasterization_state.cull_mode, 3, RID());

		if (p_multisample_state.sample_mask.size()) {
			// Use sample mask.
			ERR_FAIL_COND_V((int)TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count] != p_multisample_state.sample_mask.size(), RID());
		}

		ERR_FAIL_INDEX_V(p_depth_stencil_state.depth_compare_operator, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_blend_state.logic_op, LOGIC_OP_MAX, RID());

		const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
		ERR_FAIL_COND_V(p_blend_state.attachments.size() < pass.color_attachments.size(), RID());
		for (int i = 0; i < pass.color_attachments.size(); i++) {
			if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_color_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_color_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].color_blend_op, BLEND_OP_MAX, RID());

				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].alpha_blend_op, BLEND_OP_MAX, RID());
			}
		}

		for (int i = 0; i < shader->specialization_constants.size(); i++) {
			const ShaderSpecializationConstant& sc = shader->specialization_constants[i];
			for (int j = 0; j < p_specialization_constants.size(); j++) {
				const PipelineSpecializationConstant& psc = p_specialization_constants[j];
				if (psc.constant_id == sc.constant_id) {
					ERR_FAIL_COND_V_MSG(psc.type != sc.type, RID(), std::format("Specialization constant provided for id {} is of the wrong type.", std::to_string(sc.constant_id)));
					break;
				}
			}
		}
		std::vector<int32_t> color_attachments = pass.color_attachments;
		std::vector<PipelineSpecializationConstant> specialization_constants = p_specialization_constants;
		RenderPipeline pipeline;
		pipeline.driver_id = driver->render_pipeline_create(
			shader->driver_id,
			driver_vertex_format,
			p_render_primitive,
			p_rasterization_state,
			p_multisample_state,
			p_depth_stencil_state,
			p_blend_state,
			color_attachments,
			p_dynamic_state_flags,
			fb_format.render_pass,
			p_for_render_pass,
			specialization_constants);
		ERR_FAIL_COND_V(!pipeline.driver_id, RID());

		pipeline.shader = p_shader;
		pipeline.shader_driver_id = shader->driver_id;
		pipeline.shader_layout_hash = shader->layout_hash;
		pipeline.set_formats = shader->set_formats;
		pipeline.push_constant_size = shader->push_constant_size;
		pipeline.stage_bits = shader->stage_bits;

#ifdef DEBUG_ENABLED
		pipeline.validation.dynamic_state = p_dynamic_state_flags;
		pipeline.validation.framebuffer_format = p_framebuffer_format;
		pipeline.validation.render_pass = p_for_render_pass;
		pipeline.validation.vertex_format = p_vertex_format;
		pipeline.validation.uses_restart_indices = p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX;

		static const uint32_t primitive_divisor[RENDER_PRIMITIVE_MAX] = {
			1, 2, 1, 1, 1, 3, 1, 1, 1, 1, 1
		};
		pipeline.validation.primitive_divisor = primitive_divisor[p_render_primitive];
		static const uint32_t primitive_minimum[RENDER_PRIMITIVE_MAX] = {
			1,
			2,
			2,
			2,
			2,
			3,
			3,
			3,
			3,
			3,
			1,
		};
		pipeline.validation.primitive_minimum = primitive_minimum[p_render_primitive];
#endif

		// Create ID to associate with this pipeline.
		RID id = render_pipeline_owner.make_rid(pipeline);
		{
			//_THREAD_SAFE_METHOD_

#ifdef DEV_ENABLED
				set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
			// Now add all the dependencies.
			_add_dependency(id, p_shader);
		}
		return id;
	}

	RID RenderingDevice::render_pipeline_create_from_frame_buffer(RID p_shader, RID p_framebuffer,
		VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive,
		const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state,
		const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state,
		BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/,
		const std::vector<PipelineSpecializationConstant>& p_specialization_constants /*= std::vector<PipelineSpecializationConstant>()*/)
	{
		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

		ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

		auto framebuff = framebuffer_owner.get_or_null(p_framebuffer);

		auto frame_buffer_format = framebuff->format_id;
		FramebufferFormat fb_format;
		{
			//_THREAD_SAFE_METHOD_

			if (frame_buffer_format == INVALID_ID) {
				// If nothing provided, use an empty one (no attachments).
				frame_buffer_format = framebuffer_format_create(std::vector<AttachmentFormat>());
			}
			ERR_FAIL_COND_V(!framebuffer_formats.contains(frame_buffer_format), RID());
			fb_format = framebuffer_formats[frame_buffer_format];
		}

		// Validate shader vs. framebuffer.
		{
			ERR_FAIL_COND_V_MSG(p_for_render_pass >= uint32_t(fb_format.E->first.passes.size()), RID(), std::format("Render pass requested for pipeline creation {} is out of bounds", std::to_string(p_for_render_pass)));
			const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
			uint32_t output_mask = 0;
			for (int i = 0; i < pass.color_attachments.size(); i++) {
				if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
					output_mask |= 1 << i;
				}
			}
			ERR_FAIL_COND_V_MSG(shader->fragment_output_mask != output_mask, RID(),
				std::format("Mismatch fragment shader output mask {} and framebuffer color output mask {} when binding both in render pipeline.", std::to_string(shader->fragment_output_mask), std::to_string(output_mask)));
		}

		RDD::VertexFormatID driver_vertex_format;
		if (p_vertex_format != INVALID_ID) {
			// Uses vertices, else it does not.
			ERR_FAIL_COND_V(!vertex_formats.contains(p_vertex_format), RID());
			const VertexDescriptionCache& vd = vertex_formats[p_vertex_format];
			driver_vertex_format = vertex_formats[p_vertex_format].driver_id;

			// Validate with inputs.
			for (uint32_t i = 0; i < 64; i++) {
				if (!(shader->vertex_input_mask & ((uint64_t)1) << i)) {
					continue;
				}
				bool found = false;
				for (int j = 0; j < vd.vertex_formats.size(); j++) {
					if (vd.vertex_formats[j].location == i) {
						found = true;
						break;
					}
				}

				ERR_FAIL_COND_V_MSG(!found, RID(),
					std::format("Shader vertex input location {} not provided in vertex input description for pipeline creation.", std::to_string(i)));
			}

		}
		else {
			ERR_FAIL_COND_V_MSG(shader->vertex_input_mask != 0, RID(),
				std::format("Shader contains vertex inputs, but no vertex input description was provided for pipeline creation."));
		}

		ERR_FAIL_INDEX_V(p_render_primitive, RENDER_PRIMITIVE_MAX, RID());

		ERR_FAIL_INDEX_V(p_rasterization_state.cull_mode, 3, RID());

		if (p_multisample_state.sample_mask.size()) {
			// Use sample mask.
			ERR_FAIL_COND_V((int)TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count] != p_multisample_state.sample_mask.size(), RID());
		}

		ERR_FAIL_INDEX_V(p_depth_stencil_state.depth_compare_operator, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_blend_state.logic_op, LOGIC_OP_MAX, RID());

		const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
		ERR_FAIL_COND_V(p_blend_state.attachments.size() < pass.color_attachments.size(), RID());
		for (int i = 0; i < pass.color_attachments.size(); i++) {
			if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_color_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_color_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].color_blend_op, BLEND_OP_MAX, RID());

				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
				ERR_FAIL_INDEX_V(p_blend_state.attachments[i].alpha_blend_op, BLEND_OP_MAX, RID());
			}
		}

		for (int i = 0; i < shader->specialization_constants.size(); i++) {
			const ShaderSpecializationConstant& sc = shader->specialization_constants[i];
			for (int j = 0; j < p_specialization_constants.size(); j++) {
				const PipelineSpecializationConstant& psc = p_specialization_constants[j];
				if (psc.constant_id == sc.constant_id) {
					ERR_FAIL_COND_V_MSG(psc.type != sc.type, RID(), std::format("Specialization constant provided for id {} is of the wrong type.", std::to_string(sc.constant_id)));
					break;
				}
			}
		}
		std::vector<int32_t> color_attachments = pass.color_attachments;
		std::vector<PipelineSpecializationConstant> specialization_constants = p_specialization_constants;
		RenderPipeline pipeline;
		pipeline.driver_id = driver->render_pipeline_create(
			shader->driver_id,
			driver_vertex_format,
			p_render_primitive,
			p_rasterization_state,
			p_multisample_state,
			p_depth_stencil_state,
			p_blend_state,
			color_attachments,
			p_dynamic_state_flags,
			fb_format.render_pass,
			p_for_render_pass,
			specialization_constants);
		ERR_FAIL_COND_V(!pipeline.driver_id, RID());

		pipeline.shader = p_shader;
		pipeline.shader_driver_id = shader->driver_id;
		pipeline.shader_layout_hash = shader->layout_hash;
		pipeline.set_formats = shader->set_formats;
		pipeline.push_constant_size = shader->push_constant_size;
		pipeline.stage_bits = shader->stage_bits;

#ifdef DEBUG_ENABLED
		pipeline.validation.dynamic_state = p_dynamic_state_flags;
		pipeline.validation.framebuffer_format = p_framebuffer_format;
		pipeline.validation.render_pass = p_for_render_pass;
		pipeline.validation.vertex_format = p_vertex_format;
		pipeline.validation.uses_restart_indices = p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX;

		static const uint32_t primitive_divisor[RENDER_PRIMITIVE_MAX] = {
			1, 2, 1, 1, 1, 3, 1, 1, 1, 1, 1
		};
		pipeline.validation.primitive_divisor = primitive_divisor[p_render_primitive];
		static const uint32_t primitive_minimum[RENDER_PRIMITIVE_MAX] = {
			1,
			2,
			2,
			2,
			2,
			3,
			3,
			3,
			3,
			3,
			1,
		};
		pipeline.validation.primitive_minimum = primitive_minimum[p_render_primitive];
#endif

		// Create ID to associate with this pipeline.
		RID id = render_pipeline_owner.make_rid(pipeline);
		{
			//_THREAD_SAFE_METHOD_

#ifdef DEV_ENABLED
			set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
			// Now add all the dependencies.
			_add_dependency(id, p_shader);
		}
		return id;
	}

	void RenderingDevice::update_pipeline_cache(bool p_closing /*= false*/)
	{

	}

	Error RenderingDevice::screen_create(DisplayServerEnums::WindowID p_screen)
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

	void RenderingDevice::on_resize(const DisplayServerEnums::WindowID p_window)
	{
		_flush_and_stall_for_all_frames();
		driver->swap_chain_resize(main_queue, screen_swap_chains[p_window], _get_swap_chain_desired_count());
		//context->surface_set_needs_resize(screen_swap_chains[p_window]., true);
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
				// TODO: fix: vkQueuePresentKHR(): pPresentInfo->pSwapchains[0] images passed to present must be in layout VK_IMAGE_LAYOUT_PRESENT_SRC duting this present
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
		return OK;
	}

	int RenderingDevice::screen_get_width(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/) const
	{
		RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
		ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
		return context->surface_get_width(surface);
	}

	int RenderingDevice::screen_get_height(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/) const
	{
		RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
		ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
		return context->surface_get_height(surface);
	}

	int RenderingDevice::screen_get_pre_rotation_degrees(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/) const
	{
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was not created for the screen.");

		return driver->swap_chain_get_pre_rotation_degrees(it->second);
	}

	Rendering::RenderingDevice::FramebufferFormatID RenderingDevice::screen_get_framebuffer_format(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/) const
	{
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), INVALID_ID, "Screen was never prepared.");

		DataFormat format = driver->swap_chain_get_format(it->second);
		ERR_FAIL_COND_V(format == DATA_FORMAT_MAX, INVALID_ID);

		AttachmentFormat attachment;
		attachment.format = format;
		attachment.samples = TEXTURE_SAMPLES_1;
		attachment.usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		std::vector<AttachmentFormat> screen_attachment;
		screen_attachment.push_back(attachment);
		return const_cast<RenderingDevice*>(this)->framebuffer_format_create(screen_attachment);
	}

	Rendering::RenderingDeviceCommons::ColorSpace RenderingDevice::screen_get_color_space(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/) const
	{
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), COLOR_SPACE_MAX, "Screen was never prepared.");

		ColorSpace color_space = driver->swap_chain_get_color_space(it->second);
		ERR_FAIL_COND_V_MSG(color_space == COLOR_SPACE_MAX, COLOR_SPACE_MAX, "Unknown color space.");
		return color_space;
	}

	Error RenderingDevice::screen_free(DisplayServerEnums::WindowID p_screen /*= DisplayServerEnums::MAIN_WINDOW_ID*/)
	{
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator it = screen_swap_chains.find(p_screen);
		ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), FAILED, "Screen was never created.");

		// Flush everything so nothing can be using the swap chain before erasing it.
		_flush_and_stall_for_all_frames(true);

		const DisplayServerEnums::WindowID screen = it->first;
		const RDD::SwapChainID swap_chain = it->second;
		driver->swap_chain_free(swap_chain);
		screen_framebuffers.erase(screen);
		screen_swap_chains.erase(screen);

		return OK;
	}

	Rendering::RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create(const std::vector<AttachmentFormat>& p_format,
		uint32_t p_view_count /*= 1*/, int32_t p_fragment_density_map_attachment /*= -1*/)
	{
		FramebufferPass pass;
		for (int i = 0; i < p_format.size(); i++) {
			if (p_format[i].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				pass.depth_attachment = i;
			}
			else if (p_format[i].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
				pass.depth_resolve_attachment = i;
			}
			else {
				pass.color_attachments.push_back(i);
			}
		}

		std::vector<FramebufferPass> passes;
		passes.push_back(pass);
		return framebuffer_format_create_multipass(p_format, passes, p_view_count, p_fragment_density_map_attachment);
	}

	Rendering::RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create_multipass(const std::vector<AttachmentFormat>& p_attachments,
		const std::vector<FramebufferPass>& p_passes, uint32_t p_view_count /*= 1*/, int32_t p_vrs_attachment /*= -1*/)
	{
		FramebufferFormatKey key;
		key.attachments = p_attachments;
		key.passes = p_passes;
		key.view_count = p_view_count;
		key.vrs_method = vrs_method;
		key.vrs_attachment = p_vrs_attachment;
		key.vrs_texel_size = vrs_texel_size;

		std::map<FramebufferFormatKey, FramebufferFormatID>::iterator it = framebuffer_format_cache.find(key);
		if (it != framebuffer_format_cache.end())
		{
			return it->second;
		}

		std::vector<TextureSamples> samples;
		std::vector<RDD::AttachmentLoadOp> load_ops;
		std::vector<RDD::AttachmentStoreOp> store_ops;
		for (int64_t i = 0; i < p_attachments.size(); i++) {
			load_ops.push_back(RDD::ATTACHMENT_LOAD_OP_CLEAR);
			store_ops.push_back(RDD::ATTACHMENT_STORE_OP_STORE);
		}

		RDD::RenderPassID render_pass = _render_pass_create(driver, p_attachments, p_passes, load_ops, store_ops, p_view_count, vrs_method, p_vrs_attachment, vrs_texel_size, &samples); // Actions don't matter for this use case.
		if (!render_pass) { // Was likely invalid.
			return INVALID_ID;
		}

		FramebufferFormatID id = FramebufferFormatID(framebuffer_format_cache.size()) | (FramebufferFormatID(ID_TYPE_FRAMEBUFFER_FORMAT) << FramebufferFormatID(ID_BASE_SHIFT));
		it = framebuffer_format_cache.insert({ key, id }).first;

		FramebufferFormat fb_format;
		fb_format.E = &(*it);
		fb_format.render_pass = render_pass;
		fb_format.pass_samples = samples;
		fb_format.view_count = p_view_count;
		framebuffer_formats[id] = fb_format;

#if PRINT_FRAMEBUFFER_FORMAT
		LOGI("FRAMEBUFFER FORMAT:", id, "ATTACHMENTS:", p_attachments.size(), "PASSES:", p_passes.size());
		for (RD::AttachmentFormat attachment : p_attachments) {
			LOGI("FORMAT:", attachment.format, "SAMPLES:", attachment.samples, "USAGE FLAGS:", attachment.usage_flags);
		}
#endif

		return id;
	}

	RenderingDevice::TextureSamples RenderingDevice::framebuffer_format_get_texture_samples(FramebufferFormatID p_format, uint32_t p_pass) {
		//_THREAD_SAFE_METHOD_

		auto E = framebuffer_formats.find(p_format);
		ERR_FAIL_COND_V(!(E == framebuffer_formats.end()), TEXTURE_SAMPLES_1);
		ERR_FAIL_COND_V(p_pass >= uint32_t(E->second.pass_samples.size()), TEXTURE_SAMPLES_1);

		return E->second.pass_samples[p_pass];
	}

	RID RenderingDevice::framebuffer_create_empty(const Size2i& p_size, TextureSamples p_samples, FramebufferFormatID p_format_check) {
		//_THREAD_SAFE_METHOD_

		Framebuffer framebuffer;
		framebuffer.format_id = framebuffer_format_create_empty(p_samples);
		ERR_FAIL_COND_V(p_format_check != INVALID_FORMAT_ID && framebuffer.format_id != p_format_check, RID());
		framebuffer.size = p_size;
		framebuffer.view_count = 1;

		//RDG::FramebufferCache* framebuffer_cache = RDG::framebuffer_cache_create();
		//framebuffer_cache->width = p_size.width;
		//framebuffer_cache->height = p_size.height;
		//framebuffer.framebuffer_cache = framebuffer_cache;

		RID id = framebuffer_owner.make_rid(framebuffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif

		// This relies on the fact that HashMap will not change the address of an object after it's been inserted into the container.
		//framebuffer_cache->render_pass_creation_user_data = (void*)(&framebuffer_formats[framebuffer.format_id].E->key());

		return id;
	}

	

	RID RenderingDevice::framebuffer_create(const std::vector<RID>& p_texture_attachments, FramebufferFormatID p_format_check, uint32_t p_view_count) {
		//_THREAD_SAFE_METHOD_

		FramebufferPass pass;

		for (int i = 0; i < p_texture_attachments.size(); i++) {
			Texture* texture = texture_owner.get_or_null(p_texture_attachments[i]);

			//ERR_FAIL_COND_V_MSG(texture && texture->layers != p_view_count, RID(), "Layers of our texture doesn't match view count for this framebuffer");
			ERR_FAIL_COND_V_MSG(texture && p_view_count > 1 && texture->layers != p_view_count, RID(), "Multiview: texture layers must match view count");
			if (texture != nullptr) {
				_check_transfer_worker_texture(texture);
			}

			if (texture && texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				pass.depth_attachment = i;
			}
			else if (texture && texture->usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
				pass.depth_resolve_attachment = i;
			}
			else if (texture && texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
				// Prevent the VRS attachment from being added to the color_attachments.
			}
			else {
				if (texture && texture->is_resolve_buffer) {
					pass.resolve_attachments.push_back(i);
				}
				else {
					pass.color_attachments.push_back(texture ? i : ATTACHMENT_UNUSED);
				}
			}
		}

		std::vector<FramebufferPass> passes;
		passes.push_back(pass);

		return framebuffer_create_multipass(p_texture_attachments, passes, p_format_check, p_view_count);
	}

	RID RenderingDevice::framebuffer_create_multipass(const std::vector<RID>& p_texture_attachments, const std::vector<FramebufferPass>& p_passes, FramebufferFormatID p_format_check, uint32_t p_view_count) {
		//_THREAD_SAFE_METHOD_
		uint32_t texture_layers = 1;

		std::vector<AttachmentFormat> attachments;
		std::vector<RDD::TextureID> textures;
		//std::vector<RDG::ResourceTracker*> trackers;
		int32_t vrs_attachment = -1;
		attachments.resize(p_texture_attachments.size());
		Size2i size;
		bool size_set = false;
		for (int i = 0; i < p_texture_attachments.size(); i++) {
			AttachmentFormat af;
			Texture* texture = texture_owner.get_or_null(p_texture_attachments[i]);
			if (!texture) {
				af.usage_flags = AttachmentFormat::UNUSED_ATTACHMENT;
				//trackers.push_back(nullptr);
			}
			else {
				//ERR_FAIL_COND_V_MSG(texture->layers != p_view_count, RID(), "Layers of our texture doesn't match view count for this framebuffer");

				if (p_view_count > 1) {
					ERR_FAIL_COND_V_MSG(texture->layers != p_view_count, RID(), "Multiview: texture layers must match view count");
				}
				else {
					texture_layers = texture->layers; // capture real layer count
				}


				_check_transfer_worker_texture(texture);

				if (i != 0 && texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
					// Detect if the texture is the fragment density map and it's not the first attachment.
					vrs_attachment = i;
				}

				if (!size_set) {
					size.x = texture->width;
					size.y = texture->height;
					size_set = true;
				}
				else if (texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
					// If this is not the first attachment we assume this is used as the VRS attachment.
					// In this case this texture will be 1/16th the size of the color attachment.
					// So we skip the size check.
				}
				else {
					ERR_FAIL_COND_V_MSG((uint32_t)size.x != texture->width || (uint32_t)size.y != texture->height, RID(),
						"All textures in a framebuffer should be the same size.");
				}

				af.format = texture->format;
				af.samples = texture->samples;
				af.usage_flags = texture->usage_flags;

				//_texture_make_mutable(texture, p_texture_attachments[i]);

				textures.push_back(texture->driver_id);
				//trackers.push_back(texture->draw_tracker);
			}
			attachments[i] = af;
		}

		ERR_FAIL_COND_V_MSG(!size_set, RID(), "All attachments unused.");

		FramebufferFormatID format_id = framebuffer_format_create_multipass(attachments, p_passes, p_view_count, vrs_attachment);
		if (format_id == INVALID_ID) {
			return RID();
		}

		ERR_FAIL_COND_V_MSG(p_format_check != INVALID_ID && format_id != p_format_check, RID(),
			"The format used to check this framebuffer differs from the intended framebuffer format.");

		Framebuffer framebuffer;
		framebuffer.format_id = format_id;
		framebuffer.texture_ids = p_texture_attachments;
		framebuffer.size = size;
		framebuffer.view_count = p_view_count;

		//RDG::FramebufferCache* framebuffer_cache = RDG::framebuffer_cache_create();
		//framebuffer_cache->width = size.width;
		//framebuffer_cache->height = size.height;
		//framebuffer_cache->textures = textures;
		//framebuffer_cache->trackers = trackers;
		//framebuffer.framebuffer_cache = framebuffer_cache;

		FramebufferKey key{
		.render_pass = framebuffer_formats[format_id].render_pass,
		.width = size.x,
		.height = size.y,
		.layers = texture_layers,			// TODO: use layers properly, or some other property
		.attachments = p_texture_attachments
		};

		RID id = framebuffer_owner.make_rid(framebuffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif

		for (int i = 0; i < p_texture_attachments.size(); i++) {
			if (p_texture_attachments[i].is_valid()) {
				_add_dependency(id, p_texture_attachments[i]);
			}
		}

		auto frame_buffer = fb_cache->get(key);// create_framebuffer_from_format_id(format_id, p_texture_attachments, size.x, size.y);
		rid_to_frame_buffer_id[id] = frame_buffer;
		// This relies on the fact that HashMap will not change the address of an object after it's been inserted into the container.
		//framebuffer_cache->render_pass_creation_user_data = (void*)(&framebuffer_formats[framebuffer.format_id].E->key());

		return id;
	}

	Rendering::RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create_empty(TextureSamples p_samples /*= TEXTURE_SAMPLES_1*/)
	{
		FramebufferFormatKey key;
		key.passes.push_back(FramebufferPass());

		std::map<FramebufferFormatKey, FramebufferFormatID>::iterator it = framebuffer_format_cache.find(key);
		if (it != framebuffer_format_cache.end())
		{
			return it->second;
		}

		std::vector<RDD::Subpass> subpass;
		subpass.resize(1);

		RDD::RenderPassID render_pass = driver->render_pass_create({}, subpass, {}, 1, RDD::AttachmentReference());
		ERR_FAIL_COND_V(!render_pass, FramebufferFormatID());

		FramebufferFormatID id = FramebufferFormatID(framebuffer_format_cache.size()) | (FramebufferFormatID(ID_TYPE_FRAMEBUFFER_FORMAT) << FramebufferFormatID(ID_BASE_SHIFT));

		it = framebuffer_format_cache.insert({ key, id }).first;

		FramebufferFormat fb_format;
		fb_format.E = &(*it);
		fb_format.render_pass = render_pass;
		fb_format.pass_samples.push_back(p_samples);
		framebuffer_formats[id] = fb_format;

#if PRINT_FRAMEBUFFER_FORMAT
		print_line("FRAMEBUFFER FORMAT:", id, "ATTACHMENTS: EMPTY");
#endif

		return id;
	}

	RID RenderingDevice::vertex_buffer_create(uint32_t p_size_bytes, std::span<uint8_t> p_data /*= {}*/, BitField<BufferCreationBits> p_creation_bits /*= 0*/)
	{
		ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != p_size_bytes, RID());

		Buffer buffer;
		buffer.size = p_size_bytes;
		buffer.usage = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_VERTEX_BIT;
		if (p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_STORAGE_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);

			// Persistent buffers expect frequent CPU -> GPU writes, so GPU writes should avoid the same path.
			buffer.usage.clear_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
		}
		buffer.driver_id = driver->buffer_create(buffer.size, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
		ERR_FAIL_COND_V(!buffer.driver_id, RID());

		// Vertex buffers are assumed to be immutable unless they don't have initial data or they've been marked for storage explicitly.
		if (p_data.empty() || p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT) || p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
			//buffer.draw_tracker = RDG::resource_tracker_create();
			//buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		}

		if (p_data.size()) {
			_buffer_initialize(&buffer, p_data);
		}

		//_THREAD_SAFE_LOCK_
			buffer_memory += buffer.size;
		//_THREAD_SAFE_UNLOCK_

			RID id = vertex_buffer_owner.make_rid(buffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		return id;
	}

	RID RenderingDevice::uniform_buffer_create(uint32_t p_size_bytes, std::span<uint8_t> p_data /*= {}*/, BitField<BufferCreationBits> p_creation_bits /*= 0*/)
	{
		ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != p_size_bytes, RID());

		Buffer buffer;
		buffer.size = p_size_bytes;
		buffer.usage = (RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_UNIFORM_BIT);
		if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
			buffer.usage.set_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);

			// This is a precaution: Persistent buffers are meant for frequent CPU -> GPU transfers.
			// Writing to this buffer from GPU might cause sync issues if both CPU & GPU try to write at the
			// same time. It's probably fine (since CPU always advances the pointer before writing) but let's
			// stick to the known/intended use cases and scream if we deviate from it.
			buffer.usage.clear_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT);
		}
		buffer.driver_id = driver->buffer_create(buffer.size, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
		ERR_FAIL_COND_V(!buffer.driver_id, RID());

		// Uniform buffers are assumed to be immutable unless they don't have initial data.
		//if (p_data.empty()) {
		//	buffer.draw_tracker = RDG::resource_tracker_create();
		//	buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		//}

		if (p_data.size()) {
			_buffer_initialize(&buffer, p_data);
		}

		//_THREAD_SAFE_LOCK_
			buffer_memory += buffer.size;
		//_THREAD_SAFE_UNLOCK_

		RID id = uniform_buffer_owner.make_rid(buffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		return id;
	}

	RID RenderingDevice::uniform_set_create(const std::span<Uniform>& p_uniforms, RID p_shader, uint32_t p_shader_set, bool p_linear_pool /*= false*/)
	{
		ERR_FAIL_COND_V(p_uniforms.size() == 0, RID());

		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());

		ERR_FAIL_COND_V_MSG(p_shader_set >= (uint32_t)shader->uniform_sets.size() || shader->uniform_sets[p_shader_set].empty(), RID(),
			std::format("Desired set ({}) not used by shader.", p_shader_set));
		// See that all sets in shader are satisfied.

		const std::vector<ShaderUniform>& set = shader->uniform_sets[p_shader_set];

		uint32_t uniform_count = p_uniforms.size();
		const Uniform* uniforms = p_uniforms.data();

		uint32_t set_uniform_count = set.size();
		const ShaderUniform* set_uniforms = set.data();

		std::vector<RDD::BoundUniform> driver_uniforms;
		driver_uniforms.resize(set_uniform_count);

		// Used for verification to make sure a uniform set does not use a framebuffer bound texture.
		//std::vector<UniformSet::AttachableTexture> attachable_textures;
		//std::vector<RDG::ResourceTracker*> draw_trackers;
		//std::vector<RDG::ResourceUsage> draw_trackers_usage;
		//std::unordered_map<RID, RDG::ResourceUsage> untracked_usage;
		//std::vector<UniformSet::SharedTexture> shared_textures_to_update;
		std::vector<RID> pending_clear_textures;

		for (uint32_t i = 0; i < set_uniform_count; i++) {
			const ShaderUniform& set_uniform = set_uniforms[i];
			int uniform_idx = -1;
			for (int j = 0; j < (int)uniform_count; j++) {
				if (uniforms[j].binding == set_uniform.binding) {
					uniform_idx = j;
					break;
				}
			}
			ERR_FAIL_COND_V_MSG(uniform_idx == -1, RID(),
				std::format("All the shader bindings for the given set must be covered by the uniforms provided. Binding ({}), set ({}) was not provided.", set_uniform.binding, p_shader_set));

			const Uniform& uniform = uniforms[uniform_idx];

			ERR_FAIL_INDEX_V(uniform.uniform_type, RenderingDeviceCommons::UNIFORM_TYPE_MAX, RID());
			ERR_FAIL_COND_V_MSG(uniform.uniform_type != set_uniform.type, RID(), std::format("Shader '{}' Mismatch uniform type for binding ({}), set ({}). Expected '{}', supplied: '{}'.", shader->name, set_uniform.binding, p_shader_set, SHADER_UNIFORM_NAMES[set_uniform.type], SHADER_UNIFORM_NAMES[uniform.uniform_type]));

			RDD::BoundUniform& driver_uniform = driver_uniforms[i];
			driver_uniform.type = uniform.uniform_type;
			driver_uniform.binding = uniform.binding;

			// Mark immutable samplers to be skipped when creating uniform set.
			driver_uniform.immutable_sampler = uniform.immutable_sampler;

			switch (uniform.uniform_type) {
			case UNIFORM_TYPE_SAMPLER: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), std::format("Sampler (binding: {}) is an array of ({}) sampler elements, so it should be provided equal number of sampler IDs to satisfy it (IDs provided: {}).", uniform.binding, set_uniform.length, uniform.get_id_count()));
					}
					else {
						ERR_FAIL_V_MSG(RID(), std::format("Sampler (binding: {}) should provide one ID referencing a sampler (IDs provided: {}).", uniform.binding, uniform.get_id_count()));
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RDD::SamplerID* sampler_driver_id = sampler_owner.get_or_null(uniform.get_id(j));
					ERR_FAIL_NULL_V_MSG(sampler_driver_id, RID(), std::format("Sampler (binding: {}, index {}) is not a valid sampler.", uniform.binding, j));

					driver_uniform.ids.push_back(*sampler_driver_id);
				}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length * 2) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), std::format("SamplerTexture (binding: {}) is an array of ({}) sampler&texture elements, so it should provided twice the amount of IDs (sampler,texture pairs) to satisfy it (IDs provided: {}).", uniform.binding, set_uniform.length, uniform.get_id_count()));
					}
					else {
						ERR_FAIL_V_MSG(RID(), std::format("SamplerTexture (binding: {}) should provide two IDs referencing a sampler and then a texture (IDs provided: {}).", uniform.binding, uniform.get_id_count()));
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j += 2) {
					RDD::SamplerID* sampler_driver_id = sampler_owner.get_or_null(uniform.get_id(j + 0));
					ERR_FAIL_NULL_V_MSG(sampler_driver_id, RID(), std::format("SamplerBuffer (binding: {}, index {}) is not a valid sampler.", uniform.binding, j + 1));

					RID texture_id = uniform.get_id(j + 1);
					Texture* texture = texture_owner.get_or_null(texture_id);
					ERR_FAIL_NULL_V_MSG(texture, RID(), std::format("Texture (binding: {}, index {}) is not a valid texture.", uniform.binding, j));

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
						std::format("Texture (binding: {}, index {}) needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.", uniform.binding, j));

					if ((texture->usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))) {
						UniformSet::AttachableTexture attachable_texture;
						attachable_texture.bind = set_uniform.binding;
						attachable_texture.texture = texture->owner.is_valid() ? texture->owner : uniform.get_id(j + 1);
						attachable_textures.push_back(attachable_texture);
					}

					if (texture->pending_clear) {
						pending_clear_textures.push_back(texture_id);
					}

					RDD::TextureID driver_id = texture->driver_id;
					//RDG::ResourceTracker* tracker = texture->draw_tracker;
					if (texture->shared_fallback != nullptr && texture->shared_fallback->texture.id != 0) {
						driver_id = texture->shared_fallback->texture;
						//tracker = texture->shared_fallback->texture_tracker;
						shared_textures_to_update.push_back({ false, texture_id });
					}

					//if (tracker != nullptr) {
					//	draw_trackers.push_back(tracker);
					//	draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_SAMPLE);
					//}
					//else {
					//	untracked_usage[texture_id] = RDG::RESOURCE_USAGE_TEXTURE_SAMPLE;
					//}

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(*sampler_driver_id);
					driver_uniform.ids.push_back(driver_id);
					driver_uniform.is_depth = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
					_check_transfer_worker_texture(texture);
				}
			} break;
			case UNIFORM_TYPE_TEXTURE: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), std::format("Texture (binding: {}) is an array of ({}) textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: {}).", uniform.binding, set_uniform.length, uniform.get_id_count()));
					}
					else {
						ERR_FAIL_V_MSG(RID(), std::format("Texture (binding: {}) should provide one ID referencing a texture (IDs provided: {}).", uniform.binding, uniform.get_id_count()));
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID texture_id = uniform.get_id(j);
					Texture* texture = texture_owner.get_or_null(texture_id);
					ERR_FAIL_NULL_V_MSG(texture, RID(), std::format("Texture (binding: {}, index {}) is not a valid texture.", uniform.binding, uniform.binding), uniform.binding, j);

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
						std::format("Texture (binding: {}, index {}) needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.", uniform.binding, j));

					if ((texture->usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))) {
						UniformSet::AttachableTexture attachable_texture;
						attachable_texture.bind = set_uniform.binding;
						attachable_texture.texture = texture->owner.is_valid() ? texture->owner : uniform.get_id(j);
						attachable_textures.push_back(attachable_texture);
					}

					if (texture->pending_clear) {
						pending_clear_textures.push_back(texture_id);
					}

					RDD::TextureID driver_id = texture->driver_id;
					//RDG::ResourceTracker* tracker = texture->draw_tracker;
					if (texture->shared_fallback != nullptr && texture->shared_fallback->texture.id != 0) {
						driver_id = texture->shared_fallback->texture;
						//tracker = texture->shared_fallback->texture_tracker;
						shared_textures_to_update.push_back({ false, texture_id });
					}

					//if (tracker != nullptr) {
					//	draw_trackers.push_back(tracker);
					//	draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_SAMPLE);
					//}
					//else {
					//	untracked_usage[texture_id] = RDG::RESOURCE_USAGE_TEXTURE_SAMPLE;
					//}

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(driver_id);
					driver_uniform.is_depth = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
					_check_transfer_worker_texture(texture);
				}
			} break;
			case UNIFORM_TYPE_IMAGE: {
				//if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
				//	if (set_uniform.length > 1) {
				//		ERR_FAIL_V_MSG(RID(), "Image (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
				//	}
				//	else {
				//		ERR_FAIL_V_MSG(RID(), "Image (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
				//	}
				//}

				//for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
				//	RID texture_id = uniform.get_id(j);
				//	Texture* texture = texture_owner.get_or_null(texture_id);

				//	ERR_FAIL_NULL_V_MSG(texture, RID(),
				//		"Image (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

				//	ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_STORAGE_BIT), RID(),
				//		"Image (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_STORAGE_BIT usage flag set in order to be used as uniform.");

				//	if (texture->owner.is_null() && texture->shared_fallback != nullptr) {
				//		shared_textures_to_update.push_back({ true, texture_id });
				//	}

				//	if (texture->pending_clear) {
				//		pending_clear_textures.push_back(texture_id);
				//	}

				//	if (_texture_make_mutable(texture, texture_id)) {
				//		// The texture must be mutable as a layout transition will be required.
				//		draw_graph.add_synchronization();
				//	}

				//	if (texture->draw_tracker != nullptr) {
				//		draw_trackers.push_back(texture->draw_tracker);

				//		if (set_uniform.writable) {
				//			draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE);
				//		}
				//		else {
				//			draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ);
				//		}
				//	}

				//	DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

				//	driver_uniform.ids.push_back(texture->driver_id);
				//	_check_transfer_worker_texture(texture);
				//}
			} break;
			case UNIFORM_TYPE_TEXTURE_BUFFER: {
				//if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
				//	if (set_uniform.length > 1) {
				//		ERR_FAIL_V_MSG(RID(), "Buffer (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") texture buffer elements, so it should be provided equal number of texture buffer IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
				//	}
				//	else {
				//		ERR_FAIL_V_MSG(RID(), "Buffer (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture buffer (IDs provided: " + itos(uniform.get_id_count()) + ").");
				//	}
				//}

				//for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
				//	RID buffer_id = uniform.get_id(j);
				//	Buffer* buffer = texture_buffer_owner.get_or_null(buffer_id);
				//	ERR_FAIL_NULL_V_MSG(buffer, RID(), "Texture Buffer (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture buffer.");

				//	if (set_uniform.writable && _buffer_make_mutable(buffer, buffer_id)) {
				//		// The buffer must be mutable if it's used for writing.
				//		draw_graph.add_synchronization();
				//	}

				//	if (buffer->draw_tracker != nullptr) {
				//		draw_trackers.push_back(buffer->draw_tracker);

				//		if (set_uniform.writable) {
				//			draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE);
				//		}
				//		else {
				//			draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ);
				//		}
				//	}
				//	else {
				//		untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ;
				//	}

				//	driver_uniform.ids.push_back(buffer->driver_id);
				//	_check_transfer_worker_buffer(buffer);
				//}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
				/*if (uniform.get_id_count() != (uint32_t)set_uniform.length * 2) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") sampler buffer elements, so it should provided twice the amount of IDs (sampler,buffer pairs) to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
					else {
						ERR_FAIL_V_MSG(RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ") should provide two IDs referencing a sampler and then a texture buffer (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j += 2) {
					RDD::SamplerID* sampler_driver_id = sampler_owner.get_or_null(uniform.get_id(j + 0));
					ERR_FAIL_NULL_V_MSG(sampler_driver_id, RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ", index " + itos(j + 1) + ") is not a valid sampler.");

					RID buffer_id = uniform.get_id(j + 1);
					Buffer* buffer = texture_buffer_owner.get_or_null(buffer_id);
					ERR_FAIL_NULL_V_MSG(buffer, RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ", index " + itos(j + 1) + ") is not a valid texture buffer.");

					if (buffer->draw_tracker != nullptr) {
						draw_trackers.push_back(buffer->draw_tracker);
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ);
					}
					else {
						untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ;
					}

					driver_uniform.ids.push_back(*sampler_driver_id);
					driver_uniform.ids.push_back(buffer->driver_id);
					_check_transfer_worker_buffer(buffer);
				}*/
			} break;
			case UNIFORM_TYPE_IMAGE_BUFFER: {
				// Todo.
			} break;
			case UNIFORM_TYPE_UNIFORM_BUFFER:
			case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC: {
				ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
					std::format("Uniform buffer supplied (binding: {}) must provide one ID ({} provided).", uniform.binding, uniform.get_id_count()));

				RID buffer_id = uniform.get_id(0);
				Buffer* buffer = uniform_buffer_owner.get_or_null(buffer_id);
				ERR_FAIL_NULL_V_MSG(buffer, RID(), std::format("Uniform buffer supplied (binding: {}) is invalid.", uniform.binding));

				ERR_FAIL_COND_V_MSG(buffer->size < (uint32_t)set_uniform.length, RID(),
					std::format("Uniform buffer supplied (binding: {}) size ({}) is smaller than size of shader uniform: ({}).", uniform.binding, buffer->size, set_uniform.length));

				//if (buffer->draw_tracker != nullptr) {
				//	draw_trackers.push_back(buffer->draw_tracker);
				//	draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ);
				//}
				//else {
				//	untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ;
				//}

				driver_uniform.ids.push_back(buffer->driver_id);
				_check_transfer_worker_buffer(buffer);
			} break;
			case UNIFORM_TYPE_STORAGE_BUFFER:
			case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC: {
				//ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
				//	std::format("Storage buffer supplied (binding: {}) must provide one ID ({} provided).", uniform.binding, uniform.get_id_count()));

				//Buffer* buffer = nullptr;

				//RID buffer_id = uniform.get_id(0);
				//if (storage_buffer_owner.owns(buffer_id)) {
				//	buffer = storage_buffer_owner.get_or_null(buffer_id);
				//}
				//else if (vertex_buffer_owner.owns(buffer_id)) {
				//	buffer = vertex_buffer_owner.get_or_null(buffer_id);

				//	ERR_FAIL_COND_V_MSG(!(buffer->usage.has_flag(RDD::BUFFER_USAGE_STORAGE_BIT)), RID(), std::format("Vertex buffer supplied (binding: {}) was not created with storage flag.", uniform.binding));
				//}
				//ERR_FAIL_NULL_V_MSG(buffer, RID(), std::format("Storage buffer supplied (binding: {}) is invalid.", uniform.binding));

				//// If 0, then it's sized on link time.
				//ERR_FAIL_COND_V_MSG(set_uniform.length > 0 && buffer->size != (uint32_t)set_uniform.length, RID(),
				//	std::format("Storage buffer supplied (binding: {}) size ({}) does not match size of shader uniform: ({}).", uniform.binding, buffer->size, set_uniform.length));

				//if (set_uniform.writable && _buffer_make_mutable(buffer, buffer_id)) {
					// The buffer must be mutable if it's used for writing.
					//draw_graph.add_synchronization();
				//}

				//if (buffer->draw_tracker != nullptr) {
				//	draw_trackers.push_back(buffer->draw_tracker);

				//	if (set_uniform.writable) {
				//		draw_trackers_usage.push_back(RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
				//	}
				//	else {
				//		draw_trackers_usage.push_back(RESOURCE_USAGE_STORAGE_BUFFER_READ);
				//	}
				//}
				//else {
				//	untracked_usage[buffer_id] = RESOURCE_USAGE_STORAGE_BUFFER_READ;
				//}

				//driver_uniform.ids.push_back(buffer->driver_id);
				//_check_transfer_worker_buffer(buffer);
			} break;
			case UNIFORM_TYPE_INPUT_ATTACHMENT: {
				/*ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") supplied for non-render shader (this is not allowed).");

				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
					else {
						ERR_FAIL_V_MSG(RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID texture_id = uniform.get_id(j);
					Texture* texture = texture_owner.get_or_null(texture_id);

					ERR_FAIL_NULL_V_MSG(texture, RID(),
						"InputAttachment (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
						"InputAttachment (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.");

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(texture->driver_id);
					_check_transfer_worker_texture(texture);
				}*/
			} break;
			case UNIFORM_TYPE_ACCELERATION_STRUCTURE: {
				//ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
				//	"Acceleration structure supplied (binding: " + itos(uniform.binding) + ") must provide one ID (" + itos(uniform.get_id_count()) + " provided).");

				//RID accel_id = uniform.get_id(0);
				//AccelerationStructure* accel = acceleration_structure_owner.get_or_null(accel_id);
				//ERR_FAIL_NULL_V_MSG(accel, RID(), "Acceleration Structure supplied (binding: " + itos(uniform.binding) + ") is invalid.");

				//if (accel->draw_tracker != nullptr) {
				//	draw_trackers.push_back(accel->draw_tracker);
				//	// Acceleration structure is never going to be writable from raytracing shaders
				//	draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_ACCELERATION_STRUCTURE_READ);
				//}

				//driver_uniform.ids.push_back(accel->driver_id);
			} break;
			default: {
			}
			}
		}

		RDD::UniformSetID driver_uniform_set = driver->uniform_set_create(driver_uniforms, shader->driver_id, p_shader_set, p_linear_pool ? frame : -1);
		ERR_FAIL_COND_V(!driver_uniform_set, RID());

		UniformSet uniform_set;
		uniform_set.driver_id = driver_uniform_set;
		uniform_set.format = shader->set_formats[p_shader_set];
		//uniform_set.attachable_textures = attachable_textures;
		//uniform_set.draw_trackers = draw_trackers;
		//uniform_set.draw_trackers_usage = draw_trackers_usage;
		//uniform_set.untracked_usage = untracked_usage;
		//uniform_set.shared_textures_to_update = shared_textures_to_update;
		uniform_set.pending_clear_textures = pending_clear_textures;
		uniform_set.shader_set = p_shader_set;
		uniform_set.shader_id = p_shader;

		RID id = uniform_set_owner.make_rid(uniform_set);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		// Add dependencies.
		_add_dependency(id, p_shader);
		for (uint32_t i = 0; i < uniform_count; i++) {
			const Uniform& uniform = uniforms[i];
			int id_count = uniform.get_id_count();
			for (int j = 0; j < id_count; j++) {
				_add_dependency(id, uniform.get_id(j));
			}
		}
		return id;
	}

	bool RenderingDevice::uniform_set_is_valid(RID p_uniform_set)
	{
		return uniform_set_owner.owns(p_uniform_set);
	}

	Error RenderingDevice::buffer_copy(RID p_src_buffer, RID p_dst_buffer, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size)
	{
		Buffer* src_buffer = _get_buffer_from_owner(p_src_buffer);
		if (!src_buffer) {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Source buffer argument is not a valid buffer of any type.");
		}

		Buffer* dst_buffer = _get_buffer_from_owner(p_dst_buffer);
		if (!dst_buffer) {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Destination buffer argument is not a valid buffer of any type.");
		}

		// Validate the copy's dimensions for both buffers.
		ERR_FAIL_COND_V_MSG((p_size + p_src_offset) > src_buffer->size, ERR_INVALID_PARAMETER, "Size is larger than the source buffer.");
		ERR_FAIL_COND_V_MSG((p_size + p_dst_offset) > dst_buffer->size, ERR_INVALID_PARAMETER, "Size is larger than the destination buffer.");

		_check_transfer_worker_buffer(src_buffer);
		_check_transfer_worker_buffer(dst_buffer);

		// Perform the copy.
		RDD::BufferCopyRegion region;
		region.src_offset = p_src_offset;
		region.dst_offset = p_dst_offset;
		region.size = p_size;

		//if (_buffer_make_mutable(dst_buffer, p_dst_buffer)) {
		//	// The destination buffer must be mutable to be used as a copy destination.
		//	draw_graph.add_synchronization();
		//}
		// TODO: re, not sure if I'm using the right transfer workker
		driver->command_copy_buffer(transfer_worker_pool[src_buffer->transfer_worker_index]->command_buffer, src_buffer->driver_id, dst_buffer->driver_id, { &region, 1 });

		return OK;
	}

	Error RenderingDevice::buffer_update(RID p_buffer, uint32_t p_offset, uint32_t p_size, const void* p_data, bool p_skip_check /*= false*/)
	{
		//ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

		copy_bytes_count += p_size;

		Buffer* buffer = _get_buffer_from_owner(p_buffer);
		ERR_FAIL_NULL_V_MSG(buffer, ERR_INVALID_PARAMETER, "Buffer argument is not a valid buffer of any type.");
		ERR_FAIL_COND_V_MSG(p_offset + p_size > buffer->size, ERR_INVALID_PARAMETER, std::format("Attempted to write buffer ({} bytes) past the end.", (p_offset + p_size) - buffer->size));

		if (buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			uint8_t* dst_data = driver->buffer_persistent_map_advance(buffer->driver_id, frames_drawn);

			memcpy(dst_data + p_offset, p_data, p_size);
			//direct_copy_count++;
			buffer_flush(p_buffer);
			return OK;
		}

		_check_transfer_worker_buffer(buffer);

		// Submitting may get chunked for various reasons, so convert this to a task.
		size_t to_submit = p_size;
		size_t submit_from = 0;

		thread_local std::vector<RecordedBufferCopy> command_buffer_copies_vector;
		command_buffer_copies_vector.clear();

		const uint8_t* src_data = reinterpret_cast<const uint8_t*>(p_data);
		const uint32_t required_align = 32;

		while (to_submit > 0) {
			uint32_t block_write_offset;
			uint32_t block_write_amount;
			StagingRequiredAction required_action;

			Error err = _staging_buffer_allocate(upload_staging_buffers, MIN(to_submit, upload_staging_buffers.block_size), required_align, block_write_offset, block_write_amount, required_action);
			if (err) {
				return err;
			}

			if (!command_buffer_copies_vector.empty() && required_action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL) {
				//if (_buffer_make_mutable(buffer, p_buffer)) {
				//	// The buffer must be mutable to be used as a copy destination.
				//	draw_graph.add_synchronization();
				//}

				//draw_graph.add_buffer_update(buffer->driver_id, buffer->draw_tracker, command_buffer_copies_vector);
				for (uint32_t j = 0; j < command_buffer_copies_vector.size(); j++)
				{
					driver->command_copy_buffer(transfer_worker_pool[buffer->transfer_worker_index]->command_buffer, command_buffer_copies_vector[j].source, buffer->driver_id, { &command_buffer_copies_vector[j].region, 1 });
				}
				command_buffer_copies_vector.clear();
			}

			_staging_buffer_execute_required_action(upload_staging_buffers, required_action);

			// Copy to staging buffer.
			memcpy(upload_staging_buffers.blocks[upload_staging_buffers.current].data_ptr + block_write_offset, src_data + submit_from, block_write_amount);

			// Insert a command to copy this.
			RDD::BufferCopyRegion region;
			region.src_offset = block_write_offset;
			region.dst_offset = submit_from + p_offset;
			region.size = block_write_amount;

			RecordedBufferCopy buffer_copy;
			buffer_copy.source = upload_staging_buffers.blocks[upload_staging_buffers.current].driver_id;
			buffer_copy.region = region;
			command_buffer_copies_vector.push_back(buffer_copy);

			upload_staging_buffers.blocks[upload_staging_buffers.current].fill_amount = block_write_offset + block_write_amount;

			to_submit -= block_write_amount;
			submit_from += block_write_amount;
		}

		if (!command_buffer_copies_vector.empty()) {
			//if (_buffer_make_mutable(buffer, p_buffer)) {
			//	// The buffer must be mutable to be used as a copy destination.
			//	draw_graph.add_synchronization();
			//}

			//draw_graph.add_buffer_update(buffer->driver_id, buffer->draw_tracker, command_buffer_copies_vector);
			for (uint32_t j = 0; j < command_buffer_copies_vector.size(); j++)
			{
				driver->command_copy_buffer(get_current_command_buffer()/*transfer_worker_pool[buffer->transfer_worker_index]->command_buffer*/, command_buffer_copies_vector[j].source, buffer->driver_id, { &command_buffer_copies_vector[j].region, 1 });
			}
		}

		gpu_copy_count++;
		return OK;
	}

	Error RenderingDevice::buffer_clear(RID p_buffer, uint32_t p_offset, uint32_t p_size)
	{
		Buffer* buffer = _get_buffer_from_owner(p_buffer);
		if (!buffer) {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Buffer argument is not a valid buffer of any type.");
		}

		ERR_FAIL_COND_V_MSG(p_offset + p_size > buffer->size, ERR_INVALID_PARAMETER,
			std::format("Attempted to write buffer ({} bytes) past the end.", (p_offset + p_size) - buffer->size));

		_check_transfer_worker_buffer(buffer);

		driver->command_clear_buffer(get_current_command_buffer(), buffer->driver_id, p_offset, p_size);

		return OK;
	}

	void RenderingDevice::buffer_flush(RID p_buffer)
	{
		Buffer* buffer = _get_buffer_from_owner(p_buffer);
		ERR_FAIL_NULL_MSG(buffer, "Buffer argument is not a valid buffer of any type.");
		driver->buffer_flush(buffer->driver_id);
	}

	RID RenderingDevice::vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const std::vector<RID>& p_src_buffers, const std::vector<uint64_t>& p_offsets /*= std::vector<uint64_t>()*/)
	{
		ERR_FAIL_COND_V(!vertex_formats.contains(p_vertex_format), RID());
		const VertexDescriptionCache& vd = vertex_formats[p_vertex_format];

		VertexArray vertex_array;

		if (p_offsets.empty()) {
			vertex_array.offsets.resize(p_src_buffers.size());
		}
		else {
			ERR_FAIL_COND_V(p_offsets.size() != p_src_buffers.size(), RID());
			vertex_array.offsets = p_offsets;
		}

		vertex_array.vertex_count = p_vertex_count;
		vertex_array.description = p_vertex_format;
		vertex_array.max_instances_allowed = 0xFFFFFFFF; // By default as many as you want.
		vertex_array.buffers.resize(p_src_buffers.size());

		std::unordered_set<RID> unique_buffers;
		unique_buffers.reserve(p_src_buffers.size());

		for (const VertexAttribute& atf : vd.vertex_formats) {
			ERR_FAIL_COND_V_MSG(atf.binding >= p_src_buffers.size(), RID(), std::format("Vertex attribute location ({}) is missing a buffer for binding ({}).", atf.location, atf.binding));
			RID buf = p_src_buffers[atf.binding];
			ERR_FAIL_COND_V(!vertex_buffer_owner.owns(buf), RID());

			Buffer* buffer = vertex_buffer_owner.get_or_null(buf);

			// Validate with buffer.
			{
				uint32_t element_size = get_format_vertex_size(atf.format);
				ERR_FAIL_COND_V(element_size == 0, RID()); // Should never happen since this was prevalidated.

				if (atf.frequency == VERTEX_FREQUENCY_VERTEX) {
					// Validate size for regular drawing.
					uint64_t total_size = uint64_t(atf.stride) * (p_vertex_count - 1) + atf.offset + element_size;
					ERR_FAIL_COND_V_MSG(total_size > buffer->size, RID(),
						std::format("Vertex attribute ({}) will read past the end of the buffer.", atf.location));

				}
				else {
					// Validate size for instances drawing.
					uint64_t available = buffer->size - atf.offset;
					ERR_FAIL_COND_V_MSG(available < element_size, RID(),
						std::format("Vertex attribute ({}) uses instancing, but it's just too small.", atf.location));

					uint32_t instances_allowed = available / atf.stride;
					vertex_array.max_instances_allowed = MIN(instances_allowed, vertex_array.max_instances_allowed);
				}
			}

			vertex_array.buffers[atf.binding] = buffer->driver_id;

			if (unique_buffers.contains(buf)) {
				// No need to add dependencies multiple times.
				continue;
			}

			unique_buffers.insert(buf);

			//if (buffer->draw_tracker != nullptr) {
			//	vertex_array.draw_trackers.push_back(buffer->draw_tracker);
			//}
			//else {
				vertex_array.untracked_buffers.insert(buf);
			//}

			if (buffer->transfer_worker_index >= 0) {
				vertex_array.transfer_worker_indices.push_back(buffer->transfer_worker_index);
				vertex_array.transfer_worker_operations.push_back(buffer->transfer_worker_operation);
			}
		}

		RID id = vertex_array_owner.make_rid(vertex_array);
		for (const RID& buf : unique_buffers) {
			_add_dependency(id, buf);
		}

		return id;
	}

	RID RenderingDevice::index_buffer_create(uint32_t p_index_count, IndexBufferFormat p_format, std::span<uint8_t> p_data /*= {}*/, bool p_use_restart_indices /*= false*/, BitField<BufferCreationBits> p_creation_bits /*= 0*/)
	{
		ERR_FAIL_COND_V(p_index_count == 0, RID());

		IndexBuffer index_buffer;
		index_buffer.format = p_format;
		index_buffer.supports_restart_indices = p_use_restart_indices;
		index_buffer.index_count = p_index_count;
		uint32_t size_bytes = p_index_count * ((p_format == INDEX_BUFFER_FORMAT_UINT16) ? 2 : 4);
#ifdef DEBUG_ENABLED
		if (p_data.size()) {
			index_buffer.max_index = 0;
			ERR_FAIL_COND_V_MSG((uint32_t)p_data.size() != size_bytes, RID(),
				"Default index buffer initializer array size (" + itos(p_data.size()) + ") does not match format required size (" + itos(size_bytes) + ").");
			const uint8_t* r = p_data.ptr();
			if (p_format == INDEX_BUFFER_FORMAT_UINT16) {
				const uint16_t* index16 = (const uint16_t*)r;
				for (uint32_t i = 0; i < p_index_count; i++) {
					if (p_use_restart_indices && index16[i] == 0xFFFF) {
						continue; // Restart index, ignore.
					}
					index_buffer.max_index = MAX(index16[i], index_buffer.max_index);
				}
			}
			else {
				const uint32_t* index32 = (const uint32_t*)r;
				for (uint32_t i = 0; i < p_index_count; i++) {
					if (p_use_restart_indices && index32[i] == 0xFFFFFFFF) {
						continue; // Restart index, ignore.
					}
					index_buffer.max_index = MAX(index32[i], index_buffer.max_index);
				}
			}
		}
		else {
			index_buffer.max_index = 0xFFFFFFFF;
		}
#else
		index_buffer.max_index = 0xFFFFFFFF;
#endif
		index_buffer.size = size_bytes;
		index_buffer.usage = (RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_INDEX_BIT);
		if (p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT)) {
			index_buffer.usage.set_flag(RDD::BUFFER_USAGE_STORAGE_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
			index_buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
		}
		if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
			index_buffer.usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
		}
		index_buffer.driver_id = driver->buffer_create(index_buffer.size, index_buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
		ERR_FAIL_COND_V(!index_buffer.driver_id, RID());

		// Index buffers are assumed to be immutable unless they don't have initial data.
		//if (p_data.empty()) {
		//	index_buffer.draw_tracker = RDG::resource_tracker_create();
		//	index_buffer.draw_tracker->buffer_driver_id = index_buffer.driver_id;
		//}

		if (p_data.size()) {
			_buffer_initialize(&index_buffer, p_data);
		}

		//_THREAD_SAFE_LOCK_
			buffer_memory += index_buffer.size;
		//_THREAD_SAFE_UNLOCK_

			RID id = index_buffer_owner.make_rid(index_buffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		return id;
	}

	RID RenderingDevice::index_array_create(RID p_index_buffer, uint32_t p_index_offset, uint32_t p_index_count)
	{
		ERR_FAIL_COND_V(!index_buffer_owner.owns(p_index_buffer), RID());

		IndexBuffer* index_buffer = index_buffer_owner.get_or_null(p_index_buffer);

		ERR_FAIL_COND_V(p_index_count == 0, RID());
		ERR_FAIL_COND_V(p_index_offset + p_index_count > index_buffer->index_count, RID());

		IndexArray index_array;
		index_array.max_index = index_buffer->max_index;
		index_array.driver_id = index_buffer->driver_id;
		//index_array.draw_tracker = index_buffer->draw_tracker;
		index_array.offset = p_index_offset;
		index_array.indices = p_index_count;
		index_array.format = index_buffer->format;
		index_array.supports_restart_indices = index_buffer->supports_restart_indices;
		index_array.transfer_worker_index = index_buffer->transfer_worker_index;
		index_array.transfer_worker_operation = index_buffer->transfer_worker_operation;

		RID id = index_array_owner.make_rid(index_array);
		_add_dependency(id, p_index_buffer);
		return id;
	}

	void RenderingDevice::bind_vertex_array(RID p_vertex_array)
	{
		VertexArray* vertex_array = vertex_array_owner.get_or_null(p_vertex_array);
		driver->command_render_bind_vertex_buffers(frames[frame].command_buffer, vertex_array->buffers.size(), vertex_array->buffers.data(), vertex_array->offsets.data(), 0);
	}

	void RenderingDevice::bind_index_array(RID p_index_array)
	{
		IndexArray* index_array = index_array_owner.get_or_null(p_index_array);
		uint64_t byte_offset = index_array->offset * (index_array->format == INDEX_BUFFER_FORMAT_UINT16 ? 2 : 4);
		driver->command_render_bind_index_buffer(frames[frame].command_buffer, index_array->driver_id, index_array->format, byte_offset);
	}

	void RenderingDevice::bind_uniform_set(RID p_shader_id, RID p_uniform_set_id, uint32_t set_index) {
		auto shader = shader_owner.get_or_null(p_shader_id);
		auto uniform_set = uniform_set_owner.get_or_null(p_uniform_set_id);
		add_draw_list_bind_uniform_sets(shader->driver_id, { &uniform_set->driver_id, 1 }, set_index, 1);
	}

	void RenderingDevice::set_push_constant(const void* p_data, uint32_t p_data_size, RID p_shader)
	{
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_MSG(p_data_size != draw_list.validation.pipeline_push_constant_size,
			"This render pipeline requires (" + itos(draw_list.validation.pipeline_push_constant_size) + ") bytes of push constant data, supplied: (" + itos(p_data_size) + ")");
#endif
		auto shader = shader_owner.get_or_null(p_shader);
		std::vector<uint32_t> push_constant_data_view(reinterpret_cast<const uint32_t*>(p_data), (reinterpret_cast<const uint32_t*>(p_data)) + p_data_size / sizeof(uint32_t));
		driver->command_bind_push_constants(get_current_command_buffer(), shader->driver_id, 0, push_constant_data_view);

#ifdef DEBUG_ENABLED
		draw_list.validation.pipeline_push_constant_supplied = true;
#endif
	}

	void RenderingDevice::add_draw_list_bind_uniform_sets(RDD::ShaderID p_shader, std::span<RDD::UniformSetID> p_uniform_sets, uint32_t p_first_index, uint32_t p_set_count) {
		DEV_ASSERT(p_uniform_sets.size() >= p_set_count);

		driver->command_bind_render_uniform_sets(get_current_command_buffer(), p_uniform_sets, p_shader, p_first_index, p_set_count, driver->uniform_sets_get_dynamic_offsets(p_uniform_sets, p_shader, p_first_index, p_set_count));
	}

	RID RenderingDevice::texture_buffer_create(uint32_t p_size_elements, DataFormat p_format, std::span<uint8_t> p_data /*= {}*/)
	{
		uint32_t element_size = get_format_vertex_size(p_format);
		ERR_FAIL_COND_V_MSG(element_size == 0, RID(), "Format requested is not supported for texture buffers");
		uint64_t size_bytes = uint64_t(element_size) * p_size_elements;

		ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != size_bytes, RID());

		Buffer texture_buffer;
		texture_buffer.size = size_bytes;
		BitField<RDD::BufferUsageBits> usage = (RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_TEXEL_BIT);
		texture_buffer.driver_id = driver->buffer_create(size_bytes, usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
		ERR_FAIL_COND_V(!texture_buffer.driver_id, RID());

		// Texture buffers are assumed to be immutable unless they don't have initial data.
		//if (p_data.empty()) {
		//	texture_buffer.draw_tracker = RDG::resource_tracker_create();
		//	texture_buffer.draw_tracker->buffer_driver_id = texture_buffer.driver_id;
		//}

		bool ok = driver->buffer_set_texel_format(texture_buffer.driver_id, p_format);
		if (!ok) {
			driver->buffer_free(texture_buffer.driver_id);
			ERR_FAIL_V(RID());
		}

		if (p_data.size()) {
			_buffer_initialize(&texture_buffer, p_data);
		}

		//_THREAD_SAFE_LOCK_
			buffer_memory += size_bytes;
		//_THREAD_SAFE_UNLOCK_

			RID id = texture_buffer_owner.make_rid(texture_buffer);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		return id;
	}

	RID RenderingDevice::acquire_texture(const RDD::TextureFormat& p_format, const RenderingDevice::TextureView& p_view, const std::vector<std::vector<uint8_t>>& p_data)
	{
		return tex_cache->acquire(p_format, p_view, p_data);
	}

	void RenderingDevice::release_texture(RID p_texture)
	{
		tex_cache->release(p_texture);
	}

	RID RenderingDevice::texture_create(const TextureFormat& p_format, const TextureView& p_view, const std::vector<std::vector<uint8_t>>& p_data /*= std::vector<std::vector<uint8_t>>()*/)
	{
		TextureFormat format = p_format;

		if (format.shareable_formats.size()) {
			ERR_FAIL_COND_V_MSG((std::find(format.shareable_formats.begin(), format.shareable_formats.end(), format.format) == format.shareable_formats.end()), RID(),
				"If supplied a list of shareable formats, the current format must be present in the list");
			ERR_FAIL_COND_V_MSG(p_view.format_override != DATA_FORMAT_MAX && (std::find(format.shareable_formats.begin(), format.shareable_formats.end(), p_view.format_override) == format.shareable_formats.end()), RID(),
				"If supplied a list of shareable formats, the current view format override must be present in the list");
		}

		ERR_FAIL_INDEX_V(format.texture_type, RDD::TEXTURE_TYPE_MAX, RID());

		ERR_FAIL_COND_V_MSG(format.width < 1, RID(), "Width must be equal or greater than 1 for all textures");

		if (format.texture_type != TEXTURE_TYPE_1D && format.texture_type != TEXTURE_TYPE_1D_ARRAY) {
			ERR_FAIL_COND_V_MSG(format.height < 1, RID(), "Height must be equal or greater than 1 for 2D and 3D textures");
		}

		if (format.texture_type == TEXTURE_TYPE_3D) {
			ERR_FAIL_COND_V_MSG(format.depth < 1, RID(), "Depth must be equal or greater than 1 for 3D textures");
		}

		ERR_FAIL_COND_V(format.mipmaps < 1, RID());

		if (format.texture_type == TEXTURE_TYPE_1D_ARRAY || format.texture_type == TEXTURE_TYPE_2D_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE) {
			ERR_FAIL_COND_V_MSG(format.array_layers < 1, RID(),
				"Number of layers must be equal or greater than 1 for arrays and cubemaps.");
			ERR_FAIL_COND_V_MSG((format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE) && (format.array_layers % 6) != 0, RID(),
				"Cubemap and cubemap array textures must provide a layer number that is multiple of 6");
			ERR_FAIL_COND_V_MSG(((format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE)) && (format.width != format.height), RID(),
				"Cubemap and cubemap array textures must have equal width and height.");
			ERR_FAIL_COND_V_MSG(format.array_layers > driver->limit_get(LIMIT_MAX_TEXTURE_ARRAY_LAYERS), RID(), "Number of layers exceeds device maximum.");
		}
		else {
			format.array_layers = 1;
		}

		ERR_FAIL_INDEX_V(format.samples, TEXTURE_SAMPLES_MAX, RID());

		ERR_FAIL_COND_V_MSG(format.usage_bits == 0, RID(), "No usage bits specified (at least one is needed)");

		format.height = format.texture_type != TEXTURE_TYPE_1D && format.texture_type != TEXTURE_TYPE_1D_ARRAY ? format.height : 1;
		format.depth = format.texture_type == TEXTURE_TYPE_3D ? format.depth : 1;

		uint64_t size_max = 0;
		switch (format.texture_type) {
		case TEXTURE_TYPE_1D:
		case TEXTURE_TYPE_1D_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_1D);
			break;
		case TEXTURE_TYPE_2D:
		case TEXTURE_TYPE_2D_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_2D);
			break;
		case TEXTURE_TYPE_CUBE:
		case TEXTURE_TYPE_CUBE_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_CUBE);
			break;
		case TEXTURE_TYPE_3D:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_3D);
			break;
		case TEXTURE_TYPE_MAX:
			break;
		}
		ERR_FAIL_COND_V_MSG(format.width > size_max || format.height > size_max || format.depth > size_max, RID(), "Texture dimensions exceed device maximum.");

		uint32_t required_mipmaps = get_image_required_mipmaps(format.width, format.height, format.depth);

		ERR_FAIL_COND_V_MSG(required_mipmaps < format.mipmaps, RID(),
			std::format("Too many mipmaps requested for texture format and dimensions ({}), maximum allowed: ({}).", format.mipmaps, required_mipmaps));

		std::vector<std::vector<uint8_t>> data = p_data;
		bool immediate_flush = false;

		// If this is a VRS texture, we make sure that it is created with valid initial data. This prevents a crash on Qualcomm Snapdragon XR2 Gen 1
		// (used in Quest 2, Quest Pro, Pico 4, HTC Vive XR Elite and others) where the driver will read the texture before we've had time to finish updating it.
		if (data.empty() && (p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
			immediate_flush = true;
			for (uint32_t i = 0; i < format.array_layers; i++) {
				uint32_t required_size = get_image_format_required_size(format.format, format.width, format.height, format.depth, format.mipmaps);
				std::vector<uint8_t> layer;
				layer.resize(required_size);
				std::fill(layer.begin(), layer.end(), 255);
				data.push_back(layer);
			}
		}

		uint32_t forced_usage_bits = _texture_vrs_method_to_usage_bits();
		if (data.size()) {
			ERR_FAIL_COND_V_MSG(data.size() != (int)format.array_layers, RID(),
				std::format("Default supplied data for image format is of invalid length ({}), should be ({}).", data.size(), format.array_layers));

			for (uint32_t i = 0; i < format.array_layers; i++) {
				uint32_t required_size = get_image_format_required_size(format.format, format.width, format.height, format.depth, format.mipmaps);
				ERR_FAIL_COND_V_MSG((uint32_t)data[i].size() != required_size, RID(),
					std::format("Data for slice index {} (mapped to layer {}) differs in size (supplied: {}) than what is required by the format ({}).", i, i, data[i].size(), required_size));
			}

			ERR_FAIL_COND_V_MSG(format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, RID(),
				"Textures created as depth attachments can't be initialized with data directly. Use RenderingDevice::texture_update() instead.");

			if (!(format.usage_bits & TEXTURE_USAGE_CAN_UPDATE_BIT)) {
				forced_usage_bits |= TEXTURE_USAGE_CAN_UPDATE_BIT;
			}
		}

		{
			// Validate that this image is supported for the intended use.
			bool cpu_readable = (format.usage_bits & RDD::TEXTURE_USAGE_CPU_READ_BIT);
			BitField<RDD::TextureUsageBits> supported_usage = driver->texture_get_usages_supported_by_format(format.format, cpu_readable);

			std::string format_text = "'" + std::string(FORMAT_NAMES[format.format]) + "'";

			if ((format.usage_bits & TEXTURE_USAGE_SAMPLING_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_SAMPLING_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as sampling texture.");
			}
			if ((format.usage_bits & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as color attachment.");
			}
			if ((format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as depth-stencil attachment.");
			}
			if ((format.usage_bits & TEXTURE_USAGE_STORAGE_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_STORAGE_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as storage image.");
			}
			if ((format.usage_bits & TEXTURE_USAGE_STORAGE_ATOMIC_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as atomic storage image.");
			}
			if ((format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
				ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as variable shading rate attachment.");
			}
		}

		// Transfer and validate view info.

		RDD::TextureView tv;
		if (p_view.format_override == DATA_FORMAT_MAX) {
			tv.format = format.format;
		}
		else {
			ERR_FAIL_INDEX_V(p_view.format_override, DATA_FORMAT_MAX, RID());
			tv.format = p_view.format_override;
		}
		ERR_FAIL_INDEX_V(p_view.swizzle_r, TEXTURE_SWIZZLE_MAX, RID());
		ERR_FAIL_INDEX_V(p_view.swizzle_g, TEXTURE_SWIZZLE_MAX, RID());
		ERR_FAIL_INDEX_V(p_view.swizzle_b, TEXTURE_SWIZZLE_MAX, RID());
		ERR_FAIL_INDEX_V(p_view.swizzle_a, TEXTURE_SWIZZLE_MAX, RID());
		tv.swizzle_r = p_view.swizzle_r;
		tv.swizzle_g = p_view.swizzle_g;
		tv.swizzle_b = p_view.swizzle_b;
		tv.swizzle_a = p_view.swizzle_a;

		// Create.

		Texture texture;
		format.usage_bits |= forced_usage_bits;
		texture.driver_id = driver->texture_create(format, tv);
		ERR_FAIL_COND_V(!texture.driver_id, RID());
		texture.type = format.texture_type;
		texture.format = format.format;
		texture.width = format.width;
		texture.height = format.height;
		texture.depth = format.depth;
		texture.layers = format.array_layers;
		texture.mipmaps = format.mipmaps;
		texture.base_mipmap = 0;
		texture.base_layer = 0;
		texture.is_resolve_buffer = format.is_resolve_buffer;
		texture.is_discardable = format.is_discardable;
		texture.usage_flags = format.usage_bits & ~forced_usage_bits;
		texture.samples = format.samples;
		texture.allowed_shared_formats = format.shareable_formats;
		texture.has_initial_data = !data.empty();

		if (driver->api_trait_get(RDD::API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS)) {
			// Check if a clear for this texture must be performed the first time it's used if the driver requires explicit clears after initialization.
			texture.pending_clear = !texture.has_initial_data && (format.usage_bits & (TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
		}

		if ((format.usage_bits & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT))) {
			texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
			texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
			if (format_has_stencil(format.format)) {
				texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
			}
		}
		else {
			texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
			texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
		}

		texture.bound = false;

		// Textures are only assumed to be immutable if they have initial data and none of the other bits that indicate write usage are enabled.
		bool texture_mutable_by_default = texture.usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
		if (data.empty() || texture_mutable_by_default) {
			//TODO: re
			//_texture_make_mutable(&texture, RID());
		}

		texture_memory += driver->texture_get_allocation_size(texture.driver_id);

		RID id = texture_owner.make_rid(texture);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif

		if (data.size()) {
			const bool use_general_in_copy_queues = driver->api_trait_get(RDD::API_TRAIT_USE_GENERAL_IN_COPY_QUEUES);
			const RDD::TextureLayout dst_layout = use_general_in_copy_queues ? RDD::TEXTURE_LAYOUT_GENERAL : RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL;
			for (uint32_t i = 0; i < format.array_layers; i++) {
				_texture_initialize(id, i, data[i], dst_layout, immediate_flush);
			}

			//if (texture.draw_tracker != nullptr) {
			//	texture.draw_tracker->usage = use_general_in_copy_queues ? RDG::RESOURCE_USAGE_GENERAL : RDG::RESOURCE_USAGE_COPY_TO;
			//}
		}

		return id;
	}

	Error RenderingDevice::texture_update(RID p_texture, uint32_t p_layer, const std::vector<uint8_t>& p_data)
	{

		Texture* texture = texture_owner.get_or_null(p_texture);
		ERR_FAIL_NULL_V(texture, ERR_INVALID_PARAMETER);

		if (texture->owner != RID()) {
			p_texture = texture->owner;
			texture = texture_owner.get_or_null(texture->owner);
			ERR_FAIL_NULL_V(texture, ERR_BUG); // This is a bug.
		}

		ERR_FAIL_COND_V_MSG(texture->bound, ERR_CANT_ACQUIRE_RESOURCE,
			"Texture can't be updated while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to update this texture.");

		ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_CAN_UPDATE_BIT), ERR_INVALID_PARAMETER, "Texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT` to be set to be updatable.");

		uint32_t layer_count = _texture_layer_count(texture);
		ERR_FAIL_COND_V(p_layer >= layer_count, ERR_INVALID_PARAMETER);

		uint32_t width, height;
		uint32_t tight_mip_size = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, texture->mipmaps, &width, &height);
		uint32_t required_size = tight_mip_size;
		uint32_t required_align = _texture_alignment(texture);

		ERR_FAIL_COND_V_MSG(required_size != (uint32_t)p_data.size(), ERR_INVALID_PARAMETER,
			std::format("Required size for texture update ({}) does not match data supplied size ({}).", required_size, p_data.size()));

		// Clear the texture if the driver requires it during its first use.
		_texture_check_pending_clear(p_texture, texture);

		_check_transfer_worker_texture(texture);

		uint32_t block_w, block_h;
		get_compressed_image_format_block_dimensions(texture->format, block_w, block_h);

		uint32_t pixel_size = get_image_format_pixel_size(texture->format);
		uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(texture->format);
		uint32_t block_size = get_compressed_image_format_block_byte_size(texture->format);

		uint32_t region_size = texture_upload_region_size_px;

		const uint8_t* read_ptr = p_data.data();

		thread_local std::vector<RecordedBufferToTextureCopy> command_buffer_to_texture_copies_vector;
		command_buffer_to_texture_copies_vector.clear();

		// Indicate the texture will get modified for the shared texture fallback.
		_texture_update_shared_fallback(p_texture, texture, true);

		uint32_t mipmap_offset = 0;

		uint32_t logic_width = texture->width;
		uint32_t logic_height = texture->height;

		for (uint32_t mm_i = 0; mm_i < texture->mipmaps; mm_i++) {
			uint32_t depth = 0;
			uint32_t image_total = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, mm_i + 1, &width, &height, &depth);

			const uint8_t* read_ptr_mipmap = read_ptr + mipmap_offset;
			tight_mip_size = image_total - mipmap_offset;

			for (uint32_t z = 0; z < depth; z++) {
				const uint8_t* read_ptr_mipmap_layer = read_ptr_mipmap + (tight_mip_size / depth) * z;
				for (uint32_t y = 0; y < height; y += region_size) {
					for (uint32_t x = 0; x < width; x += region_size) {
						uint32_t region_w = MIN(region_size, width - x);
						uint32_t region_h = MIN(region_size, height - y);

						uint32_t region_logic_w = MIN(region_size, logic_width - x);
						uint32_t region_logic_h = MIN(region_size, logic_height - y);

						uint32_t region_pitch = (region_w * pixel_size * block_w) >> pixel_rshift;
						uint32_t pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
						region_pitch = STEPIFY(region_pitch, pitch_step);
						uint32_t to_allocate = region_pitch * region_h;
						uint32_t alloc_offset = 0, alloc_size = 0;
						StagingRequiredAction required_action;
						Error err = _staging_buffer_allocate(upload_staging_buffers, to_allocate, required_align, alloc_offset, alloc_size, required_action, false);
						ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

						if (!command_buffer_to_texture_copies_vector.empty() && required_action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL) {
							//if (_texture_make_mutable(texture, p_texture)) {
							//	// The texture must be mutable to be used as a copy destination.
							//	draw_graph.add_synchronization();
							//}

							// If the staging buffer requires flushing everything, we submit the command early and clear the current vector.
							//draw_graph.add_texture_update(texture->driver_id, texture->draw_tracker, command_buffer_to_texture_copies_vector);
							for (auto& tex_c: command_buffer_to_texture_copies_vector)
							{
								driver->command_copy_buffer_to_texture(get_current_command_buffer(), tex_c.from_buffer, texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, { &tex_c.region, 1 });
							}
							command_buffer_to_texture_copies_vector.clear();
						}

						_staging_buffer_execute_required_action(upload_staging_buffers, required_action);

						uint8_t* write_ptr = upload_staging_buffers.blocks[upload_staging_buffers.current].data_ptr + alloc_offset;

						ERR_FAIL_COND_V(region_w % block_w, ERR_BUG);
						ERR_FAIL_COND_V(region_h % block_h, ERR_BUG);

						_copy_region_block_or_regular(read_ptr_mipmap_layer, write_ptr, x, y, width, region_w, region_h, block_w, block_h, region_pitch, pixel_size, block_size);

						RDD::BufferTextureCopyRegion copy_region;
						copy_region.buffer_offset = alloc_offset;
						copy_region.row_pitch = region_pitch;
						copy_region.texture_subresource.aspect = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
						copy_region.texture_subresource.mipmap = mm_i;
						copy_region.texture_subresource.layer = p_layer;
						copy_region.texture_offset = Vector3i(x, y, z);
						copy_region.texture_region_size = Vector3i(region_logic_w, region_logic_h, 1);

						RecordedBufferToTextureCopy buffer_to_texture_copy;
						buffer_to_texture_copy.from_buffer = upload_staging_buffers.blocks[upload_staging_buffers.current].driver_id;
						buffer_to_texture_copy.region = copy_region;
						command_buffer_to_texture_copies_vector.push_back(buffer_to_texture_copy);

						upload_staging_buffers.blocks[upload_staging_buffers.current].fill_amount = alloc_offset + alloc_size;
					}
				}
			}

			mipmap_offset = image_total;
			logic_width = MAX(1u, logic_width >> 1);
			logic_height = MAX(1u, logic_height >> 1);
		}

		//if (_texture_make_mutable(texture, p_texture)) {
		//	// The texture must be mutable to be used as a copy destination.
		//	draw_graph.add_synchronization();
		//}

		//draw_graph.add_texture_update(texture->driver_id, texture->draw_tracker, command_buffer_to_texture_copies_vector);
		for (auto& tex_c : command_buffer_to_texture_copies_vector)
		{
			driver->command_copy_buffer_to_texture(get_current_command_buffer(), tex_c.from_buffer, texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, { &tex_c.region, 1 });
		}
		return OK;
	}

	std::vector<uint8_t> RenderingDevice::texture_get_data(RID p_texture, uint32_t p_layer)
	{
		//ERR_RENDER_THREAD_GUARD_V(std::vector<uint8_t>());

		Texture* tex = texture_owner.get_or_null(p_texture);
		ERR_FAIL_NULL_V(tex, std::vector<uint8_t>());

		ERR_FAIL_COND_V_MSG(tex->bound, std::vector<uint8_t>(),
			"Texture can't be retrieved while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to retrieve this texture.");
		ERR_FAIL_COND_V_MSG(!(tex->usage_flags & TEXTURE_USAGE_CAN_COPY_FROM_BIT), std::vector<uint8_t>(),
			"Texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT` to be set to be retrieved.");

		ERR_FAIL_COND_V(p_layer >= tex->layers, std::vector<uint8_t>());

		// Clear the texture if the driver requires it during its first use.
		_texture_check_pending_clear(p_texture, tex);

		_check_transfer_worker_texture(tex);

		if (tex->usage_flags & TEXTURE_USAGE_CPU_READ_BIT) {
			return driver->texture_get_data(tex->driver_id, p_layer);
		}
		else {
			RDD::TextureAspect aspect = tex->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
			uint32_t mip_alignment = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);
			uint32_t buffer_size = 0;

			thread_local std::vector<RDD::TextureCopyableLayout> mip_layouts;
			thread_local std::vector<RDD::BufferTextureCopyRegion> copy_regions;
			mip_layouts.resize(tex->mipmaps);
			copy_regions.resize(tex->mipmaps);

			for (uint32_t i = 0; i < tex->mipmaps; i++) {
				RDD::TextureSubresource subres;
				subres.aspect = aspect;
				subres.layer = p_layer;
				subres.mipmap = i;

				RDD::TextureCopyableLayout& mip_layout = mip_layouts[i];
				driver->texture_get_copyable_layout(tex->driver_id, subres, &mip_layout);

				uint32_t mip_offset = STEPIFY(buffer_size, mip_alignment);
				buffer_size = mip_offset + mip_layout.size;

				RDD::BufferTextureCopyRegion& copy_region = copy_regions[i];
				copy_region.buffer_offset = mip_offset;
				copy_region.row_pitch = mip_layout.row_pitch;
				copy_region.texture_subresource.aspect = aspect;
				copy_region.texture_subresource.mipmap = i;
				copy_region.texture_subresource.layer = p_layer;
				copy_region.texture_region_size.x = MAX(1u, tex->width >> i);
				copy_region.texture_region_size.y = MAX(1u, tex->height >> i);
				copy_region.texture_region_size.z = MAX(1u, tex->depth >> i);
			}

			RDD::BufferID tmp_buffer = driver->buffer_create(buffer_size, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
			ERR_FAIL_COND_V(!tmp_buffer, std::vector<uint8_t>());

			//if (_texture_make_mutable(tex, p_texture)) {
			//	// The texture must be mutable to be used as a copy source due to layout transitions.
			//	draw_graph.add_synchronization();
			//}

			//draw_graph.add_texture_get_data(tex->driver_id, tex->draw_tracker, tmp_buffer, copy_regions);

			// Flush everything so memory can be safely mapped.
			_flush_and_stall_for_all_frames();

			const uint8_t* read_ptr = driver->buffer_map(tmp_buffer);
			ERR_FAIL_NULL_V(read_ptr, std::vector<uint8_t>());

			uint32_t block_w = 0;
			uint32_t block_h = 0;
			get_compressed_image_format_block_dimensions(tex->format, block_w, block_h);

			std::vector<uint8_t> buffer_data;
			uint32_t tight_buffer_size = get_image_format_required_size(tex->format, tex->width, tex->height, tex->depth, tex->mipmaps);
			buffer_data.resize(tight_buffer_size);

			uint8_t* write_ptr = buffer_data.data();

			for (uint32_t i = 0; i < tex->mipmaps; i++) {
				uint32_t width = 0, height = 0, depth = 0;

				uint32_t tight_mip_size = get_image_format_required_size(
					tex->format,
					MAX(1u, tex->width >> i),
					MAX(1u, tex->height >> i),
					MAX(1u, tex->depth >> i),
					1,
					&width,
					&height,
					&depth);

				uint32_t row_count = (height / block_h) * depth;
				uint32_t tight_row_pitch = tight_mip_size / row_count;

				const uint8_t* rp = read_ptr + copy_regions[i].buffer_offset;
				uint32_t row_pitch = mip_layouts[i].row_pitch;

				if (tight_row_pitch == row_pitch) {
					// Same row pitch, we can copy directly.
					memcpy(write_ptr, rp, tight_mip_size);
					write_ptr += tight_mip_size;
				}
				else {
					// Copy row-by-row to erase padding.
					for (uint32_t j = 0; j < row_count; j++) {
						memcpy(write_ptr, rp, tight_row_pitch);
						rp += row_pitch;
						write_ptr += tight_row_pitch;
					}
				}
			}

			driver->buffer_unmap(tmp_buffer);
			driver->buffer_free(tmp_buffer);

			return buffer_data;
		}
	}

	RDD::TextureID RenderingDevice::texture_id_from_rid(RID texture)
	{
		return texture_owner.get_or_null(texture)->driver_id;
	}

	RID RenderingDevice::sampler_create(const SamplerState& p_state)
	{
		//_THREAD_SAFE_METHOD_

		ERR_FAIL_INDEX_V(p_state.repeat_u, SAMPLER_REPEAT_MODE_MAX, RID());
		ERR_FAIL_INDEX_V(p_state.repeat_v, SAMPLER_REPEAT_MODE_MAX, RID());
		ERR_FAIL_INDEX_V(p_state.repeat_w, SAMPLER_REPEAT_MODE_MAX, RID());
		ERR_FAIL_INDEX_V(p_state.compare_op, COMPARE_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_state.border_color, SAMPLER_BORDER_COLOR_MAX, RID());

		auto it = sampler_cache.find(p_state);
		if (it != sampler_cache.end())
			return it->second;

		RDD::SamplerID sampler = driver->sampler_create(p_state);
		ERR_FAIL_COND_V(!sampler, RID());

		RID id = sampler_owner.make_rid(sampler);
#ifdef DEV_ENABLED
		set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
		sampler_cache.emplace(p_state, id);
		return id;
	}

	bool RenderingDevice::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_sampler_filter) const {
		//_THREAD_SAFE_METHOD_

		ERR_FAIL_INDEX_V(p_format, DATA_FORMAT_MAX, false);

		return driver->sampler_is_format_supported_for_filter(p_format, p_sampler_filter);
	}

	void RenderingDevice::apply_image_barrier(RDD::CommandBufferID p_cmd_buffer, BitField<RenderingDeviceDriver::PipelineStageBits> p_src_stages, BitField<RenderingDeviceDriver::PipelineStageBits> p_dst_stages, std::span<RenderingDeviceDriver::TextureBarrier> p_texture_barriers)
	{
		driver->command_pipeline_barrier(p_cmd_buffer, p_src_stages, p_dst_stages, {}, {}, p_texture_barriers, {});
	}

	void RenderingDevice::execute_chained_cmds(bool p_present_swap_chain, RenderingDeviceDriver::FenceID p_draw_fence, RenderingDeviceDriver::SemaphoreID p_dst_draw_semaphore_to_signal)
	{
		uint32_t command_buffer_count = 1;
		CommandBufferPool& buffer_pool = frames[frame].command_buffer_pool;
		if (buffer_pool.buffers_used > 0) {
			command_buffer_count += buffer_pool.buffers_used;
			buffer_pool.buffers_used = 0;
		}

		thread_local std::vector<RDD::SwapChainID> swap_chains;
		swap_chains.clear();

		// Instead of having just one command; we have potentially many (which had to be split due to an
		// Adreno workaround on mobile, only if the workaround is active). Thus we must execute all of them
		// and chain them together via semaphores as dependent executions.
		thread_local std::vector<RDD::SemaphoreID> wait_semaphores;
		wait_semaphores = frames[frame].semaphores_to_wait_on;

		for (uint32_t i = 0; i < command_buffer_count; i++) {
			RDD::CommandBufferID command_buffer;
			RDD::SemaphoreID signal_semaphore;
			RDD::FenceID signal_fence;
			if (i > 0) {
				command_buffer = buffer_pool.buffers[i - 1];
			}
			else {
				command_buffer = frames[frame].command_buffer;
			}

			if (i == (command_buffer_count - 1)) {
				// This is the last command buffer, it should signal the semaphore & fence.
				signal_semaphore = p_dst_draw_semaphore_to_signal;
				signal_fence = p_draw_fence;

				if (p_present_swap_chain) {
					// Just present the swap chains as part of the last command execution.
					swap_chains = frames[frame].swap_chains_to_present;
				}
			}
			else {
				signal_semaphore = buffer_pool.semaphores[i];
				// Semaphores always need to be signaled if it's not the last command buffer.
			}
			if (!signal_semaphore)
			{
				driver->command_queue_execute_and_present(main_queue, wait_semaphores, { &command_buffer, 1 },
					{ }, signal_fence,
					swap_chains);
			}
			else
			{
				driver->command_queue_execute_and_present(main_queue, wait_semaphores, { &command_buffer, 1 },
					{ &signal_semaphore, 1 }, signal_fence,
					swap_chains);
			}

			// Make the next command buffer wait on the semaphore signaled by this one.
			wait_semaphores.resize(1);
			wait_semaphores[0] = signal_semaphore;
		}

		frames[frame].semaphores_to_wait_on.clear();
	}

	void RenderingDevice::swap_buffers(bool p_present)
	{
		end_frame();

		execute_frame(p_present);

		frame = (frame + 1) % frames.size();
		begin_frame();

	}

	bool RenderingDevice::begin_for_screen(DisplayServerEnums::WindowID p_screen /*= 0*/, const Color& p_clear_color /*= Color()*/)
	{
		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;

		RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator sc_it = screen_swap_chains.find(p_screen);
		std::unordered_map<DisplayServerEnums::WindowID, RDD::FramebufferID>::const_iterator fb_it = screen_framebuffers.find(p_screen);
		ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
		ERR_FAIL_COND_V_MSG(sc_it == screen_swap_chains.end(), INVALID_ID, "Screen was never prepared.");
		ERR_FAIL_COND_V_MSG(fb_it == screen_framebuffers.end(), INVALID_ID, "Framebuffer was never prepared.");

		std::vector<Rect2i> viewport{ Rect2i(0, 0, context->surface_get_width(surface), context->surface_get_height(surface)) };

		RDD::RenderPassID render_pass = driver->swap_chain_get_render_pass(sc_it->second);

		std::array<RenderingDeviceDriver::RenderPassClearValue, 1> val;
		val[0].color = p_clear_color;

		driver->command_begin_render_pass(command_buffer, render_pass, fb_it->second, RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY, viewport[0], val);
		driver->command_render_set_viewport(command_buffer, viewport);
		driver->command_render_set_scissor(command_buffer, viewport);
	}

	bool RenderingDevice::end_for_screen(DisplayServerEnums::WindowID p_screen)
	{
		driver->command_end_render_pass(get_current_command_buffer());
		return true;
	}

	void RenderingDevice::free_framebuffer(RDD::FramebufferID p_frame_buffer)
	{
		driver->framebuffer_free(p_frame_buffer);
	}

	RDD::FramebufferID RenderingDevice::create_framebuffer(RDD::RenderPassID p_render_pass, std::span<RDD::TextureID> p_attachments, uint32_t p_width, uint32_t p_height)
	{
		auto id = driver->framebuffer_create(p_render_pass, p_attachments, p_width, p_height);
		//renderpass_to_frame_buffer_id[p_render_pass] = id;
		return id;
	}

	RDD::FramebufferID RenderingDevice::create_framebuffer_from_format_id(FramebufferFormatID p_format_id, std::vector<RID> p_attachments, uint32_t p_width, uint32_t p_height)
	{
		std::vector<RDD::TextureID> attachments;
		for (auto a: p_attachments)
		{
			attachments.push_back(texture_owner.get_or_null(a)->driver_id);
		}
		return driver->framebuffer_create(framebuffer_formats[p_format_id].render_pass, attachments, p_width, p_height);
	}

	RDD::FramebufferID RenderingDevice::create_framebuffer_from_render_pass(RDD::RenderPassID p_render_pass, std::vector<RID> p_attachments, uint32_t p_width, uint32_t p_height, uint32_t p_layers /*= 1*/)
	{
		std::vector<RDD::TextureID> attachments;
		for (auto a : p_attachments)
		{
			attachments.push_back(texture_owner.get_or_null(a)->driver_id);
		}
		return driver->framebuffer_create(p_render_pass, attachments, p_width, p_height, p_layers);
	}

	RDD::RenderPassID RenderingDevice::render_pass_from_format_id(FramebufferFormatID p_format_id)
	{
		return framebuffer_formats[p_format_id].render_pass;
	}

	bool RenderingDevice::begin_render_pass(RDD::RenderPassID p_render_pass, RDD::FramebufferID p_frame_buffer, Rect2i p_region, const Color& p_clear_color)
	{
		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;
		std::vector<Rect2i> viewport{ p_region };

		std::array<RenderingDeviceDriver::RenderPassClearValue, 1> val;
		val[0].color = p_clear_color;

		driver->command_begin_render_pass(command_buffer, p_render_pass, p_frame_buffer, RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY, viewport[0], val);
		driver->command_render_set_viewport(command_buffer, viewport);
		driver->command_render_set_scissor(command_buffer, viewport);
		return true;
	}

	bool RenderingDevice::begin_render_pass_from_frame_buffer(RID p_frame_buffer, Rect2i p_region, const std::span<RenderingDeviceDriver::RenderPassClearValue>& p_clear_color)
	{
		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;
		std::vector<Rect2i> viewport{ p_region };

		auto frame_buffer = framebuffer_owner.get_or_null(p_frame_buffer);
		auto frame_buffer_format = framebuffer_formats[frame_buffer->format_id];
		auto render_pass = frame_buffer_format.render_pass;

		driver->command_begin_render_pass(command_buffer, render_pass, rid_to_frame_buffer_id[p_frame_buffer], RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY, viewport[0], p_clear_color);
		driver->command_render_set_viewport(command_buffer, viewport);
		driver->command_render_set_scissor(command_buffer, viewport);
		return true;
	}

	RDD::CommandBufferID RenderingDevice::get_current_command_buffer()
	{
		return frames[frame].command_buffer;
	}

	void RenderingDevice::render_draw(RenderingDeviceDriver::CommandBufferID p_command_buffer, uint32_t p_vertex_count, uint32_t p_instance_count)
	{
		//TODO: re
		driver->command_render_draw(p_command_buffer, p_vertex_count, p_instance_count, 0, 0);
	}

	void RenderingDevice::render_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance)
	{
		driver->command_render_draw_indexed(p_command_buffer, p_index_count, p_instance_count, p_first_index, p_vertex_offset, p_first_instance);
	}

	Rendering::RenderingDevice::VertexFormatID RenderingDevice::vertex_format_create(const std::vector<VertexAttribute>& p_vertex_descriptions)
	{
		VertexDescriptionKey key;
		key.vertex_formats = p_vertex_descriptions;

		if (!vertex_format_cache.empty())
		{
			auto it_c = vertex_format_cache.find(key);
			if (it_c != vertex_format_cache.end()) {
				return it_c->second;
			}
		}


		VertexAttributeBindingsMap bindings;
		bool has_implicit = false;
		bool has_explicit = false;
		std::vector<VertexAttribute> vertex_descriptions = p_vertex_descriptions;
		std::unordered_set<int> used_locations;

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

	RID RenderingDevice::create_swapchain_pipeline(DisplayServerEnums::WindowID window, RID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, 
		const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, 
		const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/, 
		const std::vector<PipelineSpecializationConstant>& p_specialization_constants /*= std::vector<PipelineSpecializationConstant>()*/)
	{
		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

		ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator sc_it = screen_swap_chains.find(window);
		ERR_FAIL_COND_V_MSG(sc_it == screen_swap_chains.end(), RID(), "Screen was never prepared.");

		RDD::VertexFormatID driver_vertex_format;
		if (p_vertex_format != INVALID_ID) {
			// Uses vertices, else it does not.
			ERR_FAIL_COND_V(!vertex_formats.contains(p_vertex_format), RID());
			const VertexDescriptionCache& vd = vertex_formats[p_vertex_format];
			driver_vertex_format = vertex_formats[p_vertex_format].driver_id;

			// Validate with inputs.
			for (uint32_t i = 0; i < 64; i++) {
				if (!(shader->vertex_input_mask & ((uint64_t)1) << i)) {
					continue;
				}
				bool found = false;
				for (int j = 0; j < vd.vertex_formats.size(); j++) {
					if (vd.vertex_formats[j].location == i) {
						found = true;
						break;
					}
				}

				ERR_FAIL_COND_V_MSG(!found, RID(),
					std::format("Shader vertex input location {} not provided in vertex input description for pipeline creation.", std::to_string(i)));
			}

		}
		else {
			ERR_FAIL_COND_V_MSG(shader->vertex_input_mask != 0, RID(),
				std::format("Shader contains vertex inputs, but no vertex input description was provided for pipeline creation."));
		}

		ERR_FAIL_INDEX_V(p_render_primitive, RENDER_PRIMITIVE_MAX, RID());

		ERR_FAIL_INDEX_V(p_rasterization_state.cull_mode, 3, RID());

		if (p_multisample_state.sample_mask.size()) {
			// Use sample mask.
			ERR_FAIL_COND_V((int)TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count] != p_multisample_state.sample_mask.size(), RID());
		}

		ERR_FAIL_INDEX_V(p_depth_stencil_state.depth_compare_operator, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.pass, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.depth_fail, STENCIL_OP_MAX, RID());
		ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.compare, COMPARE_OP_MAX, RID());

		ERR_FAIL_INDEX_V(p_blend_state.logic_op, LOGIC_OP_MAX, RID());

		for (int i = 0; i < shader->specialization_constants.size(); i++) {
			const ShaderSpecializationConstant& sc = shader->specialization_constants[i];
			for (int j = 0; j < p_specialization_constants.size(); j++) {
				const PipelineSpecializationConstant& psc = p_specialization_constants[j];
				if (psc.constant_id == sc.constant_id) {
					ERR_FAIL_COND_V_MSG(psc.type != sc.type, RID(), std::format("Specialization constant provided for id {} is of the wrong type.", std::to_string(sc.constant_id)));
					break;
				}
			}
		}

		auto render_pass = driver->swap_chain_get_render_pass(sc_it->second);
		std::vector<int32_t> color_attachments{ 1 };
		std::vector<PipelineSpecializationConstant> specialization_constants = p_specialization_constants;
		RenderPipeline pipeline;
		pipeline.driver_id = driver->render_pipeline_create(
			shader->driver_id,
			driver_vertex_format,
			p_render_primitive,
			p_rasterization_state,
			p_multisample_state,
			p_depth_stencil_state,
			p_blend_state,
			color_attachments,
			p_dynamic_state_flags,
			render_pass,
			p_for_render_pass,
			specialization_constants);
		ERR_FAIL_COND_V(!pipeline.driver_id, RID());

		pipeline.shader = p_shader;
		pipeline.shader_driver_id = shader->driver_id;
		pipeline.shader_layout_hash = shader->layout_hash;
		pipeline.set_formats = shader->set_formats;
		pipeline.push_constant_size = shader->push_constant_size;
		pipeline.stage_bits = shader->stage_bits;

#ifdef DEBUG_ENABLED
		pipeline.validation.dynamic_state = p_dynamic_state_flags;
		pipeline.validation.framebuffer_format = p_framebuffer_format;
		pipeline.validation.render_pass = p_for_render_pass;
		pipeline.validation.vertex_format = p_vertex_format;
		pipeline.validation.uses_restart_indices = p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX;

		static const uint32_t primitive_divisor[RENDER_PRIMITIVE_MAX] = {
			1, 2, 1, 1, 1, 3, 1, 1, 1, 1, 1
		};
		pipeline.validation.primitive_divisor = primitive_divisor[p_render_primitive];
		static const uint32_t primitive_minimum[RENDER_PRIMITIVE_MAX] = {
			1,
			2,
			2,
			2,
			2,
			3,
			3,
			3,
			3,
			3,
			1,
		};
		pipeline.validation.primitive_minimum = primitive_minimum[p_render_primitive];
#endif

		// Create ID to associate with this pipeline.
		RID id = render_pipeline_owner.make_rid(pipeline);

		{
			//_THREAD_SAFE_METHOD_

#ifdef DEV_ENABLED
				set_resource_name(id, std::format("RID {}", id.get_id()));
#endif
			// Now add all the dependencies.
			_add_dependency(id, p_shader);
		}
		return id;
	}

	void RenderingDevice::begin_frame(bool p_presented /*= false*/)
	{
		_stall_for_frame(frame);

		frames_drawn++;

		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;

		driver->command_buffer_begin(command_buffer);

		_free_pending_resources(frame);

		// Advance staging buffers if used.
		if (upload_staging_buffers.used) {
			upload_staging_buffers.current = (upload_staging_buffers.current + 1) % upload_staging_buffers.blocks.size();
			upload_staging_buffers.used = false;
		}

		if (download_staging_buffers.used) {
			download_staging_buffers.current = (download_staging_buffers.current + 1) % download_staging_buffers.blocks.size();
			download_staging_buffers.used = false;
		}

		if (frames[frame].timestamp_count) {
			driver->timestamp_query_pool_get_results(frames[frame].timestamp_pool, frames[frame].timestamp_count, frames[frame].timestamp_result_values.data());
			driver->command_timestamp_query_pool_reset(frames[frame].command_buffer, frames[frame].timestamp_pool, frames[frame].timestamp_count);
			SWAP(frames[frame].timestamp_names, frames[frame].timestamp_result_names);
			SWAP(frames[frame].timestamp_cpu_values, frames[frame].timestamp_cpu_result_values);
		}

		frames[frame].timestamp_result_count = frames[frame].timestamp_count;
		frames[frame].timestamp_count = 0;
		frames[frame].index = frames_drawn;
	}

	void RenderingDevice::end_frame()
	{
		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;

		_submit_transfer_workers(command_buffer);
		_submit_transfer_barriers(command_buffer);
		//driver->command_end_render_pass(command_buffer);
		driver->command_buffer_end(command_buffer);

		fb_cache->tick();
	}

	void  RenderingDevice::bind_render_pipeline(RDD::CommandBufferID p_command_buffer, RID pipeline)
	{
		RenderPipeline* render_pipeline = render_pipeline_owner.get_or_null(pipeline);
		driver->command_bind_render_pipeline(p_command_buffer, render_pipeline->driver_id);
	}

	void RenderingDevice::end_render_pass(RDD::CommandBufferID cmd)
	{
		driver->command_end_render_pass(cmd);
	}

	void RenderingDevice::execute_frame(bool p_present)
	{
		// Check whether this frame should present the swap chains and in which queue.
		const bool frame_can_present = p_present && !frames[frame].swap_chains_to_present.empty();
		const bool separate_present_queue = main_queue != present_queue;

		// The semaphore is required if the frame can be presented and a separate present queue is used;
		// since the separate queue will wait for that semaphore before presenting.
		const RDD::SemaphoreID semaphore = (frame_can_present && separate_present_queue)
			? frames[frame].semaphore
			: RDD::SemaphoreID(nullptr);
		const bool present_swap_chain = frame_can_present && !separate_present_queue;

		execute_chained_cmds(present_swap_chain, frames[frame].fence, semaphore);
		// Indicate the fence has been signaled so the next time the frame's contents need to be
		// used, the CPU needs to wait on the work to be completed.
		frames[frame].fence_signaled = true;
		std::vector<RenderingDeviceDriver::SemaphoreID> frame_semaphores{ frames[frame].semaphore };
		if (frame_can_present) {
			if (separate_present_queue) {
				// Issue the presentation separately if the presentation queue is different from the main queue.
				// Only wait on the semaphore signaled by the main queue and present — no command buffer or fence,
				// since the command buffer was already submitted and the fence already signaled by execute_chained_cmds.
				driver->command_queue_execute_and_present(present_queue, frame_semaphores, {}, {}, RDD::FenceID(), frames[frame].swap_chains_to_present);
			}

			frames[frame].swap_chains_to_present.clear();
		}
	}

	Error RenderingDevice::iniitialize_imgui_device(WindowPlatformData p_platfform_data, uint32_t p_devince_index /*=0*/, uint32_t swapchain_index/*=0*/)
	{
		imgui_device = std::make_unique<Vulkan::ImGuiDevice>(p_platfform_data, context, driver);

		TextureFormat tf;
		tf.texture_type = TEXTURE_TYPE_2D;
		tf.width = screen_get_width();
		tf.height = screen_get_height();
		tf.usage_bits = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLING_BIT;
		tf.format = driver->swap_chain_get_format(screen_swap_chains[swapchain_index]/*temp*/);;
		
		imgui_device->initialize(p_devince_index, main_queue_family.id - 1, 2, _get_swap_chain_desired_count(), driver->swap_chain_get_format(screen_swap_chains[swapchain_index]/*temp*/), screen_get_width(), screen_get_height());

		return OK;
	}

	void RenderingDevice::imgui_begin_frame()
	{
		imgui_device->begin_frame();
	}

	RID RenderingDevice::get_imgui_texture() {
		return imgui_texture_rid;
	}

	void RenderingDevice::imgui_execute(void* p_draw_data, RDD::CommandBufferID p_command_buffer, RID p_frame_buffer, RDD::PipelineID p_pipeline /*= RDD::PipelineID()*/)
	{
		std::vector<Rect2i> viewport{ Rect2i(0, 0, screen_get_width(), screen_get_height()) };

		std::array<RenderingDeviceDriver::RenderPassClearValue, 1> val;
		val[0].color = Color();

		driver->command_begin_render_pass(p_command_buffer, imgui_device->get_imgui_renderpass(), rid_to_frame_buffer_id[p_frame_buffer], RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY, viewport[0], val);
		driver->command_render_set_viewport(p_command_buffer, viewport);
		driver->command_render_set_scissor(p_command_buffer, viewport);
		imgui_device->execute(p_draw_data, p_command_buffer, p_pipeline);
		end_render_pass(p_command_buffer);
	}

	Vulkan::ImGuiDevice* RenderingDevice::get_imgui_device()
	{
		return imgui_device.get();
	}

	// The full list of resources that can be named is in the VkObjectType enum.
	// We just expose the resources that are owned and can be accessed easily.
	void RenderingDevice::set_resource_name(RID p_id, const std::string& p_name) {
		//_THREAD_SAFE_METHOD_

			if (texture_owner.owns(p_id)) {
				Texture* texture = texture_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_TEXTURE, texture->driver_id, p_name);
			}
			else if (framebuffer_owner.owns(p_id)) {
				//Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_id);
				// Not implemented for now as the relationship between Framebuffer and RenderPass is very complex.
			}
			else if (sampler_owner.owns(p_id)) {
				RDD::SamplerID sampler_driver_id = *sampler_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_SAMPLER, sampler_driver_id, p_name);
			}
			else if (vertex_buffer_owner.owns(p_id)) {
				Buffer* vertex_buffer = vertex_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, vertex_buffer->driver_id, p_name);
			}
			else if (index_buffer_owner.owns(p_id)) {
				IndexBuffer* index_buffer = index_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, index_buffer->driver_id, p_name);
			}
			else if (shader_owner.owns(p_id)) {
				Shader* shader = shader_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_SHADER, shader->driver_id, p_name);
			}
			else if (uniform_buffer_owner.owns(p_id)) {
				Buffer* uniform_buffer = uniform_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, uniform_buffer->driver_id, p_name);
			}
			else if (texture_buffer_owner.owns(p_id)) {
				Buffer* texture_buffer = texture_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, texture_buffer->driver_id, p_name);
			}
			else if (storage_buffer_owner.owns(p_id)) {
				Buffer* storage_buffer = storage_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, storage_buffer->driver_id, p_name);
			}
			/*else if (instances_buffer_owner.owns(p_id)) {
				InstancesBuffer* instances_buffer = instances_buffer_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, instances_buffer->buffer.driver_id, p_name);
			}
			else if (uniform_set_owner.owns(p_id)) {
				UniformSet* uniform_set = uniform_set_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_UNIFORM_SET, uniform_set->driver_id, p_name);
			}
			else if (render_pipeline_owner.owns(p_id)) {
				RenderPipeline* pipeline = render_pipeline_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_PIPELINE, pipeline->driver_id, p_name);
			}
			else if (compute_pipeline_owner.owns(p_id)) {
				ComputePipeline* pipeline = compute_pipeline_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_PIPELINE, pipeline->driver_id, p_name);
			}
			else if (acceleration_structure_owner.owns(p_id)) {
				AccelerationStructure* acceleration_structure = acceleration_structure_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_ACCELERATION_STRUCTURE, acceleration_structure->driver_id, p_name);
			}
			else if (raytracing_pipeline_owner.owns(p_id)) {
				RaytracingPipeline* pipeline = raytracing_pipeline_owner.get_or_null(p_id);
				driver->set_object_name(RDD::OBJECT_TYPE_RAYTRACING_PIPELINE, pipeline->driver_id, p_name);
			}*/
			else {
				ERR_PRINT(std::format("Attempted to name invalid ID: {}", std::to_string(p_id.get_id())));
				return;
			}
#ifdef DEV_ENABLED
		resource_names[p_id] = p_name;
#endif
	}

	void RenderingDevice::capture_timestamp(const std::string& p_name) {			// write
		//ERR_RENDER_THREAD_GUARD();

		//ERR_FAIL_COND_MSG(draw_list.active && draw_list.state.draw_count > 0, "Capturing timestamps during draw list creation is not allowed. Offending timestamp was: " + p_name);
		//ERR_FAIL_COND_MSG(compute_list.active && compute_list.state.dispatch_count > 0, "Capturing timestamps during compute list creation is not allowed. Offending timestamp was: " + p_name);
		//ERR_FAIL_COND_MSG(raytracing_list.active && raytracing_list.state.trace_count > 0, "Capturing timestamps during raytracing list creation is not allowed. Offending timestamp was: " + p_name);
		ERR_FAIL_COND_MSG(frames[frame].timestamp_count >= max_timestamp_query_elements, std::format("Tried capturing more timestamps than the configured maximum ({}). You can increase this limit in the project settings under 'Debug/Settings' called 'Max Timestamp Query Elements'.", max_timestamp_query_elements));

		//draw_graph.add_capture_timestamp(frames[frame].timestamp_pool, frames[frame].timestamp_count);

		driver->command_timestamp_write(frames[frame].command_buffer, frames[frame].timestamp_pool, frames[frame].timestamp_count);

		frames[frame].timestamp_names[frames[frame].timestamp_count] = p_name;
		frames[frame].timestamp_cpu_values[frames[frame].timestamp_count] = Util::get_current_time_usec();
		frames[frame].timestamp_count++;
	}

	uint32_t RenderingDevice::get_captured_timestamps_count() const {
		//ERR_RENDER_THREAD_GUARD_V(0);
		return frames[frame].timestamp_result_count;
	}

	uint64_t RenderingDevice::get_captured_timestamps_frame() const {
		//ERR_RENDER_THREAD_GUARD_V(0);
		return frames[frame].index;
	}

	uint64_t RenderingDevice::get_captured_timestamp_gpu_time(uint32_t p_index) const {
		//ERR_RENDER_THREAD_GUARD_V(0);
		ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, 0);
		return driver->timestamp_query_result_to_time(frames[frame].timestamp_result_values[p_index]);
	}

	uint64_t RenderingDevice::get_captured_timestamp_cpu_time(uint32_t p_index) const {
		//ERR_RENDER_THREAD_GUARD_V(0);
		ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, 0);
		return frames[frame].timestamp_cpu_result_values[p_index];
	}

	std::string RenderingDevice::get_captured_timestamp_name(uint32_t p_index) const {
		ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, "");
		return frames[frame].timestamp_result_names[p_index];
	}

#pragma region Transfer worker

	static uint32_t _get_alignment_offset(uint32_t p_offset, uint32_t p_required_align) {
		uint32_t alignment_offset = (p_required_align > 0) ? (p_offset % p_required_align) : 0;
		if (alignment_offset != 0) {
			// If a particular alignment is required, add the offset as part of the required size.
			alignment_offset = p_required_align - alignment_offset;
		}

		return alignment_offset;
	}

	void RenderingDevice::_submit_transfer_workers(RDD::CommandBufferID p_draw_command_buffer) {
		//p_draw_command_buffer = frames[frame].command_buffer;
		std::lock_guard transfer_worker_lock(transfer_worker_pool_mutex);
		for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
			TransferWorker* worker = transfer_worker_pool[i];
			if (p_draw_command_buffer) {
				std::lock_guard lock(worker->operations_mutex);
				if (worker->operations_processed >= transfer_worker_operation_used_by_draw[worker->index]) {
					// The operation used by the draw has already been processed, we don't need to wait on the worker.
					continue;
				}
			}

			{
				std::lock_guard lock(worker->thread_mutex);
				if (worker->recording) {
					std::vector<RenderingDeviceDriver::SemaphoreID> tws{ frames[frame].transfer_worker_semaphores[i] };
					std::span<RDD::SemaphoreID> semaphores = p_draw_command_buffer ? tws : std::span<RDD::SemaphoreID>();
					_end_transfer_worker(worker);
					_submit_transfer_worker(worker, semaphores);
				}

				if (p_draw_command_buffer) {
					_flush_barriers_for_transfer_worker(worker);
				}
			}
		}
	}

	void RenderingDevice::_submit_transfer_barriers(RDD::CommandBufferID p_draw_command_buffer) {
		std::lock_guard transfer_worker_lock(transfer_worker_pool_texture_barriers_mutex);
		if (!transfer_worker_pool_texture_barriers.empty()) {
			driver->command_pipeline_barrier(p_draw_command_buffer, RDD::PIPELINE_STAGE_COPY_BIT, RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT, {}, {}, transfer_worker_pool_texture_barriers, {});
			transfer_worker_pool_texture_barriers.clear();
		}
	}

	RenderingDevice::TransferWorker* RenderingDevice::_acquire_transfer_worker(uint32_t p_transfer_size, uint32_t p_required_align, uint32_t& r_staging_offset) {
		// Find the first worker that is not currently executing anything and has enough size for the transfer.
		// If no workers are available, we make a new one. If we're not allowed to make new ones, we wait until one of them is available.
		TransferWorker* transfer_worker = nullptr;
		uint32_t available_list_index = 0;
		bool transfer_worker_busy = true;
		bool transfer_worker_full = true;
		{
			std::unique_lock<std::mutex> pool_lock(transfer_worker_pool_mutex);

			// If no workers are available and we've reached the max pool capacity, wait until one of them becomes available.
			bool transfer_worker_pool_full = transfer_worker_pool_size >= transfer_worker_pool_max_size;
			while (transfer_worker_pool_available_list.empty() && transfer_worker_pool_full) {
				transfer_worker_pool_condition.wait(pool_lock);
			}

			// Look at all available workers first.
			for (uint32_t i = 0; i < transfer_worker_pool_available_list.size(); i++) {
				uint32_t worker_index = transfer_worker_pool_available_list[i];
				TransferWorker* candidate_worker = transfer_worker_pool[worker_index];
				candidate_worker->thread_mutex.lock();

				// Figure out if the worker can fit the transfer.
				uint32_t alignment_offset = _get_alignment_offset(candidate_worker->staging_buffer_size_in_use, p_required_align);
				uint32_t required_size = candidate_worker->staging_buffer_size_in_use + p_transfer_size + alignment_offset;
				bool candidate_worker_busy = candidate_worker->submitted;
				bool candidate_worker_full = required_size > candidate_worker->staging_buffer_size_allocated;
				bool pick_candidate = false;
				if (!candidate_worker_busy && !candidate_worker_full) {
					// A worker that can fit the transfer and is not waiting for a previous execution is the best possible candidate.
					pick_candidate = true;
				}
				else if (!candidate_worker_busy) {
					// The worker can't fit the transfer but it's not currently doing anything.
					// We pick it as a possible candidate if the current one is busy.
					pick_candidate = transfer_worker_busy;
				}
				else if (!candidate_worker_full) {
					// The worker can fit the transfer but it's currently executing previous work.
					// We pick it as a possible candidate if the current one is both busy and full.
					pick_candidate = transfer_worker_busy && transfer_worker_full;
				}
				else if (transfer_worker == nullptr) {
					// The worker can't fit the transfer and it's currently executing work, so it's the worst candidate.
					// We only pick if no candidate has been picked yet.
					pick_candidate = true;
				}

				if (pick_candidate) {
					if (transfer_worker != nullptr) {
						// Release the lock for the worker that was picked previously.
						transfer_worker->thread_mutex.unlock();
					}

					// Keep the lock active for this worker.
					transfer_worker = candidate_worker;
					transfer_worker_busy = candidate_worker_busy;
					transfer_worker_full = candidate_worker_full;
					available_list_index = i;

					if (!transfer_worker_busy && !transfer_worker_full) {
						// Best possible candidate, stop searching early.
						break;
					}
				}
				else {
					// Release the lock for the candidate.
					candidate_worker->thread_mutex.unlock();
				}
			}

			if (transfer_worker != nullptr) {
				// A worker was picked, remove it from the available list.
				transfer_worker_pool_available_list.erase(transfer_worker_pool_available_list.begin() + available_list_index);
			}
			else {
				DEBUG_ASSERT(!transfer_worker_pool_full && "A transfer worker should never be created when the pool is full.");

				// No existing worker was picked, we create a new one.
				uint32_t transfer_worker_index = transfer_worker_pool_size;
				++transfer_worker_pool_size;

				transfer_worker = new TransferWorker;
				transfer_worker->command_fence = driver->fence_create();
				transfer_worker->command_pool = driver->command_pool_create(transfer_queue_family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
				transfer_worker->command_buffer = driver->command_buffer_create(transfer_worker->command_pool);
				transfer_worker->index = transfer_worker_index;
				transfer_worker_pool[transfer_worker_index] = transfer_worker;
				transfer_worker_operation_used_by_draw[transfer_worker_index] = 0;
				transfer_worker->thread_mutex.lock();
			}
		}

		if (transfer_worker->submitted) {
			// Wait for the worker if the command buffer was submitted but it hasn't finished processing yet.
			_wait_for_transfer_worker(transfer_worker);
		}

		uint32_t alignment_offset = _get_alignment_offset(transfer_worker->staging_buffer_size_in_use, p_required_align);
		transfer_worker->max_transfer_size = MAX(transfer_worker->max_transfer_size, p_transfer_size);

		uint32_t required_size = transfer_worker->staging_buffer_size_in_use + p_transfer_size + alignment_offset;
		if (required_size > transfer_worker->staging_buffer_size_allocated) {
			// If there's not enough bytes to use on the staging buffer, we submit everything pending from the worker and wait for the work to be finished.
			if (transfer_worker->recording) {
				_end_transfer_worker(transfer_worker);
				_submit_transfer_worker(transfer_worker);
			}

			if (transfer_worker->submitted) {
				_wait_for_transfer_worker(transfer_worker);
			}

			alignment_offset = 0;

			// If the staging buffer can't fit the transfer, we recreate the buffer.
			const uint32_t expected_buffer_size_minimum = 16 * 1024;
			uint32_t expected_buffer_size = MAX(transfer_worker->max_transfer_size, expected_buffer_size_minimum);
			if (expected_buffer_size > transfer_worker->staging_buffer_size_allocated) {
				if (transfer_worker->staging_buffer.id != 0) {
					driver->buffer_free(transfer_worker->staging_buffer);
				}

				uint32_t new_staging_buffer_size = glm::ceilPowerOfTwo(expected_buffer_size);
				transfer_worker->staging_buffer_size_allocated = new_staging_buffer_size;
				transfer_worker->staging_buffer = driver->buffer_create(new_staging_buffer_size, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
			}
		}

		// Add the alignment before storing the offset that will be returned.
		transfer_worker->staging_buffer_size_in_use += alignment_offset;

		// Store the offset to return and increment the current size.
		r_staging_offset = transfer_worker->staging_buffer_size_in_use;
		transfer_worker->staging_buffer_size_in_use += p_transfer_size;

		if (!transfer_worker->recording) {
			// Begin the command buffer if the worker wasn't recording yet.
			driver->command_buffer_begin(transfer_worker->command_buffer);
			transfer_worker->recording = true;
		}

		return transfer_worker;
	}

	void RenderingDevice::_release_transfer_worker(TransferWorker* p_transfer_worker) {
		p_transfer_worker->thread_mutex.unlock();

		transfer_worker_pool_mutex.lock();
		transfer_worker_pool_available_list.push_back(p_transfer_worker->index);
		transfer_worker_pool_mutex.unlock();
		transfer_worker_pool_condition.notify_one();
	}

	void RenderingDevice::_end_transfer_worker(TransferWorker* p_transfer_worker) {
		driver->command_buffer_end(p_transfer_worker->command_buffer);
		p_transfer_worker->recording = false;
	}

	void RenderingDevice::_submit_transfer_worker(TransferWorker* p_transfer_worker, std::span<RDD::SemaphoreID> p_signal_semaphores /*= std::span<RDD::SemaphoreID>()*/)
	{
		driver->command_queue_execute_and_present(transfer_queue, {}, { &p_transfer_worker->command_buffer, 1 }, p_signal_semaphores, p_transfer_worker->command_fence, {});

		for (uint32_t i = 0; i < p_signal_semaphores.size(); i++) {
			// Indicate the frame should wait on these semaphores before executing the main command buffer.
			frames[frame].semaphores_to_wait_on.push_back(p_signal_semaphores[i]);
		}

		p_transfer_worker->submitted = true;

		{
			std::lock_guard lock(p_transfer_worker->operations_mutex);
			p_transfer_worker->operations_submitted = p_transfer_worker->operations_counter;
		}
	}

	void RenderingDevice::_wait_for_transfer_worker(TransferWorker* p_transfer_worker) {
		driver->fence_wait(p_transfer_worker->command_fence);
		driver->command_pool_reset(p_transfer_worker->command_pool);
		p_transfer_worker->staging_buffer_size_in_use = 0;
		p_transfer_worker->submitted = false;

		{
			std::lock_guard lock(p_transfer_worker->operations_mutex);
			p_transfer_worker->operations_processed = p_transfer_worker->operations_submitted;
		}

		_flush_barriers_for_transfer_worker(p_transfer_worker);
	}

	void RenderingDevice::_flush_barriers_for_transfer_worker(TransferWorker* p_transfer_worker) {
		// Caller must have already acquired the mutex for the worker.
		if (!p_transfer_worker->texture_barriers.empty()) {
			std::lock_guard transfer_worker_lock(transfer_worker_pool_texture_barriers_mutex);
			for (uint32_t i = 0; i < p_transfer_worker->texture_barriers.size(); i++) {
				transfer_worker_pool_texture_barriers.push_back(p_transfer_worker->texture_barriers[i]);
			}

			p_transfer_worker->texture_barriers.clear();
		}
	}

	void RenderingDevice::_check_transfer_worker_operation(uint32_t p_transfer_worker_index, uint64_t p_transfer_worker_operation) {
		TransferWorker* transfer_worker = transfer_worker_pool[p_transfer_worker_index];
		std::lock_guard lock(transfer_worker->operations_mutex);
		uint64_t& dst_operation = transfer_worker_operation_used_by_draw[transfer_worker->index];
		dst_operation = MAX(dst_operation, p_transfer_worker_operation);
	}

	void RenderingDevice::_check_transfer_worker_buffer(Buffer* p_buffer) {
		if (p_buffer->transfer_worker_index >= 0) {
			_check_transfer_worker_operation(p_buffer->transfer_worker_index, p_buffer->transfer_worker_operation);
			p_buffer->transfer_worker_index = -1;
		}
	}

	void RenderingDevice::_check_transfer_worker_texture(Texture* p_texture)
	{
		if (p_texture->transfer_worker_index >= 0) {
			_check_transfer_worker_operation(p_texture->transfer_worker_index, p_texture->transfer_worker_operation);
			p_texture->transfer_worker_index = -1;
		}
	}

	void RenderingDevice::_check_transfer_worker_vertex_array(VertexArray* p_vertex_array) {
		if (!p_vertex_array->transfer_worker_indices.empty()) {
			for (int i = 0; i < p_vertex_array->transfer_worker_indices.size(); i++) {
				_check_transfer_worker_operation(p_vertex_array->transfer_worker_indices[i], p_vertex_array->transfer_worker_operations[i]);
			}

			p_vertex_array->transfer_worker_indices.clear();
			p_vertex_array->transfer_worker_operations.clear();
		}
	}

	void RenderingDevice::_check_transfer_worker_index_array(IndexArray* p_index_array) {
		if (p_index_array->transfer_worker_index >= 0) {
			_check_transfer_worker_operation(p_index_array->transfer_worker_index, p_index_array->transfer_worker_operation);
			p_index_array->transfer_worker_index = -1;
		}
	}

	void RenderingDevice::_wait_for_transfer_workers() {
		std::lock_guard transfer_worker_lock(transfer_worker_pool_mutex);
		for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
			TransferWorker* worker = transfer_worker_pool[i];
			std::lock_guard lock(worker->thread_mutex);
			if (worker->submitted) {
				_wait_for_transfer_worker(worker);
			}
		}
	}

	void RenderingDevice::_free_transfer_workers() {
		std::lock_guard transfer_worker_lock(transfer_worker_pool_mutex);
		for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
			TransferWorker* worker = transfer_worker_pool[i];
			driver->fence_free(worker->command_fence);
			driver->buffer_free(worker->staging_buffer);
			driver->command_pool_free(worker->command_pool);
			delete worker;
		}

		transfer_worker_pool_size = 0;
	}

#pragma endregion

	void RenderingDevice::_free_dependencies_of(RID p_id) {
		auto RE = reverse_dependency_map.find(p_id);
		if (RE == reverse_dependency_map.end()) {
			return;
		}

		// Snapshot the set to avoid iterator invalidation during recursive frees
		std::unordered_set<RID> dependents = RE->second;
		reverse_dependency_map.erase(RE); // erase early to prevent re-entrancy issues

		for (RID dep : dependents) {
			auto G = dependency_map.find(dep);
			if (G != dependency_map.end()) {
				G->second.erase(p_id);
			}
			free_rid(dep);
		}
	}

	void RenderingDevice::free_rid(RID p_rid)
	{
		_free_dependencies(p_rid); // Recursively erase dependencies first, to avoid potential API problems.
		_free_internal(p_rid);
	}

	bool RenderingDevice::_buffer_make_mutable(Buffer* p_buffer, RID p_buffer_id) {
		//TODO:
		return true;
	}

	uint32_t RenderingDevice::_get_swap_chain_desired_count() const {
		return 2;
	}

	Rendering::RenderingDevice::Buffer* RenderingDevice::_get_buffer_from_owner(RID p_buffer)
	{
		Buffer* buffer = nullptr;
		if (vertex_buffer_owner.owns(p_buffer)) {
			buffer = vertex_buffer_owner.get_or_null(p_buffer);
		}
		else if (index_buffer_owner.owns(p_buffer)) {
			buffer = index_buffer_owner.get_or_null(p_buffer);
		}
		else if (uniform_buffer_owner.owns(p_buffer)) {
			buffer = uniform_buffer_owner.get_or_null(p_buffer);
		}
		//else if (texture_buffer_owner.owns(p_buffer)) {
		//	DEV_ASSERT(false && "FIXME: Broken.");
		//	//buffer = texture_buffer_owner.get_or_null(p_buffer)->buffer;
		//}
		//else if (storage_buffer_owner.owns(p_buffer)) {
		//	buffer = storage_buffer_owner.get_or_null(p_buffer);
		//}
		//else if (instances_buffer_owner.owns(p_buffer)) {
		//	buffer = &instances_buffer_owner.get_or_null(p_buffer)->buffer;
		//}
		return buffer;
	}

	Error RenderingDevice::_buffer_initialize(Buffer* p_buffer, std::span<uint8_t> p_data, uint32_t p_required_align /*= 32*/)
	{
		uint32_t transfer_worker_offset;
		TransferWorker* transfer_worker = _acquire_transfer_worker(p_data.size(), p_required_align, transfer_worker_offset);
		p_buffer->transfer_worker_index = transfer_worker->index;

		{
			std::lock_guard lock(transfer_worker->operations_mutex);
			p_buffer->transfer_worker_operation = ++transfer_worker->operations_counter;
		}

		// Copy to the worker's staging buffer.
		uint8_t* data_ptr = driver->buffer_map(transfer_worker->staging_buffer);
		ERR_FAIL_NULL_V(data_ptr, ERR_CANT_CREATE);

		memcpy(data_ptr + transfer_worker_offset, p_data.data(), p_data.size());
		driver->buffer_unmap(transfer_worker->staging_buffer);

		// Copy from the staging buffer to the real buffer.
		RDD::BufferCopyRegion region;
		region.src_offset = transfer_worker_offset;
		region.dst_offset = 0;
		region.size = p_data.size();
		driver->command_copy_buffer(transfer_worker->command_buffer, transfer_worker->staging_buffer, p_buffer->driver_id, { &region, 1 });

		_release_transfer_worker(transfer_worker);

		return OK;
	}

	uint32_t RenderingDevice::_texture_layer_count(Texture* p_texture) const
	{
		switch (p_texture->type) {
		case TEXTURE_TYPE_CUBE:
		case TEXTURE_TYPE_CUBE_ARRAY:
			return p_texture->layers * 6;
		default:
			return p_texture->layers;
		}
	}

	uint32_t RenderingDevice::_texture_alignment(Texture* p_texture) const
	{
		uint32_t alignment = get_compressed_image_format_block_byte_size(p_texture->format);
		if (alignment == 1) {
			alignment = get_image_format_pixel_size(p_texture->format);
		}

		return least_common_multiple(alignment, driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT));
	}

	Error RenderingDevice::_texture_initialize(RID p_texture, uint32_t p_layer, const std::vector<uint8_t>& p_data, RDD::TextureLayout p_dst_layout, bool p_immediate_flush)
	{
		Texture* texture = texture_owner.get_or_null(p_texture);
		ERR_FAIL_NULL_V(texture, ERR_INVALID_PARAMETER);

		if (texture->owner != RID()) {
			p_texture = texture->owner;
			texture = texture_owner.get_or_null(texture->owner);
			ERR_FAIL_NULL_V(texture, ERR_BUG); // This is a bug.
		}

		uint32_t layer_count = _texture_layer_count(texture);
		ERR_FAIL_COND_V(p_layer >= layer_count, ERR_INVALID_PARAMETER);

		uint32_t width, height;
		uint32_t tight_mip_size = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, texture->mipmaps, &width, &height);
		uint32_t required_size = tight_mip_size;
		uint32_t required_align = _texture_alignment(texture);

		ERR_FAIL_COND_V_MSG(required_size != (uint32_t)p_data.size(), ERR_INVALID_PARAMETER,
			std::format("Required size for texture update ({}) does not match data supplied size ({}).", required_size, p_data.size()));

		uint32_t block_w, block_h;
		get_compressed_image_format_block_dimensions(texture->format, block_w, block_h);

		uint32_t pixel_size = get_image_format_pixel_size(texture->format);
		uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(texture->format);
		uint32_t block_size = get_compressed_image_format_block_byte_size(texture->format);

		// The algorithm operates on two passes, one to figure out the total size the staging buffer will require to allocate and another one where the copy is actually performed.
		uint32_t staging_worker_offset = 0;
		uint32_t staging_local_offset = 0;
		TransferWorker* transfer_worker = nullptr;
		const uint8_t* read_ptr = p_data.data();
		uint8_t* write_ptr = nullptr;
		for (uint32_t pass = 0; pass < 2; pass++) {
			const bool copy_pass = (pass == 1);
			if (copy_pass) {
				transfer_worker = _acquire_transfer_worker(staging_local_offset, required_align, staging_worker_offset);
				texture->transfer_worker_index = transfer_worker->index;

				{
					std::lock_guard lock(transfer_worker->operations_mutex);
					texture->transfer_worker_operation = ++transfer_worker->operations_counter;
				}

				staging_local_offset = 0;

				write_ptr = driver->buffer_map(transfer_worker->staging_buffer);
				ERR_FAIL_NULL_V(write_ptr, ERR_CANT_CREATE);

				if (driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
					// Transition the texture to the optimal layout.
					RDD::TextureBarrier tb;
					tb.texture = texture->driver_id;
					tb.dst_access = RDD::BARRIER_ACCESS_COPY_WRITE_BIT;
					tb.prev_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
					tb.next_layout = p_dst_layout;
					tb.subresources.aspect = texture->barrier_aspect_flags;
					tb.subresources.mipmap_count = texture->mipmaps;
					tb.subresources.base_layer = p_layer;
					tb.subresources.layer_count = 1;
					driver->command_pipeline_barrier(transfer_worker->command_buffer, RDD::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, RDD::PIPELINE_STAGE_COPY_BIT, {}, {}, { &tb, 1 }, {});
				}
			}

			uint32_t mipmap_offset = 0;
			uint32_t logic_width = texture->width;
			uint32_t logic_height = texture->height;
			for (uint32_t mm_i = 0; mm_i < texture->mipmaps; mm_i++) {
				uint32_t depth = 0;
				uint32_t image_total = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, mm_i + 1, &width, &height, &depth);

				const uint8_t* read_ptr_mipmap = read_ptr + mipmap_offset;
				tight_mip_size = image_total - mipmap_offset;

				for (uint32_t z = 0; z < depth; z++) {
					if (required_align > 0) {
						uint32_t align_offset = staging_local_offset % required_align;
						if (align_offset != 0) {
							staging_local_offset += required_align - align_offset;
						}
					}

					uint32_t pitch = (width * pixel_size * block_w) >> pixel_rshift;
					uint32_t pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
					pitch = STEPIFY(pitch, pitch_step);
					uint32_t to_allocate = pitch * height;
					to_allocate >>= pixel_rshift;

					if (copy_pass) {
						const uint8_t* read_ptr_mipmap_layer = read_ptr_mipmap + (tight_mip_size / depth) * z;
						uint64_t staging_buffer_offset = staging_worker_offset + staging_local_offset;
						uint8_t* write_ptr_mipmap_layer = write_ptr + staging_buffer_offset;
						_copy_region_block_or_regular(read_ptr_mipmap_layer, write_ptr_mipmap_layer, 0, 0, width, width, height, block_w, block_h, pitch, pixel_size, block_size);

						RDD::BufferTextureCopyRegion copy_region;
						copy_region.buffer_offset = staging_buffer_offset;
						copy_region.row_pitch = pitch;
						copy_region.texture_subresource.aspect = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
						copy_region.texture_subresource.mipmap = mm_i;
						copy_region.texture_subresource.layer = p_layer;
						copy_region.texture_offset = Vector3i(0, 0, z);
						copy_region.texture_region_size = Vector3i(logic_width, logic_height, 1);
						driver->command_copy_buffer_to_texture(transfer_worker->command_buffer, transfer_worker->staging_buffer, texture->driver_id, p_dst_layout, { &copy_region, 1 });
					}

					staging_local_offset += to_allocate;
				}

				mipmap_offset = image_total;
				logic_width = MAX(1u, logic_width >> 1);
				logic_height = MAX(1u, logic_height >> 1);
			}

			if (copy_pass) {
				driver->buffer_unmap(transfer_worker->staging_buffer);

				// If the texture does not have a tracker, it means it must be transitioned to the sampling state.
				// TODO: re draw_tracker
				if (/*texture->draw_tracker == nullptr &&*/ driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
					RDD::TextureBarrier tb;
					tb.texture = texture->driver_id;
					tb.src_access = RDD::BARRIER_ACCESS_COPY_WRITE_BIT;
					tb.prev_layout = p_dst_layout;
					tb.next_layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					tb.subresources.aspect = texture->barrier_aspect_flags;
					tb.subresources.mipmap_count = texture->mipmaps;
					tb.subresources.base_layer = p_layer;
					tb.subresources.layer_count = 1;
					transfer_worker->texture_barriers.push_back(tb);
				}

				if (p_immediate_flush) {
					_end_transfer_worker(transfer_worker);
					_submit_transfer_worker(transfer_worker);
					_wait_for_transfer_worker(transfer_worker);
				}

				_release_transfer_worker(transfer_worker);
			}
		}

		return OK;
	}

	void RenderingDevice::_texture_update_shared_fallback(RID p_texture_rid, Texture* p_texture, bool p_for_writing)
	{
		if (p_texture->shared_fallback == nullptr) {
			// This texture does not use any of the shared texture fallbacks.
			return;
		}

		if (p_texture->owner.is_valid()) {
			Texture* owner_texture = texture_owner.get_or_null(p_texture->owner);
			ERR_FAIL_NULL(owner_texture);
			if (p_for_writing) {
				// Only the main texture is used for writing when using the shared fallback.
				owner_texture->shared_fallback->revision++;
			}
			else if (p_texture->shared_fallback->revision != owner_texture->shared_fallback->revision) {
				// Copy the contents of the main texture into the shared texture fallback slice. Update the revision.
				_texture_copy_shared(p_texture->owner, owner_texture, p_texture_rid, p_texture);
				p_texture->shared_fallback->revision = owner_texture->shared_fallback->revision;
			}
		}
		else if (p_for_writing) {
			// Increment the revision of the texture so shared texture fallback slices must be updated.
			p_texture->shared_fallback->revision++;
		}
	}

	void RenderingDevice::_texture_free_shared_fallback(Texture* p_texture)
	{
		if (p_texture->shared_fallback != nullptr) {
			//if (p_texture->shared_fallback->texture_tracker != nullptr) {
			//	RDG::resource_tracker_free(p_texture->shared_fallback->texture_tracker);
			//}

			//if (p_texture->shared_fallback->buffer_tracker != nullptr) {
			//	RDG::resource_tracker_free(p_texture->shared_fallback->buffer_tracker);
			//}

			if (p_texture->shared_fallback->texture.id != 0) {
				texture_memory -= driver->texture_get_allocation_size(p_texture->shared_fallback->texture);
				driver->texture_free(p_texture->shared_fallback->texture);
			}

			if (p_texture->shared_fallback->buffer.id != 0) {
				buffer_memory -= driver->buffer_get_allocation_size(p_texture->shared_fallback->buffer);
				driver->buffer_free(p_texture->shared_fallback->buffer);
			}

			delete p_texture->shared_fallback;
			p_texture->shared_fallback = nullptr;
		}
	}

	void RenderingDevice::_texture_copy_shared(RID p_src_texture_rid, Texture* p_src_texture, RID p_dst_texture_rid, Texture* p_dst_texture)
	{
		// The only type of copying allowed is from the main texture to the slice texture, as slice textures are not allowed to be used for writing when using this fallback.
		DEV_ASSERT(p_src_texture != nullptr);
		DEV_ASSERT(p_dst_texture != nullptr);
		DEV_ASSERT(p_src_texture->owner.is_null());
		DEV_ASSERT(p_dst_texture->owner == p_src_texture_rid);

		//bool src_made_mutable = _texture_make_mutable(p_src_texture, p_src_texture_rid);
		//bool dst_made_mutable = _texture_make_mutable(p_dst_texture, p_dst_texture_rid);
		//if (src_made_mutable || dst_made_mutable) {
		//	draw_graph.add_synchronization();
		//}

		if (p_dst_texture->shared_fallback->raw_reinterpretation) {
			// If one of the textures is a main texture and they have a reinterpret buffer, we prefer using that as it's guaranteed to be big enough to hold
			// anything and it's how the shared textures that don't use slices are created.
			bool src_has_buffer = p_src_texture->shared_fallback->buffer.id != 0;
			bool dst_has_buffer = p_dst_texture->shared_fallback->buffer.id != 0;
			bool from_src = p_src_texture->owner.is_null() && src_has_buffer;
			bool from_dst = p_dst_texture->owner.is_null() && dst_has_buffer;
			if (!from_src && !from_dst) {
				// If neither texture passed the condition, we just pick whichever texture has a reinterpretation buffer.
				from_src = src_has_buffer;
				from_dst = dst_has_buffer;
			}

			// Pick the buffer and tracker to use from the right texture.
			RDD::BufferID shared_buffer;
			//RDG::ResourceTracker* shared_buffer_tracker = nullptr;
			if (from_src) {
				shared_buffer = p_src_texture->shared_fallback->buffer;
				//shared_buffer_tracker = p_src_texture->shared_fallback->buffer_tracker;
			}
			else if (from_dst) {
				shared_buffer = p_dst_texture->shared_fallback->buffer;
				//shared_buffer_tracker = p_dst_texture->shared_fallback->buffer_tracker;
			}
			else {
				DEV_ASSERT(false && "This path should not be reachable.");
			}

			// Copying each mipmap from main texture to a buffer and then to the slice texture.
			thread_local std::vector<RDD::BufferTextureCopyRegion> get_data_vector;
			thread_local std::vector<RecordedBufferToTextureCopy> update_vector;
			get_data_vector.clear();
			update_vector.clear();

			uint32_t buffer_size = 0;
			uint32_t transfer_alignment = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);

			for (uint32_t i = 0; i < p_dst_texture->layers; i++) {
				for (uint32_t j = 0; j < p_dst_texture->mipmaps; j++) {
					// FIXME: When using reinterpretation buffers, the only texture aspect supported is color. Depth or stencil contents won't get copied.
					RDD::TextureSubresource texture_subresource;
					texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
					texture_subresource.layer = i;
					texture_subresource.mipmap = j;

					RDD::TextureCopyableLayout copyable_layout;
					driver->texture_get_copyable_layout(p_dst_texture->shared_fallback->texture, texture_subresource, &copyable_layout);

					uint32_t mipmap = p_dst_texture->base_mipmap + j;

					RDD::BufferTextureCopyRegion get_data_region;
					get_data_region.buffer_offset = STEPIFY(buffer_size, transfer_alignment);
					get_data_region.row_pitch = copyable_layout.row_pitch;
					get_data_region.texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
					get_data_region.texture_subresource.layer = p_dst_texture->base_layer + i;
					get_data_region.texture_subresource.mipmap = mipmap;
					get_data_region.texture_region_size.x = MAX(1U, p_src_texture->width >> mipmap);
					get_data_region.texture_region_size.y = MAX(1U, p_src_texture->height >> mipmap);
					get_data_region.texture_region_size.z = MAX(1U, p_src_texture->depth >> mipmap);
					get_data_vector.push_back(get_data_region);

					RecordedBufferToTextureCopy update_copy;
					update_copy.from_buffer = shared_buffer;
					update_copy.region.buffer_offset = get_data_region.buffer_offset;
					update_copy.region.row_pitch = get_data_region.row_pitch;
					update_copy.region.texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
					update_copy.region.texture_subresource.layer = texture_subresource.layer;
					update_copy.region.texture_subresource.mipmap = texture_subresource.mipmap;
					update_copy.region.texture_region_size.x = get_data_region.texture_region_size.x;
					update_copy.region.texture_region_size.y = get_data_region.texture_region_size.y;
					update_copy.region.texture_region_size.z = get_data_region.texture_region_size.z;
					update_vector.push_back(update_copy);

					buffer_size = get_data_region.buffer_offset + copyable_layout.size;
				}
			}

			DEV_ASSERT(buffer_size <= driver->buffer_get_allocation_size(shared_buffer));

			driver->command_copy_texture_to_buffer(get_current_command_buffer(), p_src_texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL, shared_buffer, get_data_vector);

			for (auto& tex_c : update_vector)
			{
				driver->command_copy_buffer_to_texture(get_current_command_buffer(), tex_c.from_buffer, p_dst_texture->shared_fallback->texture, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, { &tex_c.region, 1 });
			}
		}
		else {
			// Raw reinterpretation is not required. Use a regular texture copy.
			RDD::TextureCopyRegion copy_region;
			copy_region.src_subresources.aspect = p_src_texture->read_aspect_flags;
			copy_region.src_subresources.base_layer = p_dst_texture->base_layer;
			copy_region.src_subresources.layer_count = p_dst_texture->layers;
			copy_region.dst_subresources.aspect = p_dst_texture->read_aspect_flags;
			copy_region.dst_subresources.base_layer = 0;
			copy_region.dst_subresources.layer_count = copy_region.src_subresources.layer_count;

			// Copying each mipmap from main texture to to the slice texture.
			thread_local std::vector<RDD::TextureCopyRegion> region_vector;
			region_vector.clear();
			for (uint32_t i = 0; i < p_dst_texture->mipmaps; i++) {
				uint32_t mipmap = p_dst_texture->base_mipmap + i;
				copy_region.src_subresources.mipmap = mipmap;
				copy_region.dst_subresources.mipmap = i;
				copy_region.size.x = MAX(1U, p_src_texture->width >> mipmap);
				copy_region.size.y = MAX(1U, p_src_texture->height >> mipmap);
				copy_region.size.z = MAX(1U, p_src_texture->depth >> mipmap);
				region_vector.push_back(copy_region);
			}

			//draw_graph.add_texture_copy(p_src_texture->driver_id, p_src_texture->draw_tracker, p_dst_texture->shared_fallback->texture, p_dst_texture->shared_fallback->texture_tracker, region_vector);
			driver->command_copy_texture(get_current_command_buffer(), p_src_texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL, p_dst_texture->shared_fallback->texture, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, region_vector);
		}
	}

	void RenderingDevice::_texture_check_pending_clear(RID p_texture_rid, Texture* p_texture)
	{
		DEV_ASSERT(p_texture != nullptr);

		if (!p_texture->pending_clear) {
			return;
		}

		bool clear = true;
		p_texture->pending_clear = false;

		if (p_texture->owner.is_valid()) {
			// Check the owner texture instead if it exists.
			p_texture_rid = p_texture->owner;
			p_texture = texture_owner.get_or_null(p_texture_rid);
			clear = p_texture->pending_clear;
		}

		if (p_texture != nullptr && clear) {
			if (p_texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				_texture_clear_depth_stencil(p_texture_rid, p_texture, 0.0f, 0, 0, p_texture->mipmaps, 0, p_texture->layers);
			}
			else {
				_texture_clear_color(p_texture_rid, p_texture, Color(), 0, p_texture->mipmaps, 0, p_texture->layers);
			}
			p_texture->pending_clear = false;
		}
	}

	void RenderingDevice::_texture_clear_color(RID p_texture_rid, Texture* p_texture, const Color& p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers)
	{
		_check_transfer_worker_texture(p_texture);

		RDD::TextureSubresourceRange range;
		range.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
		range.base_mipmap = p_texture->base_mipmap + p_base_mipmap;
		range.mipmap_count = p_mipmaps;
		range.base_layer = p_texture->base_layer + p_base_layer;
		range.layer_count = p_layers;

		// Indicate the texture will get modified for the shared texture fallback.
		_texture_update_shared_fallback(p_texture_rid, p_texture, true);

		//if (_texture_make_mutable(p_texture, p_texture_rid)) {
		//	// The texture must be mutable to be used as a clear destination.
		//	draw_graph.add_synchronization();
		//}

		//draw_graph.add_texture_clear_color(p_texture->driver_id, p_texture->draw_tracker, p_color, range);
		driver->command_clear_color_texture(get_current_command_buffer(), p_texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, p_color, range);
	}

	void RenderingDevice::_texture_clear_depth_stencil(RID p_texture_rid, Texture* p_texture, float p_depth, uint8_t p_stencil, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers)
	{
		_check_transfer_worker_texture(p_texture);

		RDD::TextureSubresourceRange range;
		if (format_has_depth(p_texture->format)) {
			range.aspect.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		}
		if (format_has_stencil(p_texture->format)) {
			range.aspect.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
		}
		range.base_mipmap = p_texture->base_mipmap + p_base_mipmap;
		range.mipmap_count = p_mipmaps;
		range.base_layer = p_texture->base_layer + p_base_layer;
		range.layer_count = p_layers;

		// Indicate the texture will get modified for the shared texture fallback.
		_texture_update_shared_fallback(p_texture_rid, p_texture, true);

		//if (_texture_make_mutable(p_texture, p_texture_rid)) {
		//	// The texture must be mutable to be used as a clear destination.
		//	draw_graph.add_synchronization();
		//}

		//draw_graph.add_texture_clear_depth_stencil(p_texture->driver_id, p_texture->draw_tracker, p_depth, p_stencil, range);
		driver->command_clear_depth_stencil_texture(get_current_command_buffer(), p_texture->driver_id, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, p_depth, p_stencil, range);
	}

	uint32_t RenderingDevice::_texture_vrs_method_to_usage_bits() const
	{
		switch (vrs_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDD::TEXTURE_USAGE_VRS_FRAGMENT_SHADING_RATE_BIT;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDD::TEXTURE_USAGE_VRS_FRAGMENT_DENSITY_MAP_BIT;
		default:
			return 0;
		}
	}

	std::vector<uint8_t> RenderingDevice::_load_pipeline_cache()
	{
		// TODO
		return {};
	}

	void RenderingDevice::_save_pipeline_cache(void* p_data)
	{

	}

	Error RenderingDevice::_staging_buffer_allocate(StagingBuffers& p_staging_buffers, uint32_t p_amount, uint32_t p_required_align, uint32_t& r_alloc_offset, uint32_t& r_alloc_size, StagingRequiredAction& r_required_action, bool p_can_segment /*= true*/)
	{
		r_alloc_size = p_amount;
		r_required_action = STAGING_REQUIRED_ACTION_NONE;

		//LOGI(std::format("[Staging] Request: {} bytes  | Current block fill: {} / {}  | Total blocks: {}  | Max size: {}",
		//	p_amount, p_staging_buffers.blocks[p_staging_buffers.current].fill_amount, p_staging_buffers.block_size,
		//	p_staging_buffers.blocks.size(), p_staging_buffers.max_size).c_str());

		while (true) {
			r_alloc_offset = 0;

			// See if we can use current block.
			if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used == frames_drawn) {
				// We used this block this frame, let's see if there is still room.

				uint32_t write_from = p_staging_buffers.blocks[p_staging_buffers.current].fill_amount;

				{
					uint32_t align_remainder = write_from % p_required_align;
					if (align_remainder != 0) {
						write_from += p_required_align - align_remainder;
					}
				}

				int32_t available_bytes = int32_t(p_staging_buffers.block_size) - int32_t(write_from);

				if ((int32_t)p_amount < available_bytes) {
					// All is good, we should be ok, all will fit.
					r_alloc_offset = write_from;
				}
				else if (p_can_segment && available_bytes >= (int32_t)p_required_align) {
					// Ok all won't fit but at least we can fit a chunkie.
					// All is good, update what needs to be written to.
					r_alloc_offset = write_from;
					r_alloc_size = available_bytes - (available_bytes % p_required_align);

				}
				else {
					// Can't fit it into this buffer.
					// Will need to try next buffer.

					p_staging_buffers.current = (p_staging_buffers.current + 1) % p_staging_buffers.blocks.size();

					// Before doing anything, though, let's check that we didn't manage to fill all blocks.
					// Possible in a single frame.
					if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used == frames_drawn) {
						// Guess we did.. ok, let's see if we can insert a new block.
						if ((uint64_t)p_staging_buffers.blocks.size() * p_staging_buffers.block_size < p_staging_buffers.max_size) {
							// We can, so we are safe.
							Error err = _insert_staging_block(p_staging_buffers);
							if (err) {
								return err;
							}
							// Claim for this frame.
							p_staging_buffers.blocks[p_staging_buffers.current].frame_used = frames_drawn;
						}
						else {
							// Ok, worst case scenario, all the staging buffers belong to this frame
							// and this frame is not even done.
							// If this is the main thread, it means the user is likely loading a lot of resources at once,.
							// Otherwise, the thread should just be blocked until the next frame (currently unimplemented).
							r_required_action = STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL;
						}

					}
					else {
						// Not from current frame, so continue and try again.
						continue;
					}
				}

			}
			else if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used <= frames_drawn - frames.size()) {
				// This is an old block, which was already processed, let's reuse.
				p_staging_buffers.blocks[p_staging_buffers.current].frame_used = frames_drawn;
				p_staging_buffers.blocks[p_staging_buffers.current].fill_amount = 0;
			}
			else {
				// This block may still be in use, let's not touch it unless we have to, so.. can we create a new one?
				if ((uint64_t)p_staging_buffers.blocks.size() * p_staging_buffers.block_size < p_staging_buffers.max_size) {
					// We are still allowed to create a new block, so let's do that and insert it for current pos.
					Error err = _insert_staging_block(p_staging_buffers);
					if (err) {
						return err;
					}
					// Claim for this frame.
					p_staging_buffers.blocks[p_staging_buffers.current].frame_used = frames_drawn;
				}
				else {
					// Oops, we are out of room and we can't create more.
					// Let's flush older frames.
					// The logic here is that if a game is loading a lot of data from the main thread, it will need to be stalled anyway.
					// If loading from a separate thread, we can block that thread until next frame when more room is made (not currently implemented, though).
					r_required_action = STAGING_REQUIRED_ACTION_STALL_PREVIOUS;
				}
			}

			// All was good, break.
			break;
		}

		p_staging_buffers.used = true;

		return OK;
	}

	void RenderingDevice::_staging_buffer_execute_required_action(StagingBuffers& p_staging_buffers, StagingRequiredAction p_required_action)
	{
		switch (p_required_action) {
		case STAGING_REQUIRED_ACTION_NONE: {
			// Do nothing.
		} break;
		case STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL: {
			_flush_and_stall_for_all_frames();

			// Clear the whole staging buffer.
			for (int i = 0; i < p_staging_buffers.blocks.size(); i++) {
				p_staging_buffers.blocks[i].frame_used = 0;
				p_staging_buffers.blocks[i].fill_amount = 0;
			}

			// Claim for current frame.
			p_staging_buffers.blocks[p_staging_buffers.current].frame_used = frames_drawn;
		} break;
		case STAGING_REQUIRED_ACTION_STALL_PREVIOUS: {
			_stall_for_previous_frames();

			for (int i = 0; i < p_staging_buffers.blocks.size(); i++) {
				// Clear all blocks but the ones from this frame.
				int block_idx = (i + p_staging_buffers.current) % p_staging_buffers.blocks.size();
				if (p_staging_buffers.blocks[block_idx].frame_used == frames_drawn) {
					break; // Ok, we reached something from this frame, abort.
				}

				p_staging_buffers.blocks[block_idx].frame_used = 0;
				p_staging_buffers.blocks[block_idx].fill_amount = 0;
			}

			// Claim for current frame.
			p_staging_buffers.blocks[p_staging_buffers.current].frame_used = frames_drawn;
		} break;
		default: {
			DEV_ASSERT(false && "Unknown required action.");
		} break;
		}
	}

	Error RenderingDevice::_insert_staging_block(StagingBuffers& p_staging_buffers)
	{
		StagingBufferBlock block;

		block.driver_id = driver->buffer_create(p_staging_buffers.block_size, p_staging_buffers.usage_bits, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
		ERR_FAIL_COND_V(!block.driver_id, ERR_CANT_CREATE);

		block.frame_used = 0;
		block.fill_amount = 0;
		block.data_ptr = driver->buffer_map(block.driver_id);

		if (block.data_ptr == nullptr) {
			driver->buffer_free(block.driver_id);
			return ERR_CANT_CREATE;
		}

		if (p_staging_buffers.current >= p_staging_buffers.blocks.size())
		{
			p_staging_buffers.blocks.push_back(block);
			p_staging_buffers.current++;
			return OK;
		}

		p_staging_buffers.blocks[p_staging_buffers.current] = block;
		
		return OK;
	}

	RDD::TextureLayout RenderingDevice::_vrs_layout_from_method(VRSMethod p_method) {
		switch (p_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDD::TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDD::TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL;
		default:
			return RDD::TEXTURE_LAYOUT_UNDEFINED;
		}
	}

	RDD::RenderPassID RenderingDevice::_render_pass_create(RenderingDeviceDriver* p_driver, const std::vector<AttachmentFormat>& p_attachments, 
		const std::vector<FramebufferPass>& p_passes, std::span<RDD::AttachmentLoadOp> p_load_ops, std::span<RDD::AttachmentStoreOp> p_store_ops, 
		uint32_t p_view_count /*= 1*/, VRSMethod p_vrs_method /*= VRS_METHOD_NONE*/, int32_t p_vrs_attachment /*= -1*/, 
		Size2i p_vrs_texel_size /*= Size2i()*/, std::vector<TextureSamples>* r_samples /*= nullptr*/)
	{
		// NOTE:
		// Before the refactor to RenderingDevice-RenderingDeviceDriver, there was commented out code to
		// specify dependencies to external subpasses. Since it had been unused for a long timel it wasn't ported
		// to the new architecture.

		std::vector<int32_t> attachment_last_pass;
		attachment_last_pass.resize(p_attachments.size());

		if (p_view_count > 1) {
			const RDD::MultiviewCapabilities& capabilities = p_driver->get_multiview_capabilities();

			// This only works with multiview!
			ERR_FAIL_COND_V_MSG(!capabilities.is_supported, RDD::RenderPassID(), "Multiview not supported");

			// Make sure we limit this to the number of views we support.
			ERR_FAIL_COND_V_MSG(p_view_count > capabilities.max_view_count, RDD::RenderPassID(), "Hardware does not support requested number of views for Multiview render pass");
		}

		std::vector<RDD::Attachment> attachments;
		std::vector<uint32_t> attachment_remap;

		for (int i = 0; i < p_attachments.size(); i++) {
			if (p_attachments[i].usage_flags == AttachmentFormat::UNUSED_ATTACHMENT) {
				attachment_remap.push_back(RDD::AttachmentReference::UNUSED);
				continue;
			}

			ERR_FAIL_INDEX_V(p_attachments[i].format, DATA_FORMAT_MAX, RDD::RenderPassID());
			ERR_FAIL_INDEX_V(p_attachments[i].samples, TEXTURE_SAMPLES_MAX, RDD::RenderPassID());
			ERR_FAIL_COND_V_MSG(!(p_attachments[i].usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT | TEXTURE_USAGE_VRS_ATTACHMENT_BIT)),
				RDD::RenderPassID(), std::format("Texture format for index {} requires an attachment (color, depth-stencil, input or VRS) bit set.", std::to_string(i)));

			RDD::Attachment description;
			description.format = p_attachments[i].format;
			description.samples = p_attachments[i].samples;

			// We can setup a framebuffer where we write to our VRS texture to set it up.
			// We make the assumption here that if our texture is actually used as our VRS attachment.
			// It is used as such for each subpass. This is fairly certain seeing the restrictions on subpasses.
			bool is_vrs = (p_attachments[i].usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && i == p_vrs_attachment;
			if (is_vrs) {
				description.load_op = RDD::ATTACHMENT_LOAD_OP_LOAD;
				description.store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
				description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
				description.initial_layout = _vrs_layout_from_method(p_vrs_method);
				description.final_layout = _vrs_layout_from_method(p_vrs_method);
			}
			else {
				if (p_attachments[i].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) {
					description.load_op = p_load_ops[i];
					description.store_op = p_store_ops[i];
					description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
					description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
					description.initial_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					description.final_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				else if (p_attachments[i].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
					description.load_op = p_load_ops[i];
					description.store_op = p_store_ops[i];
					description.stencil_load_op = p_load_ops[i];
					description.stencil_store_op = p_store_ops[i];
					description.initial_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					description.final_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				else if (p_attachments[i].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
					description.load_op = p_load_ops[i];
					description.store_op = p_store_ops[i];
					description.stencil_load_op = p_load_ops[i];
					description.stencil_store_op = p_store_ops[i];
					description.initial_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					description.final_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				else {
					description.load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
					description.store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
					description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
					description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
					description.initial_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
					description.final_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				}
			}

			attachment_last_pass[i] = -1;
			attachment_remap.push_back(attachments.size());
			attachments.push_back(description);
		}

		std::vector<RDD::Subpass> subpasses;
		subpasses.resize(p_passes.size());
		std::vector<RDD::SubpassDependency> subpass_dependencies;

		for (int i = 0; i < p_passes.size(); i++) {
			const FramebufferPass* pass = &p_passes[i];
			RDD::Subpass& subpass = subpasses[i];

			TextureSamples texture_samples = TEXTURE_SAMPLES_1;
			bool is_multisample_first = true;

			for (int j = 0; j < pass->color_attachments.size(); j++) {
				int32_t attachment = pass->color_attachments[j];
				RDD::AttachmentReference reference;
				if (attachment == ATTACHMENT_UNUSED) {
					reference.attachment = RDD::AttachmentReference::UNUSED;
					reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				}
				else {
					ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment( %s ), in pass {}, color attachment {}.", std::to_string(attachment), std::to_string(i), std::to_string(j)).c_str());
					ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, it's marked as depth, but it's not usable as color attachment.", std::to_string(attachment), std::to_string(i)));
					ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));

					if (is_multisample_first) {
						texture_samples = p_attachments[attachment].samples;
						is_multisample_first = false;
					}
					else {
						ERR_FAIL_COND_V_MSG(texture_samples != p_attachments[attachment].samples, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, if an attachment is marked as multisample, all of them should be multisample and use the same number of samples.", std::to_string(attachment), std::to_string(i)));
					}
					reference.attachment = attachment_remap[attachment];
					reference.layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					attachment_last_pass[attachment] = i;
				}
				reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
				subpass.color_references.push_back(reference);
			}

			for (int j = 0; j < pass->input_attachments.size(); j++) {
				int32_t attachment = pass->input_attachments[j];
				RDD::AttachmentReference reference;
				if (attachment == ATTACHMENT_UNUSED) {
					reference.attachment = RDD::AttachmentReference::UNUSED;
					reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				}
				else {
					ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, input attachment {}.", std::to_string(attachment), std::to_string(i), std::to_string(j)).c_str());
					ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, it isn't marked as an input texture.", std::to_string(attachment), std::to_string(i)));
					ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));
					reference.attachment = attachment_remap[attachment];
					reference.layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					attachment_last_pass[attachment] = i;
				}
				reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
				subpass.input_references.push_back(reference);
			}

			if (pass->resolve_attachments.size() > 0) {
				ERR_FAIL_COND_V_MSG(pass->resolve_attachments.size() != pass->color_attachments.size(), RDD::RenderPassID(), std::format("The amount of resolve attachments {} must match the number of color attachments {}.", std::to_string(pass->resolve_attachments.size()), std::to_string(pass->color_attachments.size())));
				ERR_FAIL_COND_V_MSG(texture_samples == TEXTURE_SAMPLES_1, RDD::RenderPassID(), std::format("Resolve attachments specified, but color attachments are not multisample."));
			}
			for (int j = 0; j < pass->resolve_attachments.size(); j++) {
				int32_t attachment = pass->resolve_attachments[j];
				attachments[attachment].load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;

				RDD::AttachmentReference reference;
				if (attachment == ATTACHMENT_UNUSED) {
					reference.attachment = RDD::AttachmentReference::UNUSED;
					reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				}
				else {
					ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, resolve attachment {}.", std::to_string(attachment), std::to_string(i), std::to_string(j)).c_str());
					ERR_FAIL_COND_V_MSG(pass->color_attachments[j] == ATTACHMENT_UNUSED, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, resolve attachment {}, the respective color attachment is marked as unused.", std::to_string(attachment), std::to_string(i), std::to_string(j)));
					ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, resolve attachment, it isn't marked as a color texture.", std::to_string(attachment), std::to_string(i)));
					ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));
					bool multisample = p_attachments[attachment].samples > TEXTURE_SAMPLES_1;
					ERR_FAIL_COND_V_MSG(multisample, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, resolve attachments can't be multisample.", std::to_string(attachment), std::to_string(i)));
					reference.attachment = attachment_remap[attachment];
					reference.layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					attachment_last_pass[attachment] = i;
				}
				reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
				subpass.resolve_references.push_back(reference);
			}

			if (pass->depth_attachment != ATTACHMENT_UNUSED) {
				int32_t attachment = pass->depth_attachment;
				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer depth format attachment{}, in pass {}, depth attachment.", std::to_string(attachment), std::to_string(i)).c_str());
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer depth format attachment{}, in pass {}, it's marked as depth, but it's not a depth attachment.", std::to_string(attachment), std::to_string(i)));
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer depth format attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));
				subpass.depth_stencil_reference.attachment = attachment_remap[attachment];
				subpass.depth_stencil_reference.layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				attachment_last_pass[attachment] = i;

				if (is_multisample_first) {
					texture_samples = p_attachments[attachment].samples;
					is_multisample_first = false;
				}
				else {
					ERR_FAIL_COND_V_MSG(texture_samples != p_attachments[attachment].samples, RDD::RenderPassID(), std::format("Invalid framebuffer depth format attachment{}, in pass {}, if an attachment is marked as multisample, all of them should be multisample and use the same number of samples including the depth.", std::to_string(attachment), std::to_string(i)));
				}

				if (pass->depth_resolve_attachment != ATTACHMENT_UNUSED) {
					attachment = pass->depth_resolve_attachment;

					// As our fallbacks are handled outside of our pass, we should never be setting up a render pass with a depth resolve attachment when not supported.
					ERR_FAIL_COND_V_MSG(!p_driver->has_feature(SUPPORTS_FRAMEBUFFER_DEPTH_RESOLVE), RDD::RenderPassID(), std::format("Invalid framebuffer depth format attachment{}, in pass {}, a depth resolve attachment was supplied when driver doesn't support this feature.", std::to_string(attachment), std::to_string(i)));

					ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer depth resolve format attachment{}, in pass {}, depth resolve attachment.", std::to_string(attachment), std::to_string(i)).c_str());
					ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer depth resolve format attachment{}, in pass {}, it's marked as depth, but it's not a depth resolve attachment.", std::to_string(attachment), std::to_string(i)));
					ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer depth resolve format attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));

					subpass.depth_resolve_reference.attachment = attachment_remap[attachment];
					subpass.depth_resolve_reference.layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					attachment_last_pass[attachment] = i;
				}

			}
			else {
				subpass.depth_stencil_reference.attachment = RDD::AttachmentReference::UNUSED;
				subpass.depth_stencil_reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			}

			if (p_vrs_method == VRS_METHOD_FRAGMENT_SHADING_RATE && p_vrs_attachment >= 0) {
				int32_t attachment = p_vrs_attachment;
				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer VRS format attachment{}, in pass {}, VRS attachment.", std::to_string(attachment), std::to_string(i)).c_str());
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT), RDD::RenderPassID(), std::format("Invalid framebuffer VRS format attachment{}, in pass {}, it's marked as VRS, but it's not a VRS attachment.", std::to_string(attachment), std::to_string(i)));
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), std::format("Invalid framebuffer VRS attachment{}, in pass {}, it already was used for something else before in this pass.", std::to_string(attachment), std::to_string(i)));

				subpass.fragment_shading_rate_reference.attachment = attachment_remap[attachment];
				subpass.fragment_shading_rate_reference.layout = RDD::TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL;
				subpass.fragment_shading_rate_texel_size = p_vrs_texel_size;

				attachment_last_pass[attachment] = i;
			}

			for (int j = 0; j < pass->preserve_attachments.size(); j++) {
				int32_t attachment = pass->preserve_attachments[j];

				ERR_FAIL_COND_V_MSG(attachment == ATTACHMENT_UNUSED, RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, preserve attachment {}. Preserve attachments can't be unused.", std::to_string(attachment), std::to_string(i), std::to_string(j)));

				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), std::format("Invalid framebuffer format attachment{}, in pass {}, preserve attachment {}.", std::to_string(attachment), std::to_string(i), std::to_string(j)).c_str());

				if (attachment_last_pass[attachment] != i) {
					// Preserve can still be used to keep depth or color from being discarded after use.
					attachment_last_pass[attachment] = i;
					subpasses[i].preserve_attachments.push_back(attachment);
				}
			}

			if (r_samples) {
				r_samples->push_back(texture_samples);
			}

			if (i > 0) {
				RDD::SubpassDependency dependency;
				dependency.src_subpass = i - 1;
				dependency.dst_subpass = i;
				dependency.src_stages = (RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
				dependency.dst_stages = (RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
				dependency.src_access = (RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
				dependency.dst_access = (RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT);
				subpass_dependencies.push_back(dependency);
			}
		}

		RDD::AttachmentReference fragment_density_map_attachment_reference;
		if (p_vrs_method == VRS_METHOD_FRAGMENT_DENSITY_MAP && p_vrs_attachment >= 0) {
			fragment_density_map_attachment_reference.attachment = p_vrs_attachment;
			fragment_density_map_attachment_reference.layout = RDD::TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL;
		}

		RDD::RenderPassID render_pass = p_driver->render_pass_create(attachments, subpasses, subpass_dependencies, p_view_count, fragment_density_map_attachment_reference);
		ERR_FAIL_COND_V(!render_pass, RDD::RenderPassID());

		return render_pass;
	}	

	void RenderingDevice::_free_internal(RID p_id)
	{
#ifdef DEV_ENABLED
		String resource_name;
		if (resource_names.has(p_id)) {
			resource_name = resource_names[p_id];
			resource_names.erase(p_id);
		}
#endif

		// Push everything so it's disposed of next time this frame index is processed (means, it's safe to do it).
		if (texture_owner.owns(p_id)) {
			Texture* texture = texture_owner.get_or_null(p_id);
			_check_transfer_worker_texture(texture);

			//RDG::ResourceTracker* draw_tracker = texture->draw_tracker;
			//if (draw_tracker != nullptr) {
			//	draw_tracker->reference_count--;
			//	if (draw_tracker->reference_count == 0) {
			//		RDG::resource_tracker_free(draw_tracker);

			//		if (texture->owner.is_valid() && (texture->slice_type != TEXTURE_SLICE_MAX)) {
			//			// If this was a texture slice, erase the tracker from the map.
			//			Texture* owner_texture = texture_owner.get_or_null(texture->owner);
			//			if (owner_texture != nullptr && owner_texture->slice_trackers != nullptr) {
			//				owner_texture->slice_trackers->erase(texture->slice_rect);

			//				if (owner_texture->slice_trackers->is_empty()) {
			//					memdelete(owner_texture->slice_trackers);
			//					owner_texture->slice_trackers = nullptr;
			//				}
			//			}
			//		}
			//	}
			//}

			frames[frame].textures_to_dispose_of.push_back(*texture);
			texture_owner.free(p_id);
		}
		else if (framebuffer_owner.owns(p_id)) {
			Framebuffer* framebuffer = framebuffer_owner.get_or_null(p_id);
			frames[frame].framebuffers_to_dispose_of.push_back(*framebuffer);
			//driver->framebuffer_free(rid_to_frame_buffer_id[p_id]);			// temp untill we have cache
			if (framebuffer->invalidated_callback != nullptr) {
				framebuffer->invalidated_callback(framebuffer->invalidated_callback_userdata);
			}

			framebuffer_owner.free(p_id);
		}
		else if (sampler_owner.owns(p_id)) {
			RDD::SamplerID sampler_driver_id = *sampler_owner.get_or_null(p_id);
			frames[frame].samplers_to_dispose_of.push_back(sampler_driver_id);
			sampler_owner.free(p_id);
		}
		else if (vertex_buffer_owner.owns(p_id)) {
			Buffer* vertex_buffer = vertex_buffer_owner.get_or_null(p_id);
			_check_transfer_worker_buffer(vertex_buffer);

			//RDG::resource_tracker_free(vertex_buffer->draw_tracker);
			frames[frame].buffers_to_dispose_of.push_back(*vertex_buffer);
			vertex_buffer_owner.free(p_id);
		}
		else if (vertex_array_owner.owns(p_id)) {
			vertex_array_owner.free(p_id);
		}
		else if (index_buffer_owner.owns(p_id)) {
			IndexBuffer* index_buffer = index_buffer_owner.get_or_null(p_id);
			_check_transfer_worker_buffer(index_buffer);

			//RDG::resource_tracker_free(index_buffer->draw_tracker);
			frames[frame].buffers_to_dispose_of.push_back(*index_buffer);
			index_buffer_owner.free(p_id);
		}
		else if (index_array_owner.owns(p_id)) {
			index_array_owner.free(p_id);
		}
		else if (shader_owner.owns(p_id)) {
			Shader* shader = shader_owner.get_or_null(p_id);
			if (shader->driver_id) { // Not placeholder?
				frames[frame].shaders_to_dispose_of.push_back(*shader);
			}
			shader_owner.free(p_id);
		}
		else if (uniform_buffer_owner.owns(p_id)) {
			Buffer* uniform_buffer = uniform_buffer_owner.get_or_null(p_id);
			_check_transfer_worker_buffer(uniform_buffer);

			//RDG::resource_tracker_free(uniform_buffer->draw_tracker);
			frames[frame].buffers_to_dispose_of.push_back(*uniform_buffer);
			uniform_buffer_owner.free(p_id);
		}
		else if (texture_buffer_owner.owns(p_id)) {
			Buffer* texture_buffer = texture_buffer_owner.get_or_null(p_id);
			_check_transfer_worker_buffer(texture_buffer);

			//RDG::resource_tracker_free(texture_buffer->draw_tracker);
			frames[frame].buffers_to_dispose_of.push_back(*texture_buffer);
			texture_buffer_owner.free(p_id);
		}
		else if (storage_buffer_owner.owns(p_id)) {
			Buffer* storage_buffer = storage_buffer_owner.get_or_null(p_id);
			_check_transfer_worker_buffer(storage_buffer);

			//RDG::resource_tracker_free(storage_buffer->draw_tracker);
			frames[frame].buffers_to_dispose_of.push_back(*storage_buffer);
			storage_buffer_owner.free(p_id);
		}
		else if (uniform_set_owner.owns(p_id)) {
			UniformSet* uniform_set = uniform_set_owner.get_or_null(p_id);
			frames[frame].uniform_sets_to_dispose_of.push_back(*uniform_set);
			uniform_set_owner.free(p_id);

			if (uniform_set->invalidated_callback != nullptr) {
				uniform_set->invalidated_callback(uniform_set->invalidated_callback_userdata);
			}
		}
		else if (render_pipeline_owner.owns(p_id)) {
			RenderPipeline* pipeline = render_pipeline_owner.get_or_null(p_id);
			frames[frame].render_pipelines_to_dispose_of.push_back(*pipeline);
			render_pipeline_owner.free(p_id);
		}
		else {
#ifdef DEV_ENABLED
			ERR_PRINT("Attempted to free invalid ID: " + itos(p_id.get_id()) + " " + resource_name);
#else
			ERR_PRINT(std::format("Attempted to free invalid ID: {}", p_id.get_id()));
#endif
		}

		frames_pending_resources_for_processing = uint32_t(frames.size());
	}

	void RenderingDevice::_stall_for_frame(uint32_t p_frame)
	{
		if (frames[p_frame].fence_signaled) {
			driver->fence_wait(frames[p_frame].fence);
			frames[p_frame].fence_signaled = false;
		}
	}

	void RenderingDevice::_stall_for_previous_frames()
	{
		for (uint32_t i = 0; i < frames.size(); i++) {
			_stall_for_frame(i);
		}
	}

	void RenderingDevice::_flush_and_stall_for_all_frames(bool p_begin_frame /*= true*/)
	{
		_stall_for_previous_frames();
		// TODO: end render pass?
		//driver->command_end_render_pass(get_current_command_buffer());
		end_frame();
		execute_frame(false);

		if (p_begin_frame) {
			begin_frame();
		}
		else {
			_stall_for_frame(frame);
		}
	}

	void RenderingDevice::_add_dependency(RID p_id, RID p_depends_on)
	{
		// operator[] inserts empty set if key absent, returns ref either way
		dependency_map[p_depends_on].insert(p_id);
		reverse_dependency_map[p_id].insert(p_depends_on);
	}

	void RenderingDevice::_free_dependencies(RID p_id)
	{
		// --- Forward dependencies: free everything that depends on p_id ---
		auto E = dependency_map.find(p_id);
		if (E != dependency_map.end()) {
			while (!E->second.empty()) {
				free_rid(*E->second.begin());
			}
			dependency_map.erase(E);
		}

		// --- Reverse dependencies: remove p_id from others' dependency sets ---
		auto RE = reverse_dependency_map.find(p_id);
		if (RE != reverse_dependency_map.end()) {
			for (const RID& F : RE->second) {
				auto G = dependency_map.find(F);
				if (G == dependency_map.end()) {
					continue;  // ERR_CONTINUE(!G)
				}
				if (G->second.find(p_id) == G->second.end()) {
					continue;  // ERR_CONTINUE(!G->value.has(p_id))
				}
				G->second.erase(p_id);
			}
			reverse_dependency_map.erase(RE);
		}
	}

	void RenderingDevice::_free_pending_resources(int p_frame)
	{
		// Free in dependency usage order, so nothing weird happens.
		// Pipelines.
		while (!frames[p_frame].render_pipelines_to_dispose_of.empty()) {
			RenderPipeline* pipeline = &(frames[p_frame]).render_pipelines_to_dispose_of.front();

			driver->pipeline_free(pipeline->driver_id);

			frames[p_frame].render_pipelines_to_dispose_of.pop_front();
		}

		// Uniform sets.
		while (!frames[p_frame].uniform_sets_to_dispose_of.empty()) {
			UniformSet* uniform_set = &(frames[p_frame]).uniform_sets_to_dispose_of.front();

			driver->uniform_set_free(uniform_set->driver_id);

			frames[p_frame].uniform_sets_to_dispose_of.pop_front();
		}

		// Shaders.
		while (!frames[p_frame].shaders_to_dispose_of.empty()) {
			Shader* shader = &(frames[p_frame]).shaders_to_dispose_of.front();

			driver->shader_free(shader->driver_id);

			frames[p_frame].shaders_to_dispose_of.pop_front();
		}

		// Samplers.
		while (!frames[p_frame].samplers_to_dispose_of.empty()) {
			RDD::SamplerID sampler = frames[p_frame].samplers_to_dispose_of.front();

			driver->sampler_free(sampler);

			frames[p_frame].samplers_to_dispose_of.pop_front();
		}

		// Framebuffers.
		while (!frames[p_frame].framebuffers_to_dispose_of.empty()) {
			Framebuffer* framebuffer = &frames[p_frame].framebuffers_to_dispose_of.front();
			//TODO: come back when we have frame buffer chache
			//draw_graph.framebuffer_cache_free(driver, framebuffer->framebuffer_cache);
			frames[p_frame].framebuffers_to_dispose_of.pop_front();
		}

		// Textures.
		while (!frames[p_frame].textures_to_dispose_of.empty()) {
			Texture* texture = &frames[p_frame].textures_to_dispose_of.front();
			if (texture->bound) {
				WARN_PRINT("Deleted a texture while it was bound.");
			}

			_texture_free_shared_fallback(texture);

			texture_memory -= driver->texture_get_allocation_size(texture->driver_id);
			driver->texture_free(texture->driver_id);

			frames[p_frame].textures_to_dispose_of.pop_front();
		}

		// Buffers.
		while (!frames[p_frame].buffers_to_dispose_of.empty()) {
			Buffer& buffer = frames[p_frame].buffers_to_dispose_of.front();
			driver->buffer_free(buffer.driver_id);
			buffer_memory -= buffer.size;

			frames[p_frame].buffers_to_dispose_of.pop_front();
		}

		if (frames_pending_resources_for_processing > 0u) {
			--frames_pending_resources_for_processing;
		}
	}

	RenderingDevice::RenderingDevice()
	{
		auto fs = Services::get().get<FilesystemInterface>();
		compiler = std::make_unique<Compiler::GLSLCompiler>(*fs);
		compiler->set_target(Compiler::Target::Vulkan13);
	}

	RenderingDevice::~RenderingDevice() {
		//finalize();
	}

}

