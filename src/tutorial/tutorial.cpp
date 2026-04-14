#include "application/common.h"

#include "rendering/image_loader.h"
#include "rendering/pipeline_builder.h"
#include "rendering/render_passes/common.h"
#include "rendering/camera.h"
#include "input/input.h"
#include "util/timer.h"
#include "rendering/primitve_shapes.h"
#include "rendering/drawable.h"
#include "rendering/uniform_buffer.h"
#include "rendering/uniform_set_builder.h"
#include "rendering/default_textures.h"
#include "entt/entt.hpp"
#include "scene/components.h"

/**
 *  Set 0 - Per-frame global data   (camera, time, lights)
    Set 1 - Per-pass data           (shadow maps, render targets)
    Set 2 - Per-material data       (textures, material params)
    Set 3 - Per-object data         (model matrix, bone data).
 */

struct alignas(16) CameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
    float     _pad;
};

struct alignas(16) FrameData_UBO {
    CameraData camera;
    float      time;
    float      _pad[3];
    glm::mat4  light_space_matrix;
};

struct alignas(16) ObjectData_UBO {
    glm::mat4 model;
    glm::mat4 normalMatrix;
};

struct alignas(16) PointShadowUBO {
    glm::mat4 shadowMatrices[6];
    glm::vec4 lightPos;   // xyz = pos, w = farPlane
};

struct point_shadow_pass_resource {
    FrameGraphResource shadow_cubemap;
};

void add_point_shadow_pass(
	FrameGraph& fg,
	FrameGraphBlackboard& bb,
	uint32_t face_size,
	std::vector<Rendering::Drawable> drawables,
	Rendering::MeshStorage& storage)
{
	bb.add<point_shadow_pass_resource>() =
		fg.add_callback_pass<point_shadow_pass_resource>(
			"Point Shadow Pass",
			[&](FrameGraph::Builder& builder, point_shadow_pass_resource& data)
			{
				RD::TextureFormat tf;
				tf.texture_type = RD::TEXTURE_TYPE_CUBE;   // cubemap!
				tf.width = face_size;
				tf.height = face_size;
				tf.array_layers = 6;
				tf.usage_bits =
					RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
					RD::TEXTURE_USAGE_SAMPLING_BIT;
				tf.format = RD::DATA_FORMAT_D32_SFLOAT;

				data.shadow_cubemap = builder.create<Rendering::FrameGraphTexture>(
					"point shadow cubemap",
					{ tf, RD::TextureView(), "point shadow cubemap" });

				data.shadow_cubemap = builder.write(
					data.shadow_cubemap, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
			},
			[=, &storage](const point_shadow_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto  cmd = rc.command_buffer;

				auto& cube_tex = resources.get<Rendering::FrameGraphTexture>(data.shadow_cubemap);

				// Render into all 6 faces at once � the geometry shader
				// writes gl_Layer to fan triangles across each face.
				RID fb = rc.device->framebuffer_create({ cube_tex.texture_rid });

				GPU_SCOPE(cmd, "Point Shadow Pass", Color(0.8f, 0.2f, 1.0f, 1.0f));

				std::array<RDD::RenderPassClearValue, 1> clear_values;
				clear_values[0].depth = 1.0f;
				clear_values[0].stencil = 0;

				rc.device->begin_render_pass_from_frame_buffer(
					fb,
					Rect2i(0, 0, face_size, face_size),
					clear_values);

				for (const auto& drawable : drawables)
					submit_drawable(rc, cmd, drawable, storage);

				rc.wsi->end_render_pass(cmd);
			});
}

struct shadow_pass_resource {
	FrameGraphResource shadow_map;
};

void add_basic_pass(
    FrameGraph& fg,
    FrameGraphBlackboard& bb,
    Size2i extent,
    std::vector<Rendering::Drawable> drawables,
    Rendering::MeshStorage& storage,
    RID shadow_sampler,
    RID point_shadow_sampler,
    Rendering::Pipeline light_map_pipeline)
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

				data.depth = builder.create<Rendering::FrameGraphTexture>("scene depth texture", { tf_depth, RD::TextureView(), "scene depth texture" });

				data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
				data.depth = builder.write(data.depth, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);

				auto& shadow_res = bb.get<shadow_pass_resource>();
				data.shadow_map_in = builder.read(shadow_res.shadow_map, TEXTURE_READ_FLAGS::READ_DEPTH);
				auto& point_shadow_res = bb.get<point_shadow_pass_resource>();
				data.point_shadow_in = builder.read(point_shadow_res.shadow_cubemap, TEXTURE_READ_FLAGS::READ_DEPTH);
            },
            [=, &storage](const basic_pass_resource& data, FrameGraphPassResources& resources, void* ctx)
            {
                auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
                auto  cmd = rc.command_buffer;

                auto& scene_tex = resources.get<Rendering::FrameGraphTexture>(data.scene);
				auto& depth_tex = resources.get<Rendering::FrameGraphTexture>(data.depth);

				auto& shadow_tex = resources.get<Rendering::FrameGraphTexture>(data.shadow_map_in);
				auto& point_shadow_tex = resources.get<Rendering::FrameGraphTexture>(data.point_shadow_in);

				RID uniform_set_1 = Rendering::UniformSetBuilder{}
					.add_texture(0, shadow_sampler, shadow_tex.texture_rid)
					.add_texture(1, point_shadow_sampler, point_shadow_tex.texture_rid)
					.build(rc.device, light_map_pipeline.shader_rid, 1);

                uint32_t w = rc.device->screen_get_width();
                uint32_t h = rc.device->screen_get_height();

				RID frame_buffer = rc.device->framebuffer_create({ scene_tex.texture_rid, depth_tex.texture_rid });

				GPU_SCOPE(cmd, "Basic Pass", Color(1.0, 0.0, 0.0, 1.0));
				std::array<RDD::RenderPassClearValue, 2> clear_values;
				clear_values[0].color = Color();
				clear_values[1].depth = 1.0;
				clear_values[1].stencil = 0.0;

                rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

                for (auto drawable : drawables)
                {
                    if (drawable.pipeline.pipeline_rid == light_map_pipeline.pipeline_rid)
                        drawable.set_uniform_set({ uniform_set_1, 1 });
                    submit_drawable(rc, cmd, drawable, storage);
                }

                rc.wsi->end_render_pass(cmd);
            });
}

