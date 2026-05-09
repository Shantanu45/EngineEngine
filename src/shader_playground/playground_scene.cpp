#include "shader_playground/playground_scene.h"

#include "forward/default_samplers.h"
#include "rendering/default_textures.h"
#include "rendering/pipeline_builder.h"
#include "rendering/primitve_shapes.h"
#include "rendering/render_passes/common.h"

#include <glm/gtc/matrix_transform.hpp>

namespace ShaderPlayground {

struct PlaygroundPassResource : public blit_scene_input_resource {
	FrameGraphResource framebuffer_resource;
};

PlaygroundObject& PlaygroundObject::texture(uint32_t binding, const std::string& path, bool srgb)
{
	bindings.texture(binding, path, srgb);
	return *this;
}

PlaygroundObject& PlaygroundObject::texture_linear(uint32_t binding, const std::string& path)
{
	bindings.texture_linear(binding, path);
	return *this;
}

PlaygroundObject& PlaygroundObject::position(glm::vec3 value)
{
	local_position = value;
	return *this;
}

PlaygroundObject& PlaygroundObject::rotation(glm::vec3 degrees)
{
	local_rotation_degrees = degrees;
	return *this;
}

PlaygroundObject& PlaygroundObject::scale(glm::vec3 value)
{
	local_scale = value;
	return *this;
}

bool PlaygroundScene::initialize(Rendering::WSI* p_wsi, FileSystem::Filesystem& filesystem)
{
	wsi = p_wsi;
	device = wsi->get_rendering_device();

	configure_wsi();
	mesh_storage.initialize(device);
	textures.init(device, filesystem);

	sampler = RIDHandle(Rendering::create_sampler_linear_repeat(device));
	missing_texture = RIDHandle(Rendering::create_missing_texture(device));

	create_frame_resources();
	create_builtin_meshes();

	scene_camera.set_perspective(
		60.0f,
		static_cast<float>(device->screen_get_width()) / static_cast<float>(device->screen_get_height()),
		0.1f,
		100.0f);
	scene_camera.set_position(glm::vec3(0.0f, 0.0f, 3.0f));

	wsi->submit_transfer_workers();
	return wsi->pre_frame_loop();
}

void PlaygroundScene::shutdown()
{
	objects.clear();
	frame_ubo.free(device);
	missing_texture.reset();
	sampler.reset();
	textures.free_all();
	mesh_storage.finalize();
}

void PlaygroundScene::clear()
{
	objects.clear();
}

PlaygroundObject& PlaygroundScene::fullscreen(const std::string& fragment_shader)
{
	return add_object(
		PlaygroundMesh::Fullscreen,
		"assets://shaders/shader_playground/fullscreen.vert",
		fragment_shader,
		false);
}

PlaygroundObject& PlaygroundScene::quad(const std::string& vertex_shader, const std::string& fragment_shader)
{
	return add_object(PlaygroundMesh::Quad, vertex_shader, fragment_shader, true);
}

PlaygroundObject& PlaygroundScene::plane(const std::string& vertex_shader, const std::string& fragment_shader)
{
	return add_object(PlaygroundMesh::Plane, vertex_shader, fragment_shader, true);
}

PlaygroundObject& PlaygroundScene::cube(const std::string& vertex_shader, const std::string& fragment_shader)
{
	return add_object(PlaygroundMesh::Cube, vertex_shader, fragment_shader, true);
}

PlaygroundObject& PlaygroundScene::sphere(const std::string& vertex_shader, const std::string& fragment_shader)
{
	return add_object(PlaygroundMesh::Sphere, vertex_shader, fragment_shader, true);
}

void PlaygroundScene::camera(glm::vec3 position, glm::vec3 euler_degrees, float fov_degrees)
{
	scene_camera.set_perspective(
		fov_degrees,
		static_cast<float>(device->screen_get_width()) / static_cast<float>(device->screen_get_height()),
		0.1f,
		100.0f);
	scene_camera.set_position(position);
	scene_camera.set_euler_degrees(euler_degrees.x, euler_degrees.y, euler_degrees.z);
}

void PlaygroundScene::render(double frame_time, double elapsed_time)
{
	device->imgui_begin_frame();

	const uint32_t width = device->screen_get_width();
	const uint32_t height = device->screen_get_height();
	scene_camera.set_aspect(static_cast<float>(width) / static_cast<float>(height));

	PlaygroundFrameUBO frame_data{};
	frame_data.resolution = glm::vec2(width, height);
	frame_data.time = static_cast<float>(elapsed_time);
	frame_data.delta_time = static_cast<float>(frame_time);
	frame_data.view = scene_camera.get_view();
	frame_data.proj = scene_camera.get_projection();
	frame_ubo.upload(device, frame_data);

	std::vector<Rendering::Drawable> drawables;
	drawables.reserve(objects.size());
	for (auto& object : objects) {
		const RID user_set = object.bindings.build(1);
		Util::SmallVector<Rendering::UniformSetBinding> sets = { { object.frame_uniform_set, 0 } };
		if (user_set.is_valid()) {
			sets.push_back({ user_set, 1 });
		}

		drawables.push_back(Rendering::Drawable::make(
			object.pipeline,
			object.mesh,
			object.uses_object_push_constants ? object_push_constants(object) : Rendering::PushConstantData{},
			std::move(sets)));
	}

	auto cmd = device->get_current_command_buffer();
	device->_submit_transfer_workers(cmd);
	device->_submit_transfer_barriers(cmd);

	fg.reset();
	bb.reset();
	create_scene_pass(fg, bb, { width, height }, drawables);
	Rendering::add_imgui_pass(fg, bb, { width, height });
	Rendering::add_blit_pass(fg, bb, bb.get<PlaygroundPassResource>());
	fg.compile();

	Rendering::RenderContext rc;
	rc.command_buffer = device->get_current_command_buffer();
	rc.device = device;
	rc.wsi = wsi;
	fg.execute(&rc, &rc);
}

PlaygroundObject& PlaygroundScene::add_object(
	PlaygroundMesh mesh_type,
	const std::string& vertex_shader,
	const std::string& fragment_shader,
	bool uses_object_push_constants)
{
	Rendering::MeshHandle mesh = fullscreen_mesh;
	if (mesh_type == PlaygroundMesh::Quad) mesh = quad_mesh;
	if (mesh_type == PlaygroundMesh::Plane) mesh = plane_mesh;
	if (mesh_type == PlaygroundMesh::Cube) mesh = cube_mesh;
	if (mesh_type == PlaygroundMesh::Sphere) mesh = sphere_mesh;

	Rendering::RenderingDeviceCommons::PipelineDepthStencilState depth_state;
	depth_state.enable_depth_test = uses_object_push_constants;
	depth_state.enable_depth_write = uses_object_push_constants;
	depth_state.depth_compare_operator = Rendering::RenderingDeviceCommons::COMPARE_OP_LESS_OR_EQUAL;

	Rendering::RenderingDeviceCommons::PipelineRasterizationState rs;
	rs.cull_mode = Rendering::RenderingDeviceCommons::POLYGON_CULL_DISABLED;

	auto pipeline = Rendering::PipelineBuilder{}
		.set_shader({ vertex_shader, fragment_shader }, "shader_playground")
		.set_vertex_format(wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT))
		.set_depth_stencil_state(depth_state)
		.set_rasterization_state(rs)
		.build(framebuffer_format);

