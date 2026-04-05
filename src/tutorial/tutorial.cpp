#include "application/common.h"

#include "rendering/image_loader.h"
#include "rendering/pipeline_builder.h"
#include "rendering/render_passes/common.h"
#include "rendering/camera.h"
#include "input/input.h"
#include "util/timer.h"
#include "rendering/primitve_shapes.h"

struct alignas(16) Camera_UBO {
	glm::mat4 model;
	glm::mat4 view_projection;
};

struct alignas(16) Material_UBO {
	float shininess;
};

struct Light_UBO {
	glm::vec4 position;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
};

struct GridPushConstants {
	glm::mat4 mvp;
	glm::vec3 camera_pos;
	float     pad;          // vec3 needs 16-byte alignment in GLSL
};

void add_basic_pass(FrameGraph& fg, FrameGraphBlackboard& bb,
	Size2i extent,
	std::vector<RID> pipelines,
	RID uniform_set,
	std::vector<Rendering::MeshHandle> mesh_handles,
	Camera_UBO camera_pc, 
	glm::vec3 cam_pos)
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

				data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
			},
			[=](const basic_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				Camera_UBO cube_pc = camera_pc; // mutable local copy
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto cmd = rc.command_buffer;

				auto& scene_tex = resources.get<Rendering::FrameGraphTexture>(data.scene);

				uint32_t w = rc.device->screen_get_width();
				uint32_t h = rc.device->screen_get_height();

				Rect2i viewport(0, 0, w, h);

				RID frame_buffer = rc.device->framebuffer_create({ scene_tex.texture_rid});

				GPU_SCOPE(cmd, "Basic Pass", Color(1.0, 0.0, 0.0, 1.0));
				std::array<RDD::RenderPassClearValue, 2> clear_values;
				clear_values[0].color = Color();
				clear_values[1].depth = 1.0;
				clear_values[1].stencil = 0.0;

				GridPushConstants pc{};
				pc.mvp = camera_pc.view_projection * camera_pc.model;
				pc.camera_pos = cam_pos;

				rc.device->begin_render_pass_from_frame_buffer(frame_buffer, viewport, clear_values);
				rc.device->set_push_constant(&pc, sizeof(GridPushConstants), rc.device->get_shader_rid("grid_shader"));
				rc.device->bind_render_pipeline(cmd, pipelines[2]);
				//rc.device->get_driver().command_render_set_line_width(rc.device->get_current_command_buffer(), 1);

				rc.wsi->draw_mesh(cmd, mesh_handles[2]);

				rc.device->bind_render_pipeline(cmd, pipelines[0]);
				rc.device->set_push_constant(&camera_pc, sizeof(Camera_UBO), rc.device->get_shader_rid("light_map"));
				rc.device->bind_uniform_set( rc.device->get_shader_rid("light_map"), uniform_set, 0);

				rc.wsi->draw_mesh(cmd, mesh_handles[0]);

				rc.device->bind_render_pipeline(cmd, pipelines[1]);
				glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
				cube_pc.model = glm::translate(glm::mat4(1.0f), lightPos);
				rc.device->set_push_constant(&cube_pc, sizeof(Camera_UBO), rc.device->get_shader_rid("cube_shader"));

				rc.wsi->draw_mesh(cmd, mesh_handles[1]);




				rc.wsi->end_render_pass(cmd);

			});
}


struct TutorialApplication : EE::Application
{

	bool pre_frame() override
	{
		input_system = Services::get().get<EE::InputSystemInterface>();
		RenderUtilities::capturing_timestamps = true;

		camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.set_reset_on_resize();
		camera.set_mode(CameraMode::Fly);

		wsi = get_wsi();
		device = wsi->get_rendering_device();

		wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
		wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);

		wsi->create_new_vertex_format(wsi->get_default_vertex_attribute(), Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
		auto vertex_format = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

		light_mesh = Rendering::Shapes::upload_cube(*wsi, "light_cube");
		object_mesh = Rendering::Shapes::upload_cube(*wsi, "object_cube");
		grid_mesh = Rendering::Shapes::upload_grid(*wsi, 10, 1, "object_grid");

		// Create frame buffer format

		std::vector<RD::AttachmentFormat> attachments;

		RD::AttachmentFormat color;
		color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		attachments.push_back(color);

		auto framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);

		pipeline_color = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/light_map.vert", "assets://shaders/light_map.frag" }, "light_map")
			.set_vertex_format(vertex_format)
			.build(framebuffer_format);

		material_ubo = device->uniform_buffer_create(sizeof(Material_UBO));
		light_ubo = device->uniform_buffer_create(sizeof(Light_UBO));
		view_ubo = device->uniform_buffer_create(sizeof(glm::vec4));

		auto fs = Services::get().get<FilesystemInterface>();
		Rendering::ImageLoader img_loader(*fs);
		// image
		auto diffuse_image = img_loader.load_from_file("assets://textures/container2.png");
		auto specular_image = img_loader.load_from_file("assets://textures/container2_specular.png");