void add_shadow_pass(
	FrameGraph& fg,
	FrameGraphBlackboard& bb,
	Size2i shadow_extent,
	std::vector<Rendering::Drawable> drawables,
	Rendering::MeshStorage& storage)
{
	bb.add<shadow_pass_resource>() =
		fg.add_callback_pass<shadow_pass_resource>(
			"Shadow Pass",
			[&](FrameGraph::Builder& builder, shadow_pass_resource& data)
			{
				RD::TextureFormat tf_depth;
				tf_depth.texture_type = RD::TEXTURE_TYPE_2D;
				tf_depth.width = shadow_extent.x;
				tf_depth.height = shadow_extent.y;
				tf_depth.usage_bits =
					RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
					RD::TEXTURE_USAGE_SAMPLING_BIT;        // must be sampled later
				tf_depth.format = RD::DATA_FORMAT_D32_SFLOAT;

				data.shadow_map = builder.create<Rendering::FrameGraphTexture>( "shadow map", { tf_depth, RD::TextureView(), "shadow map" });

				data.shadow_map = builder.write( data.shadow_map, TEXTURE_WRITE_FLAGS::WRITE_DEPTH);
			},
			[=, &storage](const shadow_pass_resource& data,
				FrameGraphPassResources& resources,
				void* ctx)
			{
				auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
				auto  cmd = rc.command_buffer;

				auto& shadow_tex = resources.get<Rendering::FrameGraphTexture>(data.shadow_map);

				RID fb = rc.device->framebuffer_create({ shadow_tex.texture_rid });

				GPU_SCOPE(cmd, "Shadow Pass", Color(1.0, 0.5, 0.0, 1.0));
				std::array<RDD::RenderPassClearValue, 1> clear_values;
				clear_values[0].depth = 1.0f;
				clear_values[0].stencil = 0;

				rc.device->begin_render_pass_from_frame_buffer( fb, Rect2i(0, 0, shadow_extent.x, shadow_extent.y), clear_values);

				for (const auto& drawable : drawables)
					submit_drawable(rc, cmd, drawable, storage);

				rc.wsi->end_render_pass(cmd);
			});
}

