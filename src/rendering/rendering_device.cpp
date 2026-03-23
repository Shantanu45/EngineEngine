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
#include "glm/gtc/round.hpp"

namespace Rendering
{

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

			// Create the semaphores for the transfer workers.
			frames[i].transfer_worker_semaphores.resize(transfer_worker_pool_max_size);
			for (uint32_t j = 0; j < transfer_worker_pool_max_size; j++) {
				frames[i].transfer_worker_semaphores[j] = driver->semaphore_create();
				if (!frames[i].transfer_worker_semaphores[j]) {
					frame_failed = true;
					break;
				}
			}
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

		return OK;
	}

	void RenderingDevice::finalize() {

	}

#pragma region Shader

	Rendering::RDShaderSPIRV* RenderingDevice::shader_compile_spirv_from_shader_source(const RDShaderSource* p_source, bool p_allow_cache /*= true*/)
	{
		//ERR_FAIL_COND_V(p_source.is_null(), &RDShaderSPIRV());

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
			ERR_FAIL_COND_V_MSG(!error.empty(), RID(), "Can't create a shader from an errored bytecode. Check errors in source bytecode.");
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
		return shader_create_from_container_with_samplers(shader_container, RID(), immutable_samplers);//_shader_create_from_spirv(stage_data);
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
		set_resource_name(id, "RID:" + itos(id.get_id()));
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
		return id;
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
		_flush_and_stall_for_all_frames();

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
		set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
		return id;
	}

	void RenderingDevice::swap_buffers(bool p_present)
	{
		end_frame();

		execute_frame(p_present);
		_stall_for_frame(frame);

		frame = (frame + 1) % frames.size();
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

	RDD::CommandBufferID RenderingDevice::get_current_command_buffer()
	{
		return frames[frame].command_buffer;
	}

	void RenderingDevice::render_draw(RenderingDeviceDriver::CommandBufferID p_command_buffer, uint32_t p_vertex_count, uint32_t p_instance_count)
	{
		//TODO: re
		//driver->command_render_draw(p_command_buffer, p_vertex_count, p_instance_count, 0, 0);
		driver->command_render_draw_indexed(p_command_buffer, 3, 1, 0, 0, 0);
	}

	Rendering::RenderingDevice::VertexFormatID RenderingDevice::vertex_format_create(const std::vector<VertexAttribute>& p_vertex_descriptions)
	{
		VertexDescriptionKey key;
		key.vertex_formats = p_vertex_descriptions;

		auto it_c = vertex_format_cache.find(key);
		if (it_c != vertex_format_cache.end()) {
			return it_c->second;
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

	RID RenderingDevice::create_swapchain_pipeline(DisplayServerEnums::WindowID window, RID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState& p_rasterization_state, const PipelineMultisampleState& p_multisample_state, const PipelineDepthStencilState& p_depth_stencil_state, const PipelineColorBlendState& p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags /*= 0*/, uint32_t p_for_render_pass /*= 0*/, const std::vector<PipelineSpecializationConstant>& p_specialization_constants /*= std::vector<PipelineSpecializationConstant>()*/)
	{
		Shader* shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

		ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

		std::unordered_map<DisplayServerEnums::WindowID, RDD::SwapChainID>::const_iterator sc_it = screen_swap_chains.find(window);
		ERR_FAIL_COND_V_MSG(sc_it == screen_swap_chains.end(), RID(), "Screen was never prepared.");

		//FramebufferFormat fb_format = framebuffer_formats[driver->swap_chain_get_format(swapchain)];

		//// Validate shader vs. framebuffer.
		//{
		//	ERR_FAIL_COND_V_MSG(p_for_render_pass >= uint32_t(fb_format.E->first.passes.size()), RID(), std::format("Render pass requested for pipeline creation {} is out of bounds", std::to_string(p_for_render_pass)));
		//	const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
		//	uint32_t output_mask = 0;
		//	for (int i = 0; i < pass.color_attachments.size(); i++) {
		//		if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
		//			output_mask |= 1 << i;
		//		}
		//	}
		//	ERR_FAIL_COND_V_MSG(shader->fragment_output_mask != output_mask, RID(),
		//		std::format("Mismatch fragment shader output mask {} and framebuffer color output mask {} when binding both in render pipeline.", std::to_string(shader->fragment_output_mask), std::to_string(output_mask)));
		//}

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

		//const FramebufferPass& pass = fb_format.E->first.passes[p_for_render_pass];
		//ERR_FAIL_COND_V(p_blend_state.attachments.size() < pass.color_attachments.size(), RID());
		//for (int i = 0; i < pass.color_attachments.size(); i++) {
		//	if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_color_blend_factor, BLEND_FACTOR_MAX, RID());
		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_color_blend_factor, BLEND_FACTOR_MAX, RID());
		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].color_blend_op, BLEND_OP_MAX, RID());

		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
		//		ERR_FAIL_INDEX_V(p_blend_state.attachments[i].alpha_blend_op, BLEND_OP_MAX, RID());
		//	}
		//}

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
		return id;
	}

	void RenderingDevice::begin_frame(bool p_presented /*= false*/)
	{

		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;

		driver->command_buffer_begin(command_buffer);
	}

	void  RenderingDevice::bind_render_pipeline(RDD::CommandBufferID p_command_buffer, RID pipeline)
	{
		RenderPipeline* render_pipeline = render_pipeline_owner.get_or_null(pipeline);
		driver->command_bind_render_pipeline(p_command_buffer, render_pipeline->driver_id);
	}

	void RenderingDevice::end_frame()
	{
		RDD::CommandBufferID command_buffer = frames[frame].command_buffer;
		driver->command_end_render_pass(command_buffer);

		driver->command_buffer_end(command_buffer);
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

		//execute_chained_cmds(present_swap_chain, frames[frame].fence, semaphore);
		// Indicate the fence has been signaled so the next time the frame's contents need to be
		// used, the CPU needs to wait on the work to be completed.
		frames[frame].fence_signaled = true;
		std::vector<RenderingDeviceDriver::SemaphoreID> frame_semaphores{ frames[frame].semaphore };
		if (frame_can_present) {
			auto command_buffer = get_current_command_buffer();
			/*if (separate_present_queue) {*/
				// Issue the presentation separately if the presentation queue is different from the main queue.
			driver->command_queue_execute_and_present(present_queue, {}, { &command_buffer, 1}, {}, frames[frame].fence, frames[frame].swap_chains_to_present);
			//}

			frames[frame].swap_chains_to_present.clear();
		}
	}

	void RenderingDevice::_stall_for_frame(uint32_t p_frame)
	{
		if (frames[p_frame].fence_signaled) {
			driver->fence_wait(frames[p_frame].fence);
			frames[p_frame].fence_signaled = false;
		}
	}

	void RenderingDevice::_flush_and_stall_for_all_frames(bool p_begin_frame /*= true*/)
	{

	}

	uint32_t RenderingDevice::_get_swap_chain_desired_count() const {
		return 2;
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
			const RDD::MultiviewCapabilities& capabilities = {};// p_driver->get_multiview_capabilities();TODO

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

	Rendering::RenderingDevice::Buffer* RenderingDevice::_get_buffer_from_owner(RID p_buffer)
	{
		Buffer* buffer = nullptr;
		if (vertex_buffer_owner.owns(p_buffer)) {
			buffer = vertex_buffer_owner.get_or_null(p_buffer);
		}
		else if (index_buffer_owner.owns(p_buffer)) {
			buffer = index_buffer_owner.get_or_null(p_buffer);
		}
		//else if (uniform_buffer_owner.owns(p_buffer)) {
		//	buffer = uniform_buffer_owner.get_or_null(p_buffer);
		//}
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
		//for (const RID& buf : unique_buffers) {
		//	_add_dependency(id, buf);
		//}

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
		set_resource_name(id, "RID:" + itos(id.get_id()));
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
		//_add_dependency(id, p_index_buffer);
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
		driver->command_render_bind_index_buffer(frames[frame].command_buffer, index_array->driver_id, index_array->format, index_array->offset);
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
				DEV_ASSERT(!transfer_worker_pool_full && "A transfer worker should never be created when the pool is full.");

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


	RenderingDevice::RenderingDevice()
	{
		auto fs = Services::get().get<FilesystemInterface>();
		compiler = std::make_unique<Compiler::GLSLCompiler>(*fs);
		compiler->set_target(Compiler::Target::Vulkan13);
	}

	RenderingDevice::~RenderingDevice() {
		finalize();
	}

}

