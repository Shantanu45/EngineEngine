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
#include "rendering/framegraph_resources.h"
#include "imgui.h"
#include "rendering/camera.h"
#include "input/input.h"

struct basic_pass_resource
{
	FrameGraphResource scene;
};

struct imgui_pass_resource
{
	FrameGraphResource ui;
};

struct blit_pass_resource
{
	FrameGraphResource scene;
	FrameGraphResource ui;
};

void add_basic_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
	FrameGraphResource image_handle,
	RID frame_buffer,
	RID pipeline,
	RID uniform_set)
{
	bb.add<basic_pass_resource>() =
		fg.add_callback_pass<basic_pass_resource>(
			"Basic Pass",

			[image_handle](FrameGraph::Builder& builder, basic_pass_resource& data)
			{
				data.scene = builder.write(image_handle, 1u);
			},

			[=](const basic_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto cmd = rc.command_buffer;

				auto& scene = resources.get<Rendering::FrameGraphTexture>(data.scene);

				uint32_t w = rc.device->screen_get_width();
				uint32_t h = rc.device->screen_get_height();

				Rect2i viewport(0, 0, w, h);

				rc.device->begin_render_pass_from_frame_buffer(frame_buffer,
					viewport, Color());

				rc.device->bind_render_pipeline(cmd, pipeline);

				rc.device->bind_uniform_set(
					rc.device->get_shader_rid("triangle_shader"),
					uniform_set, 0);

				rc.wsi->bind_and_draw_indexed(cmd, "two_cubes");

				rc.wsi->end_render_pass(cmd);

				//rc.device->_submit_transfer_barriers(cmd);
			});
}

void add_blit_pass(FrameGraph& fg, FrameGraphBlackboard& bb)
{
	const auto& scene = bb.get<basic_pass_resource>();
	const auto& ui = bb.get<imgui_pass_resource>();

	fg.add_callback_pass<blit_pass_resource>(
		"Blit Pass",

		[&](FrameGraph::Builder& builder, blit_pass_resource& data)
		{
			data.scene = builder.read(scene.scene, 1u);
			data.ui = builder.read(ui.ui, 1u);
			builder.set_side_effect();		// mark as non cullable
		},

		[=](const blit_pass_resource& data,
			FrameGraphPassResources& resources,
			void* ctx)
		{
			auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
			auto wsi = rc.wsi;

			auto& scene = resources.get<Rendering::FrameGraphTexture>(data.scene);
			auto& ui = resources.get<Rendering::FrameGraphTexture>(data.ui);

			wsi->blit_render_target_to_screen(scene.texture);
		}
	);
}

void add_imgui_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
	FrameGraphResource image_handle)
{
	bb.add<imgui_pass_resource>() =
		fg.add_callback_pass<imgui_pass_resource>(
			"imgui Pass",

			[image_handle](FrameGraph::Builder& builder, imgui_pass_resource& data)
			{
				data.ui = builder.write(image_handle, 1u);
			},

			[=](const imgui_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto cmd = rc.command_buffer;

				auto& scene = resources.get<Rendering::FrameGraphTexture>(data.ui);

				ImGui::Render();
				rc.device->imgui_execute(ImGui::GetDrawData(), cmd);

				//rc.device->_submit_transfer_barriers(cmd);
			});
}

struct TriangleApplication : EE::Application
{

	struct alignas(16) Camera_UBO {
		glm::mat4 model;
		glm::mat4 view_projection;
	};

	void pre_frame() override
	{
		auto wsi = get_wsi();

		wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
		wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);

		wsi->create_new_vertex_format(wsi->get_default_vertex_attribute(), Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
		auto vertex_format = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

		wsi->load_gltf("assets://gltf/two_cubes.glb",  "two_cubes");

		auto device = wsi->get_rendering_device();

		std::vector<RID> fb_textures;
		{ //texture
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = device->screen_get_width();
			tf.height = device->screen_get_height();
			tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
			tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;

			texture_fb = RD::get_singleton()->texture_create(tf, RD::TextureView());
			fb_textures.push_back(texture_fb);
		}

		scene_fb = device->framebuffer_create(fb_textures);

		pipeline = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" }, "triangle_shader")
			.set_vertex_format(vertex_format)
			.build_from_frame_buffer(scene_fb);		

		camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.set_mode(CameraMode::Fly);


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
		texture_uniform_red = device->texture_create(tf2, RD::TextureView(), { image_red.pixels });


		RDC::SamplerState s;
		s.mag_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.max_lod = 0;

		sampler = device->sampler_create(s);

		std::vector<RD::Uniform> uniforms;
		RD::Uniform u;
		u.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(camera_ubo);
		uniforms.push_back(u);

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

		uniform_set = device->uniform_set_create(uniforms, device->get_shader_rid("triangle_shader"), 0);

		wsi->submit_transfer_workers();

		wsi->pre_frame_loop();

		DEBUG_ASSERT(device->iniitialize_imgui_device(wsi->get_wsi_platform_data(0).platfform_data) == OK);

		input_system = Services::get().get<EE::InputSystemInterface>();
	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		// ---- Build the frame graph ----
		FrameGraph fg;
		FrameGraphBlackboard bb;

		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		camera.update_from_input(input_system.get(), frame_time);

		Camera_UBO ubo{};
		ubo.model = glm::mat4(1.0f); // identity for now
		ubo.view_projection = camera.get_view_projection();

		auto err = device->buffer_update(camera_ubo, 0, sizeof(Camera_UBO), &ubo);

		// needs to be outside render pass begin - end
		std::vector<Rect2i> viewport{ Rect2i(0, 0, device->screen_get_width(), device->screen_get_height()) };
		auto cmd_buffer = device->get_current_command_buffer();

		Rendering::FrameGraphTexture::Desc scene_desc{
			tf,
			RD::TextureView(),
			"scene texture"
		};

		Rendering::FrameGraphTexture scene_tex;
		scene_tex.texture = texture_fb;


		FrameGraphResource scene_res = fg.import("scene texture", scene_desc, std::move(scene_tex));

		auto imgui_fb = device->get_imgui_texture();

		device->imgui_begin_frame();
		
		Rendering::FrameGraphTexture imgui_tex;
		imgui_tex.texture = imgui_fb;
		FrameGraphResource imgui_res = fg.import("scene texture", scene_desc, std::move(imgui_tex));
		add_basic_pass(fg, bb, scene_res, scene_fb, pipeline, uniform_set);
		add_imgui_pass(fg, bb, imgui_res);
		add_blit_pass(fg, bb);

		fg.compile();

		//save_graph_to_file(fg, "file_graph.dot");

		Rendering::RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device = device;
		rc.wsi = wsi;
		fg.execute(&rc, &rc);
	}

	void teardown_application() override
	{
		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		device->free_rid(camera_ubo);
		device->free_rid(texture_uniform);
		device->free_rid(texture_fb);
		device->free_rid(pipeline);
		device->free_rid(sampler);
		//device->free_rid(scene_fb);
	}

private:
	RID camera_ubo;
	RID texture_uniform;
	RID texture_uniform_red;
	RID sampler;
	RID uniform_set;

	Rendering::MeshPrimitive prim;

	RID pipeline;

	RID texture_fb;
	RID imgui_texture_fb;
	RDC::TextureFormat tf;

	RID scene_fb;
	RID imgui_fb;
	Camera camera;

	std::shared_ptr<EE::InputSystemInterface> input_system;
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