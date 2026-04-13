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
#include "application/application_entry/application_entry.h"
#include "libassert/assert.hpp"
#include "rendering/image_loader.h"
#include "rendering/pipeline_builder.h"
#include "rendering/render_passes/common.h"
#include "rendering/camera.h"
#include "input/input.h"
#include "util/timer.h"
#include "rendering/primitve_shapes.h"

void add_basic_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
	Size2i extent,
	RID pipeline,
	RID uniform_set, 
	Rendering::MeshHandle mesh_handle)
{
	bb.add<basic_pass_resource>() =
		fg.add_callback_pass<basic_pass_resource>(
			"Basic Pass",
			[&](FrameGraph::Builder& builder, basic_pass_resource& data)
			{
				RD::TextureFormat tf;
				tf.texture_type = RD::TEXTURE_TYPE_2D;
				tf.width = extent.x;
				tf.height = extent.y;
				tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
				tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;

				data.scene = builder.create<Rendering::FrameGraphTexture>("scene texture", { tf, RD::TextureView(), "scene texture" });

				RD::TextureFormat tf_depth;
				tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
				tf_depth.width = extent.x;
				tf_depth.height = extent.y;
				tf_depth.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;// | RD::TEXTURE_USAGE_SAMPLING_BIT;
				tf_depth.format = RD::DATA_FORMAT_D32_SFLOAT;

				data.depth = builder.create<Rendering::FrameGraphTexture>("depth texture", { tf_depth, RD::TextureView(), "depth texture" });


				data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
				data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
			},
			[=](const basic_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto cmd = rc.command_buffer;

				auto& scene_tex = resources.get<Rendering::FrameGraphTexture>(data.scene);
				auto& depth_tex = resources.get<Rendering::FrameGraphTexture>(data.depth);

				uint32_t w = rc.device->screen_get_width();
				uint32_t h = rc.device->screen_get_height();

				Rect2i viewport(0, 0, w, h);

				RID frame_buffer = rc.device->framebuffer_create({scene_tex.texture_rid, depth_tex.texture_rid});

				GPU_SCOPE(cmd, "Basic Pass", Color(1.0, 0.0, 0.0, 1.0));
				std::array<RDD::RenderPassClearValue, 2> clear_values;
				clear_values[0].color = Color();
				clear_values[1].depth = 1.0;
				clear_values[1].stencil = 0.0;

				rc.device->begin_render_pass_from_frame_buffer(frame_buffer,
					viewport, clear_values);

				rc.device->bind_render_pipeline(cmd, pipeline.pipeline_rid);

				rc.device->bind_uniform_set(pipeline.shader_rid, uniform_set, 0);

				rc.wsi->draw_mesh(cmd, mesh_handle);

				rc.wsi->end_render_pass(cmd);

				//rc.device->_submit_transfer_barriers(cmd);
			});
}

struct TriangleApplication : EE::Application
{

	struct alignas(16) Camera_UBO {
		glm::mat4 model;
		glm::mat4 view_projection;
	};

	bool pre_frame() override
	{
		input_system = Services::get().get<EE::InputSystemInterface>();
		RenderUtilities::capturing_timestamps = true;

		camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.set_reset_on_resize();
		camera.set_mode(CameraMode::Fly);

		auto wsi = get_wsi();

		wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
		wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);

		wsi->create_new_vertex_format(wsi->get_default_vertex_attribute(), Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
		auto vertex_format = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

		//mesh_handle = wsi->load_gltf("assets://gltf/two_cubes.glb",  "two_cubes");

		mesh_handle = Rendering::Shapes::upload_sphere(*wsi, 32, 32, "hires_sphere");

		auto device = wsi->get_rendering_device();

		// Create frame buffer format

		std::vector<RD::AttachmentFormat> attachments;

		RD::AttachmentFormat color;
		color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		attachments.push_back(color);

		RD::AttachmentFormat depth;
		depth.format = RD::DATA_FORMAT_D32_SFLOAT;
		depth.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		attachments.push_back(depth);

		auto framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);

		RDC::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test = true;
		depth_state.enable_depth_write = true;
		depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

		pipeline = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" }, "triangle_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(depth_state)
			.build(framebuffer_format);

		camera_ubo = device->uniform_buffer_create(sizeof(Camera_UBO));

		auto fs = Services::get().get<FilesystemInterface>();
		Rendering::ImageLoader img_loader(*fs);
		auto image = img_loader.load_from_file("assets://textures/wall.jpg");
		auto image_red = img_loader.load_from_file("assets://textures/wall_red.jpg");

