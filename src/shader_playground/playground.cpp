#include "application/common.h"
#include "rendering/pipeline_builder.h"
#include "rendering/render_passes/common.h"
#include "rendering/primitve_shapes.h"
#include "rendering/drawable.h"
#include "rendering/uniform_buffer.h"
#include "rendering/uniform_set_builder.h"

using RD = Rendering::RenderingDevice;
using RDC = Rendering::RenderingDeviceCommons;
using RDD = Rendering::RenderingDeviceDriver;

struct PlaygroundUBO {
    glm::vec2 resolution;
    float     time;
    float     _pad = 0.0f;
};

void add_playground_pass(
	FrameGraph& fg,
	FrameGraphBlackboard& bb,
	Size2i extent,
	Rendering::Drawable quad_drawable,
	Rendering::MeshStorage& storage)
{
	bb.add<basic_pass_resource>() =
		fg.add_callback_pass<basic_pass_resource>(
			"Playground Pass",
			[&](FrameGraph::Builder& builder, basic_pass_resource& data)
			{
				RD::TextureFormat tf;
				tf.texture_type = RD::TEXTURE_TYPE_2D;
				tf.width        = extent.x;
				tf.height       = extent.y;
				tf.usage_bits   = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
				tf.format       = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				data.scene = builder.create<Rendering::FrameGraphTexture>("scene texture", { tf, RD::TextureView(), "scene texture" });

				RD::TextureFormat tf_depth;
				tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
				tf_depth.width        = extent.x;
				tf_depth.height       = extent.y;
				tf_depth.usage_bits   = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				tf_depth.format       = RD::DATA_FORMAT_D32_SFLOAT;
				data.depth = builder.create<Rendering::FrameGraphTexture>("depth texture", { tf_depth, RD::TextureView(), "depth texture" });

				data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
				data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
			},
			[=, &storage](const basic_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
			{
				auto& rc  = *static_cast<Rendering::RenderContext*>(ctx);
				auto  cmd = rc.command_buffer;

				auto& scene_tex = resources.get<Rendering::FrameGraphTexture>(data.scene);
				auto& depth_tex = resources.get<Rendering::FrameGraphTexture>(data.depth);

				uint32_t w = rc.device->screen_get_width();
				uint32_t h = rc.device->screen_get_height();

				RID frame_buffer = rc.device->framebuffer_get_or_create({ scene_tex.texture_rid, depth_tex.texture_rid });

				GPU_SCOPE(cmd, "Playground Pass", Color(1.0, 0.0, 0.0, 1.0));
				std::array<RDD::RenderPassClearValue, 2> clear_values;
				clear_values[0].color   = Color();
				clear_values[1].depth   = 1.0;
				clear_values[1].stencil = 0.0;

				rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);
				submit_drawable(rc, cmd, quad_drawable, storage);
				rc.wsi->end_render_pass(cmd);
			});
}

struct ShaderPlayground : EE::Application
{
	bool pre_frame() override
	{
		wsi    = get_wsi();
		device = wsi->get_rendering_device();

		wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
		wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);

		wsi->create_new_vertex_format(
			wsi->get_default_vertex_attribute(),
			Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
		auto vertex_format = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

		mesh_storage->initialize(device);
		quad_mesh = Rendering::Shapes::upload_quad(*wsi, *mesh_storage, "fullscreen_quad");

		RD::AttachmentFormat color;
		color.format      = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		RD::AttachmentFormat depth;
		depth.format      = RD::DATA_FORMAT_D32_SFLOAT;
		depth.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		auto framebuffer_format = RD::get_singleton()->framebuffer_format_create({ color, depth });

		RDC::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test  = false;
		depth_state.enable_depth_write = false;

		RDC::PipelineRasterizationState rs;
		rs.cull_mode = RDC::POLYGON_CULL_DISABLED;

		pipeline = Rendering::PipelineBuilder{}
			.set_shader({
				"assets://shaders/shader_playground/fullscreen_quad.vert",
				"assets://shaders/shader_playground/fullscreen_quad.frag"
			}, "fullscreen_quad_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(depth_state)
			.set_rasterization_state(rs)
			.build(framebuffer_format);

		playground_ubo.create(device, "Playground UBO");

		uniform_set_0 = Rendering::UniformSetBuilder{}
			.add(playground_ubo.as_uniform(0))
			.build(device, pipeline.shader_rid, 0);

		wsi->submit_transfer_workers();
		return wsi->pre_frame_loop();
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
		device->imgui_begin_frame();

		PlaygroundUBO ubo_data{};
		ubo_data.resolution = { device->screen_get_width(), device->screen_get_height() };
		ubo_data.time       = static_cast<float>(elapsed_time);
		playground_ubo.upload(device, ubo_data);

		auto quad_drawable = Rendering::Drawable::make(
			pipeline, quad_mesh, Rendering::PushConstantData{}, { { uniform_set_0, 0 } });

		fg.reset();
		bb.reset();

		add_playground_pass(fg, bb,
			{ device->screen_get_width(), device->screen_get_height() },
			quad_drawable, *mesh_storage);
		Rendering::add_imgui_pass(fg, bb,
			{ device->screen_get_width(), device->screen_get_height() });
		Rendering::add_blit_pass(fg, bb);

		fg.compile();

		Rendering::RenderContext rc;
		rc.command_buffer = device->get_current_command_buffer();
		rc.device         = device;
		rc.wsi            = wsi;
		fg.execute(&rc, &rc);
	}

	void teardown_application() override
	{
		mesh_storage->finalize();
	}

private:
	Rendering::WSI*             wsi    = nullptr;
	Rendering::RenderingDevice* device = nullptr;

	Rendering::UniformBuffer<PlaygroundUBO> playground_ubo;

	Rendering::Pipeline   pipeline;
	Rendering::MeshHandle quad_mesh;

	RIDHandle uniform_set_0;

	std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();

	FrameGraph           fg;
	FrameGraphBlackboard bb;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try {
			auto* app = new ShaderPlayground();
			return app;
		}
		catch (const std::exception& e) {
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}
