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
#include "entt/entt.hpp"
#include "scene/components.h"

/**
 *  Set 0 — Per-frame global data   (camera, time, lights)
    Set 1 — Per-pass data           (shadow maps, render targets)
    Set 2 — Per-material data       (textures, material params)
    Set 3 — Per-object data         (model matrix, bone data).
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
};

struct alignas(16) ObjectData_UBO {
    glm::mat4 model;
    glm::mat4 normalMatrix;
};

struct alignas(16) Material_UBO {
    float shininess;
};

void add_basic_pass(
    FrameGraph& fg,
    FrameGraphBlackboard& bb,
    Size2i extent,
    std::vector<Rendering::Drawable> drawables,
    Rendering::MeshStorage& storage)
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
                tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT
                    | RD::TEXTURE_USAGE_SAMPLING_BIT;
                tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;

                data.scene = builder.create<Rendering::FrameGraphTexture>(
                    "scene texture", { tf, RD::TextureView(), "scene texture" });
                data.scene = builder.write(data.scene, TEXTURE_WRITE_FLAGS::WRITE_COLOR);
            },
            [=, &storage](const basic_pass_resource& data,
                FrameGraphPassResources& resources,
                void* ctx)
            {
                auto& rc = *static_cast<Rendering::RenderContext*>(ctx);
                auto  cmd = rc.command_buffer;

                auto& scene_tex = resources.get<Rendering::FrameGraphTexture>(data.scene);

                uint32_t w = rc.device->screen_get_width();
                uint32_t h = rc.device->screen_get_height();

                RID frame_buffer = rc.device->framebuffer_create({ scene_tex.texture_rid });

                GPU_SCOPE(cmd, "Basic Pass", Color(1.0, 0.0, 0.0, 1.0));

                std::array<RDD::RenderPassClearValue, 2> clear_values;
                clear_values[0].color = Color();
                clear_values[1].depth = 1.0;
                clear_values[1].stencil = 0.0;

                rc.device->begin_render_pass_from_frame_buffer(frame_buffer, Rect2i(0, 0, w, h), clear_values);

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
        RenderUtilities::capturing_timestamps = true;

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
        mesh_loader = std::make_unique<Rendering::MeshLoader>(*fs);
        mesh_storage->initialize(device);

        // --- Meshes ---
        light_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "light_cube");
        object_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "object_cube");
        grid_mesh = Rendering::Shapes::upload_grid(*wsi, *mesh_storage, 10, 1, "object_grid");

        // --- Framebuffer format ---
        RD::AttachmentFormat color;
        color.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
        color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

        auto framebuffer_format = RD::get_singleton()->framebuffer_format_create({ color });

        // --- Pipelines ---
        pipeline_color = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/light_map.vert",
                          "assets://shaders/light_map.frag" }, "light_map")
            .set_vertex_format(vertex_format)
            .build(framebuffer_format);

        pipeline_light = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/light_cube.vert",
                          "assets://shaders/light_cube.frag" }, "cube_shader")
            .set_vertex_format(vertex_format)
            .build(framebuffer_format);

        pipeline_grid = Rendering::PipelineBuilder{}
            .set_shader({ "assets://shaders/grid.vert",
                          "assets://shaders/grid.frag" }, "grid_shader")
            .set_vertex_format(vertex_format)
            .set_render_primitive(RDC::RENDER_PRIMITIVE_LINES)
            .build(framebuffer_format);

        // --- UBOs ---
        frame_ubo.create(device, "Frame UBO");
        material_ubo.create(device, "Material UBO");
        pointlight_ubo.create(device, "Light UBO");

        // --- Textures ---
        Rendering::ImageLoader img_loader(*fs);

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
        specular_uniform = device->texture_create(tf, RD::TextureView(), { specular_image.pixels });
        device->set_resource_name(diffuse_uniform, "Diffuse texture");
        device->set_resource_name(specular_uniform, "Specular texture");

        sampler = device->sampler_create(RD::SamplerState());

        // --- Uniform sets ---
        uniform_set_0 = Rendering::UniformSetBuilder{}
            .add(frame_ubo.as_uniform(0))
            .add(material_ubo.as_uniform(1))
            .add(pointlight_ubo.as_uniform(2))
            .add_texture(3, sampler, diffuse_uniform)
            .add_texture(4, sampler, specular_uniform)
            .build(device, device->get_shader_rid("light_map"), 0);

        uniform_set_0_light = Rendering::UniformSetBuilder{}
            .add(frame_ubo.as_uniform(0))
            .build(device, device->get_shader_rid("cube_shader"), 0);

        // --- Scene setup ---
        // object cube
		for (int x = 0; x < 2; x++) {
			for (int z = 0; z < 5; z++) {
				auto entity = world.create();
				world.emplace<TransformComponent>(entity, TransformComponent{
					.position = glm::vec3(x * 2.5f, 0.0f, z * 2.5f) });
				world.emplace<MeshComponent>(entity, MeshComponent{
					object_mesh, pipeline_color, "light_map", uniform_set_0 });
			}
		}

        // light cube
        auto light = world.create();
        world.emplace<TransformComponent>(light, TransformComponent{
            .position = glm::vec3(1.0f, 1.0f, 1.0f),
            .scale = glm::vec3(0.2f) });
        world.emplace<MeshComponent>(light, MeshComponent{
            light_mesh, pipeline_light, "cube_shader", uniform_set_0_light });
        world.emplace<LightComponent>(light, LightComponent{ .data = {
            .constant = 1.0f,
            .ambient = glm::vec3(0.2f),
            .linear = 0.09f,
            .diffuse = glm::vec3(0.5f),
            .quadratic = 0.032f,
            .specular = glm::vec3(1.0f),
        } });

        wsi->submit_transfer_workers();
        return wsi->pre_frame_loop();
    }

    void render_frame(double frame_time, double elapsed_time) override
    {
        camera.update_from_input(input_system.get(), frame_time);

        // --- Frame UBO ---
        FrameData_UBO frame_data{};
        frame_data.camera.view = camera.get_view();
        frame_data.camera.proj = camera.get_projection();
        frame_data.camera.cameraPos = camera.get_position();
        frame_data.time = static_cast<float>(elapsed_time);
        frame_ubo.upload(device, frame_data);

        // --- Upload lights ---
        world.view<TransformComponent, LightComponent>().each(
            [&](auto entity, TransformComponent& t, LightComponent& l) {
                l.data.position = t.position;
                pointlight_ubo.upload(device, l.data);
            });

        // --- Upload material ---
        // TODO: move into MaterialComponent and upload per object
        material_ubo.upload(device, Material_UBO{ 32.0f });

        // --- Build drawables ---
        std::vector<Rendering::Drawable> drawables;

        // grid — no entity, always identity
        glm::mat4 identity = glm::mat4(1.0f);
        drawables.push_back(
            Rendering::Drawable::make(pipeline_grid, grid_mesh, "grid_shader",
                Rendering::PushConstantData::from(
                    ObjectData_UBO{ identity, glm::transpose(glm::inverse(identity)) }),
                { { uniform_set_0_light, 0 } }));

        // all mesh entities
        world.view<TransformComponent, MeshComponent>().each(
            [&](auto entity, TransformComponent& t, MeshComponent& m) {
                auto model = t.get_model();
                auto normal = t.get_normal_matrix();
                drawables.push_back(
                    Rendering::Drawable::make(m.pipeline, m.mesh, m.shader,
                        Rendering::PushConstantData::from(ObjectData_UBO{ model, normal }),
                        { { m.uniform_set, 0 } }));
            });

        device->imgui_begin_frame();
        const auto timer = Services::get().get<Util::FrameTimer>();
        ImGui::Text("FPS: %.1f", timer->get_fps());
        ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);

        fg.reset();
        bb.reset();

        add_basic_pass(fg, bb,
            { device->screen_get_width(), device->screen_get_height() }, drawables, *mesh_storage);
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

    void teardown_application() override
    {
        auto wsi = get_wsi();
        auto device = wsi->get_rendering_device();

        frame_ubo.free(device);
        material_ubo.free(device);
        pointlight_ubo.free(device);

        device->free_rid(uniform_set_0);
        device->free_rid(uniform_set_0_light);
        device->free_rid(diffuse_uniform);
        device->free_rid(specular_uniform);
        device->free_rid(pipeline_color);
        device->free_rid(pipeline_light);
        device->free_rid(pipeline_grid);
        device->free_rid(sampler);
        mesh_storage->finalize();
    }

private:
    entt::registry world;

    Rendering::UniformBuffer<FrameData_UBO>   frame_ubo;
    Rendering::UniformBuffer<Material_UBO>    material_ubo;
    Rendering::UniformBuffer<PointLight_UBO>  pointlight_ubo;

    RID uniform_set_0;
    RID uniform_set_0_light;
    RID diffuse_uniform;
    RID specular_uniform;

    RID pipeline_color;
    RID pipeline_light;
    RID pipeline_grid;

    RID sampler;

    Camera camera;
    std::shared_ptr<EE::InputSystemInterface> input_system;

    Rendering::MeshHandle light_mesh;
    Rendering::MeshHandle object_mesh;
    Rendering::MeshHandle grid_mesh;

    Rendering::WSI* wsi;
    Rendering::RenderingDevice* device;

    FrameGraph           fg;
    FrameGraphBlackboard bb;

    std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();
    std::unique_ptr<Rendering::MeshLoader>  mesh_loader;
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