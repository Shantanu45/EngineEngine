#include "wsi.h"
#include "libassert/assert.hpp"
#include "filesystem/filesystem.h"
#include "compiler/compiler.h"
#include "rendering/rendering_device_commons.h"

namespace Vulkan
{
	void WSI::set_platform(WSIPlatform* wsi_platform)
	{
		platform = wsi_platform;
	}

	bool WSI::init_context()
	{
		frame_count = 2;
		context.set_platform_surface_extension(platform->get_instance_extensions());
		context.initialize();
		if(_create_rendering_context_window(DisplayServerEnums::MAIN_WINDOW_ID) != OK)
			return false;
		return true;
	}

	static std::vector<uint8_t> byte_data_from_shader_file( Compiler::GLSLCompiler* compiler, std::string path, Compiler::Stage stage)
	{
		compiler->set_source_from_file(path, stage);
		compiler->preprocess();
		std::string error_message;

		std::vector<uint32_t> spirv_compiled = compiler->compile(error_message, {});
		std::vector<uint8_t> bytes_spirv(spirv_compiled.size() * sizeof(uint32_t));
		std::memcpy(bytes_spirv.data(), spirv_compiled.data(), bytes_spirv.size());
		return bytes_spirv;
	};


	bool WSI::init_device()
	{
		device_ptr = std::make_unique<RenderingDeviceDriverVulkan>(&context);
		device_ptr->initialize(0, 2);			//TODO: figure out the parameters
		swapchain = device_ptr->swap_chain_create(surface);

		BitField<RenderingDeviceDriverVulkan::CommandQueueFamilyBits> main_queue_bits = {};
		main_queue_bits.set_flag(RenderingDeviceDriverVulkan::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);

		auto main_queue_family = device_ptr->command_queue_family_get(main_queue_bits, surface);
		ERR_FAIL_COND_V(!main_queue_family, FAILED);

		main_queue = device_ptr->command_queue_create(main_queue_family, true);
		ERR_FAIL_COND_V(!main_queue, FAILED);

		frames.resize(frame_count);

		// Create data for all the frames.
		bool frame_failed = false;
		for (uint32_t i = 0; i < frames.size(); i++) {
			frames[i].index = 0;
			frames[i].command_pool = device_ptr->command_pool_create(main_queue_family, RenderingDeviceDriverVulkan::COMMAND_BUFFER_TYPE_PRIMARY);
			if (!frames[i].command_pool) {
				frame_failed = true;
				break;
			}
			frames[i].command_buffer = device_ptr->command_buffer_create(frames[i].command_pool);
			if (!frames[i].command_buffer) {
				frame_failed = true;
				break;
			}
			frames[i].semaphore = device_ptr->semaphore_create();
			if (!frames[i].semaphore) {
				frame_failed = true;
				break;
			}
			frames[i].fence = device_ptr->fence_create();
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
					device_ptr->command_pool_free(frames[i].command_pool);
				}
				if (frames[i].semaphore) {
					device_ptr->semaphore_free(frames[i].semaphore);
				}
				if (frames[i].fence) {
					device_ptr->fence_free(frames[i].fence);
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


		device_ptr->swap_chain_resize(main_queue, swapchain, frame_count);



		auto fs = new FileSystem::Filesystem;
		FileSystem::Filesystem::setup_default_filesystem(fs, "D:/DXProjects/EngineEngine/assets");
		auto compiler = std::make_unique<Compiler::GLSLCompiler>(*fs);
		compiler->set_target(Compiler::Target::Vulkan13);
		std::string path = "assets://shaders/vert.glsl";

		auto bytes_spirv_vs = byte_data_from_shader_file(compiler.get(), "assets://shaders/vert.glsl", Compiler::Stage::Vertex);
		auto bytes_spirv_ps = byte_data_from_shader_file(compiler.get(), "assets://shaders/frag.glsl", Compiler::Stage::Fragment);
		/*compiler->set_source_from_file(path);
		compiler->preprocess();
		std::string error_message;

		std::vector<uint32_t> spirv_compiled = compiler->compile(error_message, {});
		std::vector<uint8_t> bytes_spirv(spirv_compiled.size() * sizeof(uint32_t));
		std::memcpy(bytes_spirv.data(), spirv_compiled.data(), bytes_spirv.size());*/

		const RenderingShaderContainerFormatVulkan& container_format_vs = RenderingShaderContainerFormatVulkan();
		RenderingShaderContainer* shader_container_vs = container_format_vs.create_container();
		std::vector<RenderingDeviceCommons::ShaderStageSPIRVData> spirv_vec;
		RenderingDeviceCommons::ShaderStageSPIRVData data1;
		data1.shader_stage = RenderingDeviceCommons::SHADER_STAGE_VERTEX;
		data1.spirv = bytes_spirv_vs;

		const RenderingShaderContainerFormatVulkan& container_format_ps = RenderingShaderContainerFormatVulkan();
		RenderingShaderContainer* shader_container_ps = container_format_ps.create_container();
		std::vector<RenderingDeviceCommons::ShaderStageSPIRVData> spirv_vec_ps;
		RenderingDeviceCommons::ShaderStageSPIRVData data2;
		data2.shader_stage = RenderingDeviceCommons::SHADER_STAGE_FRAGMENT;
		data2.spirv = bytes_spirv_ps;
		spirv_vec.push_back(data1);
		spirv_vec.push_back(data2);

		bool code_compiled_vs = shader_container_vs->set_code_from_spirv("solidcolor", spirv_vec);
		DEBUG_ASSERT(code_compiled_vs);
		auto shader = device_ptr->shader_create_from_container(shader_container_vs, {});

		//const RenderingShaderContainerFormatVulkan& container_format_ps = RenderingShaderContainerFormatVulkan();
		//RenderingShaderContainer* shader_container_ps = container_format_ps.create_container();
		//std::vector<ShaderStageSPIRVData> spirv_vec_ps;
		//ShaderStageSPIRVData data2;
		//data2.shader_stage = SHADER_STAGE_FRAGMENT;
		//data2.spirv = bytes_spirv_ps;
		//spirv_vec.push_back(data2);
		//bool code_compiled_ps = shader_container_ps->set_code_from_spirv("solidcolor_ps", spirv_vec_ps);
		//DEBUG_ASSERT(code_compiled_ps);
		//device_ptr->shader_create_from_container(shader_container_ps, {});
		auto vertex = device_ptr->vertex_format_create({}, {});
		std::vector<int32_t> subpasses{ 1 };
		pipeline = device_ptr->render_pipeline_create(shader, vertex, RenderingDeviceDriverVulkan::RenderPrimitive::RENDER_PRIMITIVE_TRIANGLE_STRIPS, {}, {}, {}, RenderingDeviceDriverVulkan::PipelineColorBlendState::create_blend(), subpasses, {}, device_ptr->swap_chain_get_render_pass(swapchain), 0);
		return true;
	}

	bool WSI::begin_frame()
	{
		VkResult result = VK_SUCCESS;
		do
		{
			bool resize_required;
			RenderingDeviceDriverVulkan::FramebufferID framebuffer = device_ptr->swap_chain_acquire_framebuffer(main_queue, swapchain, resize_required);


			auto command_buffer = frames[curr_frame].command_buffer;
			auto render_pass = device_ptr->swap_chain_get_render_pass(swapchain);
			device_ptr->command_buffer_begin(command_buffer);
			std::array<RenderingDeviceDriver::RenderPassClearValue, 1> val;
			val[0].color = Color{1.0 , 0.0 , 0.0 , 0.0};
			std::vector<Rect2i> frame_rects = { Rect2i{ 0, 0, (int)platform->get_surface_width(), (int)platform->get_surface_height() } };
			device_ptr->command_begin_render_pass(command_buffer, render_pass, framebuffer, RenderingDeviceDriverVulkan::COMMAND_BUFFER_TYPE_PRIMARY, frame_rects[0], val);
			device_ptr->command_bind_render_pipeline(command_buffer, pipeline);
			device_ptr->command_render_set_viewport(command_buffer, frame_rects);
			device_ptr->command_render_set_scissor(command_buffer, frame_rects);
			device_ptr->command_render_draw(command_buffer, 3, 1, 0, 0);
			device_ptr->command_end_render_pass(command_buffer);
			device_ptr->command_buffer_end(command_buffer);


			device_ptr->command_queue_execute_and_present(main_queue, {}, { &command_buffer, 1 }, {}, frames[curr_frame].fence, { &swapchain, 1 });

			if (result >= 0)
			{
				platform->poll_input();
			}
		} while (result < 0);

		device_ptr->fence_wait(frames[curr_frame].fence);

		curr_frame = (curr_frame + 1) % 2;

		return true;
	}

	bool WSI::end_frame()
	{
		return true;
	}

	Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
	{
		WindowData& wd = windows[p_window_id];
		wd.platfform_data = platform->get_window_platform_data(p_window_id);

		Error err = context.window_create(p_window_id, &wd.platfform_data);
		ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create %s window.", p_rendering_driver));
		context.window_set_size(p_window_id, platform->get_surface_width(), platform->get_surface_height());
		surface = context.surface_get_from_window(p_window_id);
		return OK;
	}

	void WSI::free_pending_resources(int p_frame) 
	{
		
	}

	void WSI::teardown()
	{
		if (platform)
			platform->release_resources();
		int frame = 0;
		for (uint32_t i = 0; i < frames.size(); i++) {
			int f = (frame + i) % frames.size();
			free_pending_resources(f);
			device_ptr->command_pool_free(frames[i].command_pool);
			//device_ptr->timestamp_query_pool_free(frames[i].timestamp_pool);
			device_ptr->semaphore_free(frames[i].semaphore);
			device_ptr->fence_free(frames[i].fence);

			//Device::CommandBufferPool& buffer_pool = frames[i].command_buffer_pool;
			//for (uint32_t j = 0; j < buffer_pool.buffers.size(); j++) {
			//	device_ptr->semaphore_free(buffer_pool.semaphores[j]);
			//}

			//for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
			//	device_ptr->semaphore_free(frames[i].transfer_worker_semaphores[j]);
			//}
		}

		//if (pipeline_cache_enabled) {
		//	update_pipeline_cache(true);
		//	device_ptr->pipeline_cache_free();
		//}
	}
}
