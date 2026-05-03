#include "application/common.h"

#include "rendering/frame_data.h"
#include "rendering/skybox.h"
#include "rendering/texture_cache.h"
#include "rendering/camera.h"
#include "rendering/renderers/forward_renderer.h"
#include "rendering/primitve_shapes.h"
#include "rendering/drawable.h"
#include "rendering/default_textures.h"
#include "input/input.h"
#include "util/timer.h"
#include "tutorial/scene/components.h"
#include "entt/entt.hpp"

/**
 *  Set 0 - Per-frame global data   (camera, time, lights)
    Set 1 - Per-pass data           (shadow maps, render targets)
    Set 2 - Per-material data       (textures, material params)
    Set 3 - Per-object data         (model matrix, bone data).
 */

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

        auto fs = Services::get().get<FilesystemInterface>();
        mesh_storage->initialize(device);
        tex_cache.init(device, *fs);

        // --- Meshes ---
        light_mesh       = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "light_cube");
        point_light_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "point_light_cube");
        object_mesh      = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "object_cube");
        grid_mesh        = Rendering::Shapes::upload_grid(*wsi, *mesh_storage, 10, 1, "object_grid");
        plane_mesh       = Rendering::Shapes::upload_plane(*wsi, *mesh_storage, 1, "object_plane");
        skybox_mesh      = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "skybox_cube");

        // --- Textures ---
        fallback_texture = Rendering::create_white_texture(device);

        std::array<std::string, 6> faces = {
            "assets://textures/skybox/right.jpg",
            "assets://textures/skybox/left.jpg",
            "assets://textures/skybox/top.jpg",
            "assets://textures/skybox/bottom.jpg",
            "assets://textures/skybox/front.jpg",
            "assets://textures/skybox/back.jpg",
        };
        cubemap_uniform = Rendering::load_skybox_cubemap(device, *fs, faces);

        // --- Renderer ---
        renderer.initialize(wsi, device, cubemap_uniform.get());

        // --- Materials ---
        Rendering::Material mat;
        mat.diffuse            = tex_cache.color("assets://textures/container2.png");
        mat.metallic_roughness = tex_cache.linear("assets://textures/container2_specular.png");
        mat.base_color_factor  = glm::vec4(1.0f);
        mat.shininess          = 64.0f;
        Rendering::MaterialHandle h = material_registry.create(
            device, std::move(mat), fallback_texture, renderer.color_pipeline().shader_rid);

        Rendering::Material mat_rock;
        mat_rock.diffuse           = tex_cache.color("assets://textures/bricks2.jpg");
        mat_rock.normal            = tex_cache.linear("assets://textures/bricks2_normal.jpg");
        mat_rock.displacement      = tex_cache.linear("assets://textures/bricks2_disp.jpg");
        mat_rock.base_color_factor = glm::vec4(1.0f);
        mat_rock.shininess         = 32.0f;
        Rendering::MaterialHandle h_rock = material_registry.create(
            device, std::move(mat_rock), fallback_texture, renderer.color_pipeline().shader_rid);

        // --- Scene entities ---
        for (int x = 0; x < 2; x++) {
            for (int z = 0; z < 2; z++) {
                auto entity = world.create();
                world.emplace<TransformComponent>(entity, TransformComponent{
                    .position = glm::vec3(x * 2.5f, 0.5f, z * 2.5f) });
                world.emplace<MeshComponent>(entity, MeshComponent{
                    .mesh      = object_mesh,
                    .materials = { h },
                });
            }
        }

        auto entity_plane = world.create();
        world.emplace<TransformComponent>(entity_plane, TransformComponent{
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .scale    = glm::vec3(10.0f) });
        world.emplace<MeshComponent>(entity_plane, MeshComponent{
            .mesh      = plane_mesh,
            .materials = { h_rock },
        });

        // Directional light
        auto light = world.create();
        world.emplace<TransformComponent>(light, TransformComponent{
            .position = glm::vec3(5.0f, 10.0f, 5.0f),
            .scale    = glm::vec3(0.2f) });
        world.emplace<MeshComponent>(light, MeshComponent{
            .mesh     = light_mesh,
            .category = Rendering::MeshCategory::LightVisualization,
        });
        world.emplace<LightComponent>(light, LightComponent{ .data = {
            .position    = glm::vec4(5.0f, 10.0f, 5.0f, 15.0f),
            .direction   = glm::vec4(-0.0f, -1.0f, -0.5f, 0.0f),
            .color       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .type        = static_cast<uint32_t>(LightType::Directional),
            .outer_angle = 0.0f,
        } });

        // Point light
        auto point_light = world.create();
        world.emplace<TransformComponent>(point_light, TransformComponent{
            .position = glm::vec3(1.0f, 1.0f, 1.0f),
            .scale    = glm::vec3(0.1f) });
        world.emplace<MeshComponent>(point_light, MeshComponent{
            .mesh     = point_light_mesh,
            .category = Rendering::MeshCategory::LightVisualization,
        });
        world.emplace<LightComponent>(point_light, LightComponent{ .data = {
            .position    = glm::vec4(1.0f, 1.0f, 1.0f, 15.0f),
            .direction   = glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f),
            .color       = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .type        = static_cast<uint32_t>(LightType::Point),
            .outer_angle = 0.0f,
        } });

        wsi->submit_transfer_workers();
        return wsi->pre_frame_loop();
    }

    void render_frame(double frame_time, double elapsed_time) override
    {
        camera.update_from_input(input_system.get(), frame_time);

        device->imgui_begin_frame();
        const auto timer = Services::get().get<Util::FrameTimer>();
        ImGui::Text("FPS: %.1f", timer->get_fps());
        ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);

        Rendering::SceneView view = build_scene_view(elapsed_time);

        fg.reset();
        bb.reset();

        renderer.setup_passes(fg, bb, view, *mesh_storage);
        Rendering::add_imgui_pass(fg, bb, view.extent);
        Rendering::add_blit_pass(fg, bb, bb.get<forward_pass_resource>());

        fg.compile();

        Rendering::RenderContext rc;
        rc.command_buffer = device->get_current_command_buffer();
        rc.device = device;
        rc.wsi = wsi;
        fg.execute(&rc, &rc);
    }

    void teardown_application() override
    {
        material_registry.free_all(device);
        tex_cache.free_all();
        mesh_storage->finalize();
    }