struct TutorialApplication : EE::Application
{
    bool pre_frame() override
    {
        input_system = Services::get().get<EE::InputSystemInterface>();
        RenderUtilities::capturing_timestamps = false;

        camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        camera.set_reset_on_resize();
        camera.set_mode(CameraMode::Fly);

        wsi = get_wsi();
        device = wsi->get_rendering_device();

        wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
        wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);

        wsi->create_new_vertex_format(
            wsi->get_default_vertex_attribute(),
            Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
        auto vertex_format = wsi->get_vertex_format_by_type(
            Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

        auto fs = Services::get().get<FilesystemInterface>();
        mesh_loader = std::make_unique<Rendering::MeshLoader>(*fs, device);
        mesh_storage->initialize(device);

		Rendering::ImageLoader img_loader(*fs);

        // --- Meshes ---
        light_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "light_cube");
        point_light_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "point_light_cube");
        object_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "object_cube");
        grid_mesh = Rendering::Shapes::upload_grid(*wsi, *mesh_storage, 10, 1, "object_grid");
        plane_mesh = Rendering::Shapes::upload_plane(*wsi, *mesh_storage, 1, "object_plane");
		skybox_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "skybox_cube");


        // --- Framebuffer format ---
        RD::AttachmentFormat color;
        color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
        color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

		RD::AttachmentFormat depth;
		depth.format = RD::DATA_FORMAT_D32_SFLOAT;
		depth.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        auto framebuffer_format = RD::get_singleton()->framebuffer_format_create({ color, depth });

        // --- Pipelines ---

		RDC::PipelineDepthStencilState depth_state;
		depth_state.enable_depth_test = true;
		depth_state.enable_depth_write = true;
		depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

        pipeline_color = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/light_map.vert",
                          "assets://shaders/light_map.frag" }, "light_map")
            .set_vertex_format(vertex_format)
            .set_depth_stencil_state(depth_state)
            .build(framebuffer_format);

        pipeline_light = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/light_cube.vert",
                          "assets://shaders/light_cube.frag" }, "cube_shader")
            .set_vertex_format(vertex_format)
            .set_depth_stencil_state(depth_state)
            .build(framebuffer_format);

        pipeline_grid = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/grid.vert",
                          "assets://shaders/grid.frag" }, "grid_shader")
            .set_vertex_format(vertex_format)
            .set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
            .set_depth_stencil_state(depth_state)
            .build(framebuffer_format);

        // --- UBOs ---
        frame_ubo.create(device, "Frame UBO");
        light_ubo.create(device, "Light UBO");

        // --- Sky box ---

		std::array<std::string, 6> faces = {
	        "assets://textures/skybox/right.jpg",
	        "assets://textures/skybox/left.jpg",
	        "assets://textures/skybox/top.jpg",
	        "assets://textures/skybox/bottom.jpg",
	        "assets://textures/skybox/front.jpg",
	        "assets://textures/skybox/back.jpg",
		};

		auto face0 = img_loader.load_from_file(faces[0]);

		RDC::TextureFormat cubemap_tf;
		cubemap_tf.width = face0.width;
		cubemap_tf.height = face0.height;
		cubemap_tf.array_layers = 6;                          // 6 faces
		cubemap_tf.texture_type = RDC::TEXTURE_TYPE_CUBE;     // cubemap type
		cubemap_tf.usage_bits = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
		cubemap_tf.format = RDC::DATA_FORMAT_R8G8B8A8_SRGB;

		// load all 6 faces
		std::vector<std::vector<uint8_t>> face_pixels;
		for (auto& path : faces) {
			auto img = img_loader.load_from_file(path);
			face_pixels.push_back(img.pixels);
		}

		cubemap_uniform = device->texture_create(cubemap_tf, RD::TextureView(), face_pixels);
		device->set_resource_name(cubemap_uniform, "Skybox cubemap");

		sampler_cube = device->sampler_create(RD::SamplerState());

		RDC::PipelineDepthStencilState skybox_depth;
		skybox_depth.enable_depth_test = true;
		skybox_depth.enable_depth_write = false;
		skybox_depth.depth_compare_operator = RDC::COMPARE_OP_LESS_OR_EQUAL;
        RDC::PipelineRasterizationState rs;
        rs.cull_mode = Rendering::RenderingDeviceCommons::POLYGON_CULL_DISABLED;


        pipeline_skybox = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/skybox.vert",
                          "assets://shaders/skybox.frag" }, "skybox_shader")
            .set_vertex_format(vertex_format)
            .set_depth_stencil_state(skybox_depth)
            .set_rasterization_state(rs)
			.build(framebuffer_format);


        // --- Textures ---

        auto diffuse_image = img_loader.load_from_file("assets://textures/container2.png");
        auto specular_image = img_loader.load_from_file("assets://textures/container2_specular.png");
        auto rock_image = img_loader.load_from_file("assets://textures/brickwall.jpg");
        auto rock_n_image = img_loader.load_from_file("assets://textures/brickwall_normal.jpg");

        fallback_texture = Rendering::create_white_texture(device);

        diffuse_uniform     = Rendering::upload_texture_2d(device, diffuse_image,  RDC::DATA_FORMAT_R8G8B8A8_SRGB,  "Diffuse texture");
        specular_uniform    = Rendering::upload_texture_2d(device, specular_image, RDC::DATA_FORMAT_R8G8B8A8_UNORM, "Specular texture");
        rock_normal_uniform = Rendering::upload_texture_2d(device, rock_n_image,   RDC::DATA_FORMAT_R8G8B8A8_UNORM, "Rock normal texture");
        rock_uniform        = Rendering::upload_texture_2d(device, rock_image,     RDC::DATA_FORMAT_R8G8B8A8_SRGB,  "Rock texture");

        sampler = device->sampler_create(RD::SamplerState());

		// sampler setting to use Sampler2DShadow in glsl
		RD::SamplerState shadow_samp;
		shadow_samp.mag_filter = RD::SAMPLER_FILTER_LINEAR;
		shadow_samp.min_filter = RD::SAMPLER_FILTER_LINEAR;
		shadow_samp.mip_filter = RD::SAMPLER_FILTER_NEAREST;
		shadow_samp.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		shadow_samp.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		shadow_samp.repeat_w = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		shadow_samp.enable_compare = true;
		shadow_samp.compare_op = RD::COMPARE_OP_LESS_OR_EQUAL;
		shadow_sampler = device->sampler_create(shadow_samp);

		// sampler for the point shadow cubemap — nearest, clamp, no comparison (manual in shader)
		RD::SamplerState ps_samp;
		ps_samp.mag_filter = RD::SAMPLER_FILTER_NEAREST;
		ps_samp.min_filter = RD::SAMPLER_FILTER_NEAREST;
		ps_samp.repeat_u   = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		ps_samp.repeat_v   = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		ps_samp.repeat_w   = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		point_shadow_sampler = device->sampler_create(ps_samp);

        // --- Uniform sets ---
        uniform_set_0 = Rendering::UniformSetBuilder{}
            .add(frame_ubo.as_uniform(0))
            .add(light_ubo.as_uniform(2))
            .build(device, pipeline_color.shader_rid, 0);

        uniform_set_0_light = Rendering::UniformSetBuilder{}
            .add(frame_ubo.as_uniform(0))
            .build(device, pipeline_light.shader_rid, 0);

		uniform_set_skybox = Rendering::UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add_texture(1, sampler_cube, cubemap_uniform)
			.build(device, pipeline_skybox.shader_rid, 0);

		// --- Shadow framebuffer format ---
		RD::AttachmentFormat shadow_depth_att;
		shadow_depth_att.format = RD::DATA_FORMAT_D32_SFLOAT;
		shadow_depth_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		auto shadow_fb_format = RD::get_singleton()->framebuffer_format_create({ shadow_depth_att });

		// --- Shadow pipeline ---
		RDC::PipelineDepthStencilState shadow_depth_state;
		shadow_depth_state.enable_depth_test = true;
		shadow_depth_state.enable_depth_write = true;
		shadow_depth_state.depth_compare_operator = RDC::COMPARE_OP_LESS;

		RDC::PipelineRasterizationState rs_shadow;
        rs_shadow.cull_mode = Rendering::RenderingDeviceCommons::POLYGON_CULL_FRONT;

		pipeline_shadow = Rendering::PipelineBuilder{}
			.set_shader({ "assets://shaders/shadow.vert",
						  "assets://shaders/shadow.frag" }, "shadow_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(shadow_depth_state)
			.set_rasterization_state(rs_shadow)
			.build(shadow_fb_format);

		// --- Uniform set for shadow pass (frame UBO only, set 0) ---
		uniform_set_0_shadow = Rendering::UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.build(device, pipeline_shadow.shader_rid, 0);

		// --- Point shadow cubemap framebuffer format ---
		// 6 layers, one per face � depth only
		RD::AttachmentFormat point_shadow_att;
		point_shadow_att.format = RD::DATA_FORMAT_D32_SFLOAT;
		point_shadow_att.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		// The FB format itself is still 2D; the cube is in the image
		auto point_shadow_fb_format = RD::get_singleton()->framebuffer_format_create({ point_shadow_att });

		RDC::PipelineDepthStencilState ps_depth;
		ps_depth.enable_depth_test = true;
		ps_depth.enable_depth_write = true;
		ps_depth.depth_compare_operator = RDC::COMPARE_OP_LESS;

		pipeline_point_shadow = Rendering::PipelineBuilder{}
			.set_shader({
				"assets://shaders/point_shadow.vert",
				"assets://shaders/point_shadow.geom",   // geometry shader
				"assets://shaders/point_shadow.frag"
				}, "point_shadow_shader")
			.set_vertex_format(vertex_format)
			.set_depth_stencil_state(ps_depth)
			.build(point_shadow_fb_format);

		point_shadow_ubo.create(device, "Point Shadow UBO");

		uniform_set_0_point_shadow = Rendering::UniformSetBuilder{}
			.add(frame_ubo.as_uniform(0))
			.add(point_shadow_ubo.as_uniform(1))
			.build(device, pipeline_point_shadow.shader_rid, 0);

        // --- Scene setup ---
        // 
		Rendering::Material mat;
		mat.diffuse = diffuse_uniform;
		mat.base_color_factor = glm::vec4(1.0f);
        mat.metallic_roughness = specular_uniform;
		mat.shininess = 64.0f;

        Rendering::MaterialHandle h = material_registry.create(device, std::move(mat), sampler, fallback_texture, pipeline_color.shader_rid);

		Rendering::Material mat_rock;
		mat_rock.diffuse = rock_uniform;
        mat_rock.normal = rock_normal_uniform;
		mat_rock.base_color_factor = glm::vec4(1.0f);
		mat_rock.shininess = 32.0f;
		Rendering::MaterialHandle h_rock = material_registry.create(device, std::move(mat_rock), sampler, fallback_texture, pipeline_color.shader_rid);

        // object cube
		for (int x = 0; x < 2; x++) {
			for (int z = 0; z < 2; z++) {
				auto entity = world.create();
				world.emplace<TransformComponent>(entity, TransformComponent{
					.position = glm::vec3(x * 2.5f, 0.5f, z * 2.5f) });
				world.emplace<MeshComponent>(entity, MeshComponent{
				   .mesh = object_mesh,
				   .pipeline = pipeline_color,
				   .uniform_sets = {uniform_set_0, {}, material_registry.get_uniform_set(h), {}}
				});
                world.emplace<MaterialComponent>(entity, MaterialComponent{ h });
			}
		}

		auto entity_plane = world.create();
		world.emplace<TransformComponent>(entity_plane, TransformComponent{
			.position = glm::vec3(0.0, 0.0, 0.0),
			.scale = glm::vec3(10.f) });
		world.emplace<MeshComponent>(entity_plane, MeshComponent{
            .mesh = plane_mesh,
            .pipeline = pipeline_color,
            .uniform_sets = {uniform_set_0, {}, material_registry.get_uniform_set(h_rock), {}}
            });
		world.emplace<MaterialComponent>(entity_plane, MaterialComponent{ h_rock });


        // light cube
  //      auto light = world.create();
  //      world.emplace<TransformComponent>(light, TransformComponent{
  //          .position = glm::vec3(5.0f, 10.0f, 5.0f),  // high up, centered over scene
	 //       .scale = glm::vec3(0.2f) });
  //      world.emplace<MeshComponent>(light, MeshComponent{
  //          light_mesh, pipeline_light, "cube_shader", uniform_set_0_light });
		//world.emplace<LightComponent>(light, LightComponent{ .data = {
  //          .position = glm::vec4(5.0f, 10.0f, 5.0f, 15.0f),
	 //       .direction = glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f),
		//	.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),         // w = intensity
		//	.type = static_cast<uint32_t>(LightType::Directional),
		//	.outer_angle = 0.0f,
		//} });

		auto point_light = world.create();
		world.emplace<TransformComponent>(point_light, TransformComponent{
			.position = glm::vec3(1.0f, 1.0f, 1.0f),  // high up, centered over scene
			.scale = glm::vec3(0.1f) });
		world.emplace<MeshComponent>(point_light, MeshComponent{
			.mesh = point_light_mesh, .pipeline = pipeline_light,
			.uniform_sets = {uniform_set_0_light} });
		world.emplace<LightComponent>(point_light, LightComponent{ .data = {
			.position = glm::vec4(1.0f, 1.0f, 1.0f, 15.0f),
			.direction = glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f),
			.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),         // w = intensity
			.type = static_cast<uint32_t>(LightType::Point),
			.outer_angle = 0.0f,
		} });

        wsi->submit_transfer_workers();
        return wsi->pre_frame_loop();
    }

    void render_frame(double frame_time, double elapsed_time) override
    {
        camera.update_from_input(input_system.get(), frame_time);

        upload_light_ubo();
        upload_frame_ubo(elapsed_time);
        upload_point_shadow_ubo();

        auto shadow_drawables       = build_shadow_drawables();
        auto point_shadow_drawables = build_point_shadow_drawables();
        auto main_drawables         = build_main_drawables();

        device->imgui_begin_frame();
        const auto timer = Services::get().get<Util::FrameTimer>();
        ImGui::Text("FPS: %.1f", timer->get_fps());
        ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);

        fg.reset();
        bb.reset();

        add_point_shadow_pass(fg, bb, 1024, point_shadow_drawables, *mesh_storage);
        add_shadow_pass(fg, bb, { 2048, 2048 }, shadow_drawables, *mesh_storage);
        add_basic_pass(fg, bb,
            { device->screen_get_width(), device->screen_get_height() }, main_drawables, *mesh_storage, shadow_sampler, point_shadow_sampler, pipeline_color);
        Rendering::add_imgui_pass(fg, bb,
            { device->screen_get_width(), device->screen_get_height() });
        Rendering::add_blit_pass(fg, bb);

        fg.compile();

        Rendering::RenderContext rc;
        rc.command_buffer = device->get_current_command_buffer();
        rc.device = device;
        rc.wsi = wsi;
        fg.execute(&rc, &rc);
    }

    // -----------------------------------------------------------------------
    // Per-frame upload helpers
    // -----------------------------------------------------------------------

    void upload_light_ubo()
    {
        LightBuffer buf{};
        world.view<TransformComponent, LightComponent>().each(
            [&](auto, TransformComponent& t, LightComponent& l) {
                if (buf.count >= MAX_LIGHTS) return;
                Light gpu_light = l.data;
                gpu_light.position = glm::vec4(t.position, l.data.position.w);
                buf.lights[buf.count++] = gpu_light;
            });
        light_ubo.upload(device, buf);
    }

    // Returns the light-space matrix for the first directional light found,
    // or identity if none exists.
    glm::mat4 compute_light_space_matrix()
    {
        glm::mat4 result = glm::mat4(1.0f);
        world.view<TransformComponent, LightComponent>().each(
            [&](auto, TransformComponent& t, LightComponent& l) {
                if (l.data.type != static_cast<uint32_t>(LightType::Directional)) return;
                glm::mat4 proj = glm::orthoRH_ZO(-8.0f, 8.0f, -8.0f, 8.0f, 1.0f, 30.0f);
                glm::mat4 view = glm::lookAt(
                    t.position,
                    t.position + glm::vec3(l.data.direction),
                    glm::vec3(0, 1, 0));
                result = proj * view;
            });
        return result;
    }

    void upload_frame_ubo(double elapsed_time)
    {
        FrameData_UBO data{};
        data.camera.view        = camera.get_view();
        data.camera.proj        = camera.get_projection();
        data.camera.cameraPos   = camera.get_position();
        data.time               = static_cast<float>(elapsed_time);
        data.light_space_matrix = compute_light_space_matrix();
        frame_ubo.upload(device, data);
    }

    void upload_point_shadow_ubo()
    {
        constexpr float ps_near = 0.1f;

        PointShadowUBO data{};
        world.view<TransformComponent, LightComponent>().each(
            [&](auto, TransformComponent& t, LightComponent& l) {
                if (l.data.type != static_cast<uint32_t>(LightType::Point)) return;
                glm::vec3 lp     = t.position;
                float     ps_far = l.data.position.w;  // match range used by light attenuation
                glm::mat4 proj   = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, ps_near, ps_far);
                data.shadowMatrices[0] = proj * glm::lookAt(lp, lp + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0));
                data.shadowMatrices[1] = proj * glm::lookAt(lp, lp + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0));
                data.shadowMatrices[2] = proj * glm::lookAt(lp, lp + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1));
                data.shadowMatrices[3] = proj * glm::lookAt(lp, lp + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1));
                data.shadowMatrices[4] = proj * glm::lookAt(lp, lp + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0));
                data.shadowMatrices[5] = proj * glm::lookAt(lp, lp + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0));
                data.lightPos = glm::vec4(lp, ps_far);
            });
        point_shadow_ubo.upload(device, data);
    }

    // -----------------------------------------------------------------------
    // Drawable builders
    // -----------------------------------------------------------------------

    std::vector<Rendering::Drawable> build_shadow_drawables()
    {
        std::vector<Rendering::Drawable> out;
        world.view<TransformComponent, MeshComponent>(entt::exclude<LightComponent>).each(
            [&](auto, TransformComponent& t, MeshComponent& m) {
                out.push_back(Rendering::Drawable::make(
                    pipeline_shadow, m.mesh,
                    Rendering::PushConstantData::from(ObjectData_UBO{ t.get_model(), t.get_normal_matrix() }),
                    { { uniform_set_0_shadow, 0 } }
                ));
            });
        return out;
    }

    std::vector<Rendering::Drawable> build_point_shadow_drawables()
    {
        std::vector<Rendering::Drawable> out;
        world.view<TransformComponent, MeshComponent>(entt::exclude<LightComponent>).each(
            [&](auto, TransformComponent& t, MeshComponent& m) {
                out.push_back(Rendering::Drawable::make(
                    pipeline_point_shadow, m.mesh,
                    Rendering::PushConstantData::from(ObjectData_UBO{ t.get_model(), t.get_normal_matrix() }),
                    { { uniform_set_0_point_shadow, 0 } }
                ));
            });
        return out;
    }

    std::vector<Rendering::Drawable> build_main_drawables()
    {
        material_registry.upload_all(device);
        std::vector<Rendering::Drawable> out;

        glm::mat4 identity = glm::mat4(1.0f);

        out.push_back(Rendering::Drawable::make(pipeline_skybox, skybox_mesh,
            Rendering::PushConstantData::from(ObjectData_UBO{ identity, identity }),
            { { uniform_set_skybox, 0 } }));

        out.push_back(Rendering::Drawable::make(pipeline_grid, grid_mesh,
            Rendering::PushConstantData::from(ObjectData_UBO{ identity, glm::transpose(glm::inverse(identity)) }),
            { { uniform_set_0_light, 0 } }));

        world.view<TransformComponent, MeshComponent>().each(
            [&](auto, TransformComponent& t, MeshComponent& m) {
                out.push_back(Rendering::Drawable::make(
                    m.pipeline, m.mesh,
                    Rendering::PushConstantData::from(ObjectData_UBO{ t.get_model(), t.get_normal_matrix() }),
                    { { m.uniform_sets[0], 0 }, { m.uniform_sets[2], 2 } }
                ));
            });

        return out;
    }

    void teardown_application() override
    {
        auto wsi = get_wsi();
        auto device = wsi->get_rendering_device();
        material_registry.free_all(device);
        frame_ubo.free(device);
        light_ubo.free(device);
		device->free_rid(cubemap_uniform);
		device->free_rid(rock_uniform);
		device->free_rid(rock_normal_uniform);
		device->free_rid(pipeline_skybox.pipeline_rid);

        device->free_rid(fallback_texture);
        device->free_rid(diffuse_uniform);
        device->free_rid(specular_uniform);
		device->free_rid(pipeline_color.pipeline_rid);
		device->free_rid(pipeline_light.pipeline_rid);
		device->free_rid(pipeline_grid.pipeline_rid);
		device->free_rid(pipeline_shadow.pipeline_rid);
		device->free_rid(pipeline_point_shadow.pipeline_rid);
        mesh_storage->finalize();

		//device->free_rid(uniform_set_0);
        //device->free_rid(uniform_set_0_light);
		//device->free_rid(uniform_set_skybox);
		//material_ubo.free(device);

    }