	objects.emplace_back();
	auto& object = objects.back();
	object.pipeline = pipeline;
	object.mesh = mesh;
	object.mesh_type = mesh_type;
	object.uses_object_push_constants = uses_object_push_constants;
	object.frame_uniform_set = RIDHandle(Rendering::UniformSetBuilder{}
		.add(frame_ubo.as_uniform(0))
		.build(device, pipeline.shader_rid, 0));
	object.bindings.initialize(device, &textures, pipeline.shader_rid, sampler);
	return object;
}

void PlaygroundScene::configure_wsi()
{
	using VertexDataMode = Rendering::WSI::VERTEX_DATA_MODE;
	wsi->set_vertex_data_mode(static_cast<VertexDataMode>(0));
	wsi->set_index_buffer_format(Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32);
	wsi->create_new_vertex_format(
		wsi->get_default_vertex_attribute(),
		Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

	Rendering::RenderingDevice::AttachmentFormat color;
	color.format = Rendering::RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
	color.usage_flags =
		Rendering::RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
		Rendering::RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

	Rendering::RenderingDevice::AttachmentFormat depth;
	depth.format = Rendering::RenderingDevice::DATA_FORMAT_D32_SFLOAT;
	depth.usage_flags = Rendering::RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	framebuffer_format = device->framebuffer_format_create({ color, depth });
}

void PlaygroundScene::create_frame_resources()
{
	frame_ubo.create(device, "Playground frame UBO");
}

void PlaygroundScene::create_builtin_meshes()
{
	fullscreen_mesh = Rendering::Shapes::upload_quad(*wsi, mesh_storage, "playground_fullscreen_quad");
	quad_mesh = Rendering::Shapes::upload_quad(*wsi, mesh_storage, "playground_quad");
	plane_mesh = Rendering::Shapes::upload_plane(*wsi, mesh_storage, 1, "playground_plane");
	cube_mesh = Rendering::Shapes::upload_cube(*wsi, mesh_storage, "playground_cube");
	sphere_mesh = Rendering::Shapes::upload_sphere(*wsi, mesh_storage, 32, 32, "playground_sphere");
}

void PlaygroundScene::create_scene_pass(FrameGraph& frame_graph, FrameGraphBlackboard& blackboard, Size2i extent, const std::vector<Rendering::Drawable>& drawables)
{
	blackboard.add<PlaygroundPassResource>() =
		frame_graph.add_callback_pass<PlaygroundPassResource>(
			"Shader Playground",
			[&](FrameGraph::Builder& builder, PlaygroundPassResource& data)
			{
				Rendering::RenderingDevice::TextureFormat tf;
				tf.texture_type = Rendering::RenderingDevice::TEXTURE_TYPE_2D;
				tf.width = extent.x;
				tf.height = extent.y;
				tf.usage_bits =
					Rendering::RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
					Rendering::RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT;
				tf.format = Rendering::RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
				data.scene = builder.create<Rendering::FrameGraphTexture>("playground scene", { tf, Rendering::RenderingDevice::TextureView(), "playground scene" });

				Rendering::RenderingDevice::TextureFormat tf_depth;
				tf_depth.texture_type = Rendering::RenderingDevice::TEXTURE_TYPE_2D;
				tf_depth.width = extent.x;
				tf_depth.height = extent.y;
				tf_depth.usage_bits = Rendering::RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				tf_depth.format = Rendering::RenderingDevice::DATA_FORMAT_D32_SFLOAT;
				data.depth = builder.create<Rendering::FrameGraphTexture>("playground depth", { tf_depth, Rendering::RenderingDevice::TextureView(), "playground depth" });

				data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
				data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
				data.framebuffer_resource = builder.create<Rendering::FrameGraphFramebuffer>(
					"playground framebuffer",
					{
						.build = [&frame_graph, scene_id = data.scene, depth_id = data.depth](Rendering::RenderContext& rc) -> RID {
							auto& scene_tex = frame_graph.get_resource<Rendering::FrameGraphTexture>(scene_id);
							auto& depth_tex = frame_graph.get_resource<Rendering::FrameGraphTexture>(depth_id);
							return rc.device->framebuffer_create({ scene_tex.texture_rid, depth_tex.texture_rid });
						},
						.name = "playground framebuffer"
					});
				data.framebuffer_resource = builder.write(data.framebuffer_resource, FrameGraph::kFlagsIgnored);
			},
			[=, this](const PlaygroundPassResource& data, FrameGraphPassResources& resources, void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto cmd = rc.command_buffer;
				RID framebuffer = resources.get<Rendering::FrameGraphFramebuffer>(data.framebuffer_resource).framebuffer_rid;

				std::array<Rendering::RenderingDeviceDriver::RenderPassClearValue, 2> clear_values;
				clear_values[0].color = Color(0.02f, 0.02f, 0.025f, 1.0f);
				clear_values[1].depth = 1.0f;
				clear_values[1].stencil = 0.0f;

				rc.device->begin_render_pass_from_frame_buffer(framebuffer, Rect2i(0, 0, extent.x, extent.y), clear_values);
				for (const auto& drawable : drawables) {
					Rendering::submit_drawable(rc, cmd, drawable, mesh_storage);
				}
				rc.wsi->end_render_pass(cmd);
			});
}

Rendering::PushConstantData PlaygroundScene::object_push_constants(const PlaygroundObject& object) const
{
	glm::mat4 model(1.0f);
	model = glm::translate(model, object.local_position);
	model = glm::rotate(model, glm::radians(object.local_rotation_degrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(object.local_rotation_degrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(object.local_rotation_degrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::scale(model, object.local_scale);

	PlaygroundObjectPC pc;
	pc.model = model;
	pc.normal_matrix = glm::transpose(glm::inverse(model));
	return Rendering::PushConstantData::from(pc);
}

} // namespace ShaderPlayground
