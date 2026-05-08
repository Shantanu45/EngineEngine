#include "forward/forward_runtime.h"

#include "forward/forward_scene_setup.h"
#include "forward/forward_ui_setup.h"
#include "forward/scene/transform_system.h"
#include "rendering/primitve_shapes.h"
#include "rendering/utils.h"
#include "util/profiler.h"

#include <array>
#include <cstdint>
#include <string>

bool ForwardRuntime::initialize(const ForwardRuntimeConfig& config)
{
	wsi = config.wsi;
	device = wsi->get_rendering_device();
	input_system = config.input_system;
	render_imgui = config.enable_default_ui;

	RenderUtilities::capturing_timestamps = false;

	configure_camera(config.camera);
	configure_wsi();
	create_default_resources(config.filesystem, config.skybox_faces);
	load_scene(config.filesystem, config.scene_path, config.scene_name_prefix, config.enable_default_lights);
	if (config.enable_default_ui)
		register_default_ui();

	wsi->submit_transfer_workers();
	return wsi->pre_frame_loop();
}

void ForwardRuntime::render_frame(double frame_time, double elapsed_time)
{
	ZoneScoped;
	camera.update_from_input(input_system.get(), frame_time);
	update_world_transforms(world);
	RenderUtilities::capturing_timestamps = render_settings.show_timings;

	if (render_imgui) {
		device->imgui_begin_frame();
		ui_layer.draw_frame(ui_ctx);
	}

	RenderSceneExtractResult extracted_scene = scene_extractor.extract(RenderSceneExtractInput{
		.world             = world,
		.camera            = camera,
		.settings          = render_settings,
		.asset_registry    = resources.assets(),
		.material_registry = resources.materials(),
		.device            = device,
		.skybox_mesh       = skybox_mesh,
		.grid_mesh         = grid_mesh,
		.extent            = { device->screen_get_width(), device->screen_get_height() },
		.elapsed           = elapsed_time,
	});
	render_stats = extracted_scene.stats;
	const Rendering::SceneView& view = extracted_scene.view;

	TracyPlot("Draw Calls", (int64_t)render_stats.draw_count);
	render_pipeline.render(view, resources.meshes(), render_imgui);
}

void ForwardRuntime::shutdown()
{
	render_pipeline.shutdown();
	resources.shutdown();
}

void ForwardRuntime::configure_camera(const ForwardCameraConfig& camera_config)
{
	camera.set_perspective(
		camera_config.fov_degrees,
		camera_config.aspect,
		camera_config.near_plane,
		camera_config.far_plane);
	camera.set_reset_on_resize(camera_config.reset_aspect_on_resize);
	camera.set_position(camera_config.position);
	camera.set_euler_degrees(
		camera_config.euler_degrees.x,
		camera_config.euler_degrees.y,
		camera_config.euler_degrees.z);
	camera.set_mode(camera_config.mode);
}

void ForwardRuntime::configure_wsi()
{
	using VertexDataMode = Rendering::WSI::VERTEX_DATA_MODE;
	wsi->set_vertex_data_mode(static_cast<VertexDataMode>(0));
	wsi->set_index_buffer_format(Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32);
	wsi->create_new_vertex_format(
		wsi->get_default_vertex_attribute(),
		Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
}

void ForwardRuntime::create_default_resources(FileSystem::Filesystem& filesystem, const std::array<std::string, 6>& skybox_faces)
{
	resources.initialize(device, filesystem);

	grid_mesh        = Rendering::Shapes::upload_grid(*wsi, resources.meshes(), 10, 1, "object_grid");
	skybox_mesh      = Rendering::Shapes::upload_cube(*wsi, resources.meshes(), "skybox_cube");
	light_mesh       = Rendering::Shapes::upload_cube(*wsi, resources.meshes(), "light_cube");
	point_light_mesh = Rendering::Shapes::upload_cube(*wsi, resources.meshes(), "point_light_cube");
	light_mesh_asset = resources.assets().register_mesh(light_mesh);
	point_light_mesh_asset = resources.assets().register_mesh(point_light_mesh);

	resources.load_skybox_cubemap(skybox_faces);

	render_pipeline.initialize(wsi, device, resources.skybox_cubemap());
}

void ForwardRuntime::load_scene(FileSystem::Filesystem& filesystem, const std::string& path, const std::string& name_prefix, bool add_default_lights)
{
	auto vfmt = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
	instantiate_forward_gltf_scene(ForwardGltfSceneRequest{
		.world             = world,
		.filesystem        = filesystem,
		.device            = device,
		.resources         = resources,
		.fallback_texture  = resources.default_white_texture(),
		.shader_rid        = render_pipeline.color_pipeline().shader_rid,
		.pbr_shader_rid    = render_pipeline.pbr_color_pipeline().shader_rid,
		.vertex_format     = vfmt,
		.path              = path,
		.name_prefix       = name_prefix,
	});

	if (add_default_lights) {
		add_default_forward_lights(world, ForwardDemoLightMeshes{
			.directional = light_mesh_asset,
			.point       = point_light_mesh_asset,
		});
	}
}

void ForwardRuntime::register_default_ui()
{
	ui_ctx.camera   = &camera;
	ui_ctx.world    = &world;
	ui_ctx.wsi      = wsi;
	ui_ctx.settings = &render_settings;
	ui_ctx.stats    = &render_stats;

	register_default_forward_panels(ui_layer);
}