		RDC::TextureFormat tf2;
		tf2.width = image.width;
		tf2.height = image.height;
		tf2.array_layers = 1;
		tf2.texture_type = RDC::TEXTURE_TYPE_2D;
		tf2.usage_bits = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf2.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

		texture_uniform = device->texture_create(tf2, RD::TextureView(), { image.pixels });
		device->set_resource_name(texture_uniform, "Wall texture");
		texture_uniform_red = device->texture_create(tf2, RD::TextureView(), { image_red.pixels });
		device->set_resource_name(texture_uniform_red, "Wall texture with red marking");


		RDC::SamplerState s;
		s.mag_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.max_lod = 0;

		sampler = device->sampler_create(s);

		std::vector<RD::Uniform> uniforms;

		RD::Uniform tu;
		tu.uniform_type = RDC::UNIFORM_TYPE_TEXTURE;
		tu.binding = 1;
		tu.append_id(texture_uniform);
		uniforms.push_back(tu);

		RD::Uniform tu_red;
		tu_red.uniform_type = RDC::UNIFORM_TYPE_TEXTURE;
		tu_red.binding = 2;
		tu_red.append_id(texture_uniform_red);
		uniforms.push_back(tu_red);

		RD::Uniform su;
		su.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		su.binding = 3;
		su.append_id(sampler);
		uniforms.push_back(su);

		uniform_set = device->uniform_set_create(uniforms, pipeline.shader_rid, 0);

		wsi->submit_transfer_workers();
		return wsi->pre_frame_loop();
	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		//TIMESTAMP_BEGIN();
		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		camera.update_from_input(input_system.get(), frame_time);

		Camera_UBO ubo{};
		ubo.model = glm::mat4(1.0f); // identity for now
		ubo.view_projection = camera.get_view_projection();

		device->set_push_constant(&ubo, sizeof(Camera_UBO), pipeline.shader_rid);

		device->imgui_begin_frame();
		const auto timer = Services::get().get<Util::FrameTimer>();
		
		//auto gpu_time = wsi->get_gpu_frame_time();
		//auto cpu_time = wsi->get_cpu_frame_time();

		ImGui::Text("FPS: %.1f", timer->get_fps());
		ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);
		//ImGui::Text("CPUe Time: %.3f ms", cpu_time);
		//ImGui::Text("GPUe Time: %.3f ms", gpu_time);

		// ---- Build the frame graph ----
		FrameGraph fg;
		FrameGraphBlackboard bb;

		add_basic_pass(fg, bb, { device->screen_get_width(), device->screen_get_height() }, pipeline, uniform_set, mesh_handle);
		Rendering::add_imgui_pass(fg, bb, { device->screen_get_width(), device->screen_get_height() });
		Rendering::add_blit_pass(fg, bb);

		fg.compile();

		//save_graph_to_file(fg, "file_graph.dot");

		Rendering::RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device = device;
		rc.wsi = wsi;
		fg.execute(&rc, &rc);
		//TIMESTAMP_BEGIN();
	}

	void teardown_application() override
	{
		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		device->free_rid(camera_ubo);
		device->free_rid(texture_uniform);
		device->free_rid(texture_uniform_red);
		device->free_rid(pipeline.pipeline_rid);
	}

private:
	RID camera_ubo;
	RID texture_uniform;
	RID texture_uniform_red;
	RID sampler;
	RID uniform_set;

	Rendering::MeshPrimitive prim;

	Rendering::Pipeline pipeline;
	Camera camera;

	std::shared_ptr<EE::InputSystemInterface> input_system;

	Rendering::MeshHandle mesh_handle;

};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try
		{
			auto* app = new TriangleApplication();
			return app;
		}
		catch (const std::exception& e)
		{
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}



//struct alignas(16) UBO {
 //	float x, y, z;
 //	float _pad;  // pad to 16 bytes
 //};
		 //RDC::TextureFormat c_tf;
	 //c_tf.width = device->screen_get_width();
	 //c_tf.height = device->screen_get_height();
	 //c_tf.array_layers = 1;
	 //c_tf.texture_type = RDC::TEXTURE_TYPE_2D;
	 //c_tf.usage_bits = RDC::TEXTURE_USAGE_CPU_READ_BIT | RDC::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	 //c_tf.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

	 //copy_texture = device->texture_create(c_tf, RD::TextureView());