private:
    entt::registry world;

    Rendering::UniformBuffer<FrameData_UBO>   frame_ubo;
    Rendering::UniformBuffer<LightBuffer> light_ubo;

	Rendering::UniformBuffer<PointShadowUBO> point_shadow_ubo;
	Rendering::Pipeline pipeline_point_shadow;
	RID uniform_set_0_point_shadow;

    RID uniform_set_0;
    RID uniform_set_0_light;
    RID uniform_set_0_shadow;
    RID fallback_texture;
    RID diffuse_uniform;
    RID specular_uniform;
    RID cubemap_uniform;
    RID rock_uniform;
    RID rock_normal_uniform;

	Rendering::Pipeline pipeline_skybox;
	RID uniform_set_skybox;
	RID sampler_cube;
	RID shadow_sampler;
	RID point_shadow_sampler;

    Rendering::Pipeline pipeline_color;
    Rendering::Pipeline pipeline_light;
    Rendering::Pipeline pipeline_grid;
    Rendering::Pipeline pipeline_shadow;

    RID sampler;

    Camera camera;
    std::shared_ptr<EE::InputSystemInterface> input_system;

    Rendering::MeshHandle light_mesh;
    Rendering::MeshHandle point_light_mesh;
    Rendering::MeshHandle object_mesh;
    Rendering::MeshHandle grid_mesh;
    Rendering::MeshHandle plane_mesh;
	Rendering::MeshHandle skybox_mesh;


    Rendering::WSI* wsi;
    Rendering::RenderingDevice* device;

    FrameGraph           fg;
    FrameGraphBlackboard bb;

    std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();
    std::unique_ptr<Rendering::MeshLoader>  mesh_loader;

    Rendering::MaterialRegistry material_registry;
};

namespace EE
{
    Application* application_create(int, char**)
    {
        EE_APPLICATION_SETUP;

        try {
            auto* app = new TutorialApplication();
            return app;
        }
        catch (const std::exception& e) {
            LOGE("application_create() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}