private:
    Rendering::SceneView build_scene_view(double elapsed)
    {
        material_registry.upload_dirty(device);

        Rendering::SceneView view;
        view.camera      = &camera;
        view.elapsed     = elapsed;
        view.extent      = { device->screen_get_width(), device->screen_get_height() };
        view.skybox_mesh = skybox_mesh;
        view.grid_mesh   = grid_mesh;

        world.view<TransformComponent, MeshComponent>().each(
            [&](auto, TransformComponent& t, MeshComponent& m) {
                Rendering::MeshInstance inst;
                inst.mesh          = m.mesh;
                inst.model         = t.get_model();
                inst.normal_matrix = t.get_normal_matrix();
                inst.category      = m.category;
                for (auto h : m.materials)
                    inst.material_sets.push_back(
                        h != Rendering::INVALID_MATERIAL
                            ? material_registry.get_uniform_set(h)
                            : RID());
                view.instances.push_back(std::move(inst));
            });

        world.view<TransformComponent, LightComponent>().each(
            [&](auto, TransformComponent& t, LightComponent& l) {
                Light gl = l.data;
                gl.position = glm::vec4(t.position, l.data.position.w);
                view.lights.push_back(gl);
            });

        return view;
    }

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    // Textures — declared first so they outlive the renderer's uniform sets that reference them.
    RIDHandle fallback_texture;
    RIDHandle cubemap_uniform;
    Rendering::TextureCache tex_cache;

    // Material registry — uniform sets reference tex_cache textures, so must be
    // destroyed before tex_cache (declared after it, destroyed before it).
    Rendering::MaterialRegistry material_registry;

    // Scene data
    entt::registry world;
    std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();
    Rendering::MeshHandle light_mesh;
    Rendering::MeshHandle point_light_mesh;
    Rendering::MeshHandle object_mesh;
    Rendering::MeshHandle grid_mesh;
    Rendering::MeshHandle plane_mesh;
    Rendering::MeshHandle skybox_mesh;

    Camera camera;
    std::shared_ptr<EE::InputSystemInterface> input_system;

    Rendering::WSI*             wsi    = nullptr;
    Rendering::RenderingDevice* device = nullptr;

    FrameGraph           fg;
    FrameGraphBlackboard bb;

    // Renderer — declared last so it is destroyed first, before cubemap_uniform
    // and fallback_texture which its uniform sets reference.
    Rendering::ForwardRenderer renderer;
};

namespace EE
{
    Application* application_create(int, char**)
    {
        EE_APPLICATION_SETUP;

        try {
            return new TutorialApplication();
        }
        catch (const std::exception& e) {
            LOGE("application_create() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}