		RDC::TextureFormat tf;
		tf.width = diffuse_image.width;
		tf.height = diffuse_image.height;
		tf.array_layers = 1;
		tf.texture_type = RDC::TEXTURE_TYPE_2D;
		tf.usage_bits = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

		diffuse_uniform = device->texture_create(tf, RD::TextureView(), { diffuse_image.pixels });
		device->set_resource_name(diffuse_uniform, "Diffuse texture");

		specular_uniform = device->texture_create(tf, RD::TextureView(), { specular_image.pixels });
		device->set_resource_name(specular_uniform, "Specular texture");

		auto sampler = device->sampler_create(RD::SamplerState());

		std::vector<RD::Uniform> uniforms;

		RD::Uniform u;
		u.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(material_ubo);
		uniforms.push_back(u);

		RD::Uniform lu;
		lu.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
		lu.binding = 1;
		lu.append_id(light_ubo);
		uniforms.push_back(lu);

		RD::Uniform vu;
		vu.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
		vu.binding = 2;
		vu.append_id(view_ubo);
		uniforms.push_back(vu);

		RD::Uniform du;
		du.uniform_type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		du.binding = 3;
		du.append_id(sampler);
		du.append_id(diffuse_uniform);
		uniforms.push_back(du);

		RD::Uniform su;
		su.uniform_type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		su.binding = 4;
		su.append_id(sampler);
		su.append_id(specular_uniform);
		uniforms.push_back(su);

		uniform_set = device->uniform_set_create(uniforms, device->get_shader_rid("light_map"), 0);

		pipeline_light = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/light_cube.vert", "assets://shaders/light_cube.frag" }, "cube_shader")
			.set_vertex_format(vertex_format)
			.build(framebuffer_format);

		pipeline_grid = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/grid.vert", "assets://shaders/grid.frag" }, "grid_shader")
			.set_vertex_format(vertex_format)
			.set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
			.build(framebuffer_format);


		wsi->submit_transfer_workers();
		return wsi->pre_frame_loop();
	}

	void render_frame(double frame_time, double elapsed_time) override
	{

		camera.update_from_input(input_system.get(), frame_time);

		Camera_UBO camera_pc{};
		camera_pc.model = glm::mat4(1.0f); // identity for now
		camera_pc.view_projection = camera.get_view_projection();

		Material_UBO mat;
		mat.shininess = 32;

		Light_UBO light;
		light.ambient = {0.1, 0.1, 0.0, 0.0 };
		light.diffuse =	 {0.5, 0.5, 0.0, 0.0 };
		light.position = {1.0, 1.0, 1.0, 0.0};
		light.specular = {1.0, 1.0, 1.0, 0.0 };

		device->buffer_update(material_ubo, 0, sizeof(Material_UBO), &mat);
		device->buffer_update(light_ubo, 0, sizeof(Light_UBO), &light);
		glm::vec4 cam_pos = glm::vec4(camera.get_position(), 0.0);
		device->buffer_update(view_ubo, 0, sizeof(glm::vec4), &cam_pos);

		device->imgui_begin_frame();
		const auto timer = Services::get().get<Util::FrameTimer>();

		ImGui::Text("FPS: %.1f", timer->get_fps());
		ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);

		fg.reset();
		bb.reset();

		add_basic_pass(fg, bb, { device->screen_get_width(), device->screen_get_height() }, { pipeline_color, pipeline_light, pipeline_grid}, uniform_set, { object_mesh, light_mesh, grid_mesh }, camera_pc, camera.get_position());
		Rendering::add_imgui_pass(fg, bb, { device->screen_get_width(), device->screen_get_height() });
		Rendering::add_blit_pass(fg, bb);

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

		device->free_rid(material_ubo);
		device->free_rid(light_ubo);
		device->free_rid(diffuse_uniform);
		device->free_rid(specular_uniform);
		device->free_rid(view_ubo);
		device->free_rid(pipeline_color);
	}

private:
	RID material_ubo;
	RID light_ubo;
	RID view_ubo;
	RID uniform_set;

	RID diffuse_uniform;
	RID specular_uniform;

	Rendering::MeshPrimitive prim;

	RID pipeline_color;
	RID pipeline_light;
	RID pipeline_grid;
	Camera camera;

	std::shared_ptr<EE::InputSystemInterface> input_system;

	Rendering::MeshHandle light_mesh;
	Rendering::MeshHandle object_mesh;
	Rendering::MeshHandle grid_mesh;

	Rendering::WSI* wsi;
	Rendering::RenderingDevice* device;

	// ---- Build the frame graph ----
	FrameGraph fg;
	FrameGraphBlackboard bb;
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try
		{
			auto* app = new TutorialApplication();
			return app;
		}
		catch (const std::exception& e)
		{
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}