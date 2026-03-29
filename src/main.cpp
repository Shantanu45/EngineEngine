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
#include "rendering/renderer_compositor.h"
#include "rendering/pipeline_builder.h"
#include "rendering/fg/frame_graph.h"
#include "rendering/fg/blackboard.h"

using RD = Rendering::RenderingDevice;
using RDC = Rendering::RenderingDeviceCommons;
using RDD = Rendering::RenderingDeviceDriver;

struct RenderContext
{
	Rendering::RenderingDevice* device;
	RDD::CommandBufferID command_buffer;
	Rendering::WSI* wsi;
};

struct FrameGraphTexture {
	struct Desc {
		RDC::TextureFormat texture_format;
		const RD::TextureView texture_view;
		std::string texture_name;
	};

	void create(const Desc& desc, void* ctx) {
		auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);
		texture = device.texture_create(desc.texture_format, desc.texture_view, {});
	}

	void destroy(const Desc& desc, void* ctx) {
		auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);
		device.free_rid(texture);
	}

	void pre_read(const Desc& desc, uint32_t flags, void* ctx) {
		auto& rc = *static_cast<RenderContext*>(ctx);

		RDD::TextureBarrier barrier2;
		barrier2.dst_access = RDD::BARRIER_ACCESS_SHADER_READ_BIT;
		barrier2.next_layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier2.prev_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier2.src_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier2.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier2.texture = rc.device->texture_id_from_rid(texture);
		rc.device->apply_image_barrier(rc.command_buffer, RDD::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			RDD::PipelineStageBits::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { &barrier2, 1 });
	}

	void pre_write(const Desc& desc, uint32_t flags, void* ctx) {
		auto& rc = *static_cast<RenderContext*>(ctx);

		RDD::TextureBarrier barrier;
		barrier.src_access = 0;
		barrier.dst_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.prev_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
		barrier.next_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier.texture = rc.device->texture_id_from_rid(texture);
		rc.device->apply_image_barrier(rc.command_buffer, RDD::PipelineStageBits::PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			RDD::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, { &barrier, 1 });
	}

	// to_string is used by the Graphviz dot exporter
	static std::string to_string(const Desc& d) {
		return d.texture_name;
	}

	RID texture;
};


struct basic_pass_resource
{
	FrameGraphResource scene;
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
				auto& rc = *static_cast<RenderContext*>(ctx);
				auto cmd = rc.command_buffer;

				auto& scene = resources.get<FrameGraphTexture>(data.scene);

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
	const auto& basic = bb.get<basic_pass_resource>();

	fg.add_callback_pass<basic_pass_resource>(
		"Blit Pass",

		[&](FrameGraph::Builder& builder, basic_pass_resource& data)
		{
			data.scene = builder.read(basic.scene, 1u);
			builder.set_side_effect();		// mark as non cullable
		},

		[=](const basic_pass_resource& data,
			FrameGraphPassResources& resources,
			void* ctx)
		{
			auto& rc = *static_cast<RenderContext*>(ctx);
			auto wsi = rc.wsi;

			auto& scene = resources.get<FrameGraphTexture>(data.scene);

			wsi->blit_render_target_to_screen(scene.texture);
		}
	);
}

struct TriangleApplication : EE::Application
{

	struct alignas(16) UBO {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
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

		state_uniform = device->uniform_buffer_create(sizeof(UBO));

		auto fs = Services::get().get<FilesystemInterface>();
		Rendering::ImageLoader img_loader(*fs);
		auto image = img_loader.load_from_file("assets://textures/wall.jpg");

		RDC::TextureFormat tf2;
		tf2.width = image.width;
		tf2.height = image.height;
		tf2.array_layers = 1;
		tf2.texture_type = RDC::TEXTURE_TYPE_2D;
		tf2.usage_bits = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf2.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

		texture_uniform = device->texture_create(tf2, RD::TextureView(), { image.pixels });

		wsi->pre_frame_loop();

		RDC::SamplerState s;
		s.mag_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.max_lod = 0;

		sampler = device->sampler_create(s);

		std::vector<RD::Uniform> uniforms;
		RD::Uniform u;
		u.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(state_uniform);
		uniforms.push_back(u);

		RD::Uniform tu;
		tu.uniform_type = RDC::UNIFORM_TYPE_TEXTURE;
		tu.binding = 1;
		tu.append_id(texture_uniform);
		uniforms.push_back(tu);

		RD::Uniform su;
		su.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		su.binding = 2;
		su.append_id(sampler);
		uniforms.push_back(su);

		DEV_ASSERT(rendering_device != nullptr);
		wsi->submit_transfer_workers();

		uniform_set = device->uniform_set_create(uniforms, device->get_shader_rid("triangle_shader"), 0);
	}
	
	void render_frame(double frame_time, double elapsed_time) override
	{
		// ---- Build the frame graph ----
		FrameGraph fg;
		FrameGraphBlackboard bb;

		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

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

		FrameGraphTexture::Desc scene_desc{
			tf,
			RD::TextureView(),
			"scene texture"
		};

		FrameGraphTexture scene_tex;
		scene_tex.texture = texture_fb;

		FrameGraphResource scene_res = fg.import("scene texture", scene_desc, std::move(scene_tex));
		add_basic_pass(fg, bb, scene_res, scene_fb, pipeline, uniform_set);
		add_blit_pass(fg, bb);

		fg.compile();

		//save_graph_to_file(fg, "file_graph.dot");

		RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device = device;
		rc.wsi = wsi;
		fg.execute(&rc, &rc);
	}

	void post_frame() override
	{
		auto wsi = get_wsi();

		auto device = wsi->get_rendering_device();

		device->free_rid(state_uniform);
		device->free_rid(texture_uniform);
		device->free_rid(texture_fb);
		device->free_rid(scene_fb);
		device->free_rid(pipeline);
		device->free_rid(sampler);
	}

private:
	RID state_uniform;
	RID texture_uniform;
	RID sampler;
	RID uniform_set;

	Rendering::MeshPrimitive prim;

	RID pipeline;

	RID texture_fb;
	RDC::TextureFormat tf;

	RID scene_fb;

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