/*****************************************************************//**
 * \file   main.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vma/vk_mem_alloc.h"
#include "application/application.h"
#include "application/application_entry/application_entry.h"
#include "libassert/assert.hpp"
#include "rendering/image_loader.h"
#include <cmath> 
#include <cstddef>
#include "rendering/renderer_compositor.h"

struct TriangleApplication : EE::Application
{
	//struct alignas(16) UBO {
	//	float x, y, z;
	//	float _pad;  // pad to 16 bytes
	//};

	struct alignas(16) UBO {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	};

	void pre_frame() override
	{
		auto wsi = get_wsi();

		wsi->set_default_vertex_attribute();

		wsi->load_gltf("assets://gltf/two_cubes.glb");
		wsi->set_program("triangle_shader", { "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" });

		auto device = wsi->get_rendering_device();

		state_uniform = device->uniform_buffer_create(sizeof(UBO));

		//Rendering::RenderingDeviceCommons::TextureFormat c_tf;
		//c_tf.width = device->screen_get_width();
		//c_tf.height = device->screen_get_height();
		//c_tf.array_layers = 1;
		//c_tf.texture_type = Rendering::RenderingDeviceCommons::TEXTURE_TYPE_2D;
		//c_tf.usage_bits = Rendering::RenderingDeviceCommons::TEXTURE_USAGE_CPU_READ_BIT | Rendering::RenderingDeviceCommons::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		//c_tf.format = Rendering::RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;

		//copy_texture = device->texture_create(c_tf, Rendering::RenderingDevice::TextureView());


		auto fs = Services::get().get<FilesystemInterface>();
		Rendering::ImageLoader img_loader(*fs);
		auto image = img_loader.load_from_file("assets://textures/wall.jpg");

		Rendering::RenderingDeviceCommons::TextureFormat tf;
		tf.width = image.width;
		tf.height = image.height;
		tf.array_layers = 1;
		tf.texture_type = Rendering::RenderingDeviceCommons::TEXTURE_TYPE_2D;
		tf.usage_bits = Rendering::RenderingDeviceCommons::TEXTURE_USAGE_SAMPLING_BIT | Rendering::RenderingDeviceCommons::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf.format = Rendering::RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;

		texture_uniform = device->texture_create(tf, Rendering::RenderingDevice::TextureView(), { image.pixels });
		wsi->pre_frame_loop();

		Rendering::RenderingDeviceCommons::SamplerState s;
		s.mag_filter = Rendering::RenderingDeviceCommons::SAMPLER_FILTER_LINEAR;
		s.min_filter = Rendering::RenderingDeviceCommons::SAMPLER_FILTER_LINEAR;
		s.max_lod = 0;

		sampler = device->sampler_create(s);

		std::vector<Rendering::RenderingDevice::Uniform> uniforms;
		Rendering::RenderingDevice::Uniform u;
		u.uniform_type = Rendering::RenderingDeviceCommons::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(state_uniform);
		uniforms.push_back(u);

		Rendering::RenderingDevice::Uniform tu;
		tu.uniform_type = Rendering::RenderingDeviceCommons::UNIFORM_TYPE_TEXTURE;
		tu.binding = 1;
		tu.append_id(texture_uniform);
		uniforms.push_back(tu);

		Rendering::RenderingDevice::Uniform su;
		su.uniform_type = Rendering::RenderingDevice::UNIFORM_TYPE_SAMPLER;
		su.binding = 2;
		su.append_id(sampler);
		uniforms.push_back(su);

		DEV_ASSERT(rendering_device != nullptr);
		wsi->pipeline_create();
		//device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);

		//wsi->set_program({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" });

		uniform_set = device->uniform_set_create(uniforms, wsi->get_bound_shader(), 0);

	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		double intpart;
		double fracpart = std::modf(elapsed_time, &intpart);

		UBO ubo{};
		ubo.model = glm::mat4(1.0f); // identity for now
		ubo.view = glm::lookAt(
			glm::vec3(0.0f, 0.0f, 3.0f),  // camera position
			glm::vec3(0.0f, 0.0f, 0.0f),  // look at origin
			glm::vec3(0.0f, 1.0f, 0.0f)   // up vector
		);
		ubo.projection = glm::perspective(glm::radians(45.0f),
			(float)device->screen_get_width() / (float)device->screen_get_height(),         // aspect ratio
			0.1f,                          // near
			100.0f                         // far
		);

		// Vulkan clip space fix - flip Y
		ubo.projection[1][1] *= -1;
		auto err = device->buffer_update(state_uniform, 0, sizeof(UBO), &ubo);

		// needs to be outside render pass begin - end
		std::vector<Rect2i> viewport{ Rect2i(0, 0, device->screen_get_width(), device->screen_get_height()) };
		auto cmd_buffer = device->get_current_command_buffer();

		Rendering::RenderingDeviceDriver::TextureBarrier barrier;
		barrier.src_access = 0;
		barrier.dst_access = Rendering::RenderingDeviceDriver::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.prev_layout = Rendering::RenderingDeviceDriver::TEXTURE_LAYOUT_UNDEFINED;
		barrier.next_layout = Rendering::RenderingDeviceDriver::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.subresources = { Rendering::RenderingDeviceDriver::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier.texture = device->texture_id_from_rid(wsi->get_texture_fb());
		device->apply_image_barrier(cmd_buffer, Rendering::RenderingDeviceDriver::PipelineStageBits::PIPELINE_STAGE_TOP_OF_PIPE_BIT, Rendering::RenderingDeviceDriver::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, { &barrier, 1 });

		device->begin_render_pass(wsi->get_current_render_pass(), wsi->get_current_frame_buffer(), viewport[0], Color());
		//
		device->bind_render_pipeline(cmd_buffer, wsi->get_current_pipeline());
		device->bind_uniform_set(wsi->get_bound_shader(), uniform_set, 0);
		wsi->bind_and_draw_indexed(cmd_buffer);
		wsi->end_render_pass(cmd_buffer);

		device->_submit_transfer_barriers(cmd_buffer);

		Rendering::RenderingDeviceDriver::TextureBarrier barrier2;
		barrier2.dst_access = Rendering::RenderingDeviceDriver::BARRIER_ACCESS_SHADER_READ_BIT;
		barrier2.next_layout = Rendering::RenderingDeviceDriver::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier2.prev_layout = Rendering::RenderingDeviceDriver::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier2.src_access = Rendering::RenderingDeviceDriver::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier2.subresources = { Rendering::RenderingDeviceDriver::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier2.texture = device->texture_id_from_rid(wsi->get_texture_fb());

		device->apply_image_barrier(cmd_buffer, Rendering::RenderingDeviceDriver::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, Rendering::RenderingDeviceDriver::PipelineStageBits::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { &barrier2, 1 });
		wsi->blit_render_target_to_screen(wsi->get_texture_fb());

		//device->begin_for_screen(DisplayServerEnums::MAIN_WINDOW_ID);


	}

private:
	RID state_uniform;
	RID texture_uniform;
	RID sampler;
	RID uniform_set;

	Rendering::MeshPrimitive prim;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;
		spdlog::info("hwlloe");
		try
		{
			auto* app = new TriangleApplication();
			return app;
		}
		catch (const std::exception& e)
		{
			//LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}