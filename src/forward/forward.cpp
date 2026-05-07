#include "application/common.h"

#include "rendering/frame_data.h"
#include "rendering/skybox.h"
#include "rendering/camera.h"
#include "rendering/renderers/forward_renderer.h"
#include "rendering/primitve_shapes.h"
#include "rendering/default_textures.h"
#include "rendering/mesh_loader.h"
#include "rendering/gltf_material_bridge.h"
#include "rendering/render_settings.h"
#include "rendering/utils.h"
#include "input/input.h"
#include "util/timer.h"
#include "util/profiler.h"
#include "forward/scene/components.h"
#include "forward/ui_layer.h"
#include "forward/menu_bar.h"
#include "forward/debug_stats_panel.h"
#include "entt/entt.hpp"

#include <functional>
#include "util/small_vector.h"

/**
 *  Set 0 - Per-frame global data   (camera, time, lights)
    Set 1 - Per-pass data           (shadow maps, render targets)
    Set 2 - Per-material data       (textures, material params)
    Set 3 - Per-object data         (model matrix, bone data).
 */

struct ForwardApplication : EE::Application
{
    bool pre_frame() override
    {
        input_system = Services::get().get<EE::InputSystemInterface>();
        RenderUtilities::capturing_timestamps = false;

        camera.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        camera.set_reset_on_resize();
        camera.set_mode(CameraMode::Fly);

        wsi    = get_wsi();
        device = wsi->get_rendering_device();

        wsi->set_vertex_data_mode(Rendering::WSI::VERTEX_DATA_MODE::INTERLEVED_DATA);
        wsi->set_index_buffer_format(RDC::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32);
        wsi->create_new_vertex_format(
            wsi->get_default_vertex_attribute(),
            Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);

        auto fs = Services::get().get<FilesystemInterface>();
        mesh_storage->initialize(device);

        grid_mesh        = Rendering::Shapes::upload_grid(*wsi, *mesh_storage, 10, 1, "object_grid");
        skybox_mesh      = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "skybox_cube");
        light_mesh       = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "light_cube");
        point_light_mesh = Rendering::Shapes::upload_cube(*wsi, *mesh_storage, "point_light_cube");

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

        renderer.initialize(wsi, device, cubemap_uniform.get());

        // --- Load Sponza ---
        auto vfmt = wsi->get_vertex_format_by_type(Rendering::VERTEX_FORMAT_VARIATIONS::DEFAULT);
        mesh_loader = std::make_unique<Rendering::MeshLoader>(*fs, device);
        mesh_loader->load_file("assets://gltf/Sponza/glTF/Sponza.gltf");

        const Rendering::GltfScene* gs = mesh_loader->get_scene();

        // Upload all images — one RID per image, indexed by GltfScene::images
        Util::SmallVector<RID> image_rids;
        for (int i = 0; i < (int)gs->images.size(); i++)
            image_rids.push_back(mesh_loader->upload_cached(
                gs, i, "assets://models/sponza/Sponza.gltf"));

        // Convert PBR materials -> MaterialRegistry entries
        for (const auto& pbr : gs->materials) {
            Rendering::Material mat = Rendering::material_from_pbr(pbr, image_rids);
            sponza_mats.push_back(material_registry.create(
                device, std::move(mat), fallback_texture, renderer.color_pipeline().shader_rid));
        }

        // Upload each primitive as its own MeshHandle — [mesh_idx][prim_idx]
        // Shared by multiple nodes that reference the same mesh (no duplicate uploads)
        Util::SmallVector<Util::SmallVector<Rendering::MeshHandle>> prim_handles(gs->meshes.size());
        for (int mi = 0; mi < (int)gs->meshes.size(); mi++) {
            const auto& mesh = gs->meshes[mi];
            prim_handles[mi].resize(mesh.primitives.size(), Rendering::INVALID_MESH);
            for (int pi = 0; pi < (int)mesh.primitives.size(); pi++) {
                std::string name = "sponza/m" + std::to_string(mi) + "/p" + std::to_string(pi);
                prim_handles[mi][pi] = mesh_loader->upload_primitive(
                    *mesh_storage, name, mesh.primitives[pi], vfmt);
            }
        }

        // Walk GLTF node hierarchy -> one entity per primitive for tight per-primitive AABBs
        std::function<void(int, glm::mat4)> visit = [&](int ni, glm::mat4 parent_world) {
            const auto& node     = gs->nodes[ni];
            glm::mat4 node_world = parent_world * node.get_local_transform();

            if (node.mesh_index >= 0 && node.mesh_index < (int)prim_handles.size()) {
                const auto& mesh = gs->meshes[node.mesh_index];
                for (int pi = 0; pi < (int)mesh.primitives.size(); pi++) {
                    const auto& prim = mesh.primitives[pi];
                    auto e = world.create();
                    world.emplace<TransformComponent>(e, TransformComponent{
                        .matrix_override = node_world });
                    world.emplace<MeshComponent>(e, MeshComponent{
                        .mesh       = prim_handles[node.mesh_index][pi],
                        .materials  = { prim.material_index >= 0
                                        ? sponza_mats[prim.material_index]
                                        : Rendering::INVALID_MATERIAL },
                        .local_aabb = Rendering::compute_aabb_single(prim),
                    });
                }
            }

            for (int child : node.children)
                visit(child, node_world);
        };

        if (!gs->scenes.empty())
            for (int root : gs->scenes[gs->default_scene].root_nodes)
                visit(root, glm::mat4(1.0f));

        // --- Lights ---
        auto dir_light = world.create();
        world.emplace<TransformComponent>(dir_light, TransformComponent{
            .position = glm::vec3(5.0f, 10.0f, 5.0f),
            .scale    = glm::vec3(0.2f) });
        world.emplace<MeshComponent>(dir_light, MeshComponent{
            .mesh     = light_mesh,
            .category = Rendering::MeshCategory::LightVisualization,
        });
        world.emplace<LightComponent>(dir_light, LightComponent{ .data = {
            .position    = glm::vec4(5.0f, 10.0f, 5.0f, 15.0f),
            .direction   = glm::vec4(0.0f, -1.0f, -0.5f, 0.0f),
            .color       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .type        = static_cast<uint32_t>(LightType::Directional),
            .outer_angle = 0.0f,
        } });

        auto pt_light = world.create();
        world.emplace<TransformComponent>(pt_light, TransformComponent{
            .position = glm::vec3(1.0f, 1.0f, 1.0f),
            .scale    = glm::vec3(0.1f) });
        world.emplace<MeshComponent>(pt_light, MeshComponent{
            .mesh     = point_light_mesh,
            .category = Rendering::MeshCategory::LightVisualization,
        });
        world.emplace<LightComponent>(pt_light, LightComponent{ .data = {
            .position    = glm::vec4(1.0f, 1.0f, 1.0f, 15.0f),
            .direction   = glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f),
            .color       = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .type        = static_cast<uint32_t>(LightType::Point),
            .outer_angle = 0.0f,
        } });

        ui_ctx.camera    = &camera;
        ui_ctx.world     = &world;
        ui_ctx.wsi       = wsi;
        ui_ctx.settings  = &render_settings;

        ui_layer.add(std::make_unique<MenuBarPanel>());
        ui_layer.add(std::make_unique<DebugStatsPanel>());

        wsi->submit_transfer_workers();
        return wsi->pre_frame_loop();
    }

    void render_frame(double frame_time, double elapsed_time) override
    {
        ZoneScoped;
        camera.update_from_input(input_system.get(), frame_time);
        RenderUtilities::capturing_timestamps = render_settings.show_timings;

        device->imgui_begin_frame();
        ui_layer.draw_frame(ui_ctx);

        Rendering::SceneView view = build_scene_view(elapsed_time);

        fg.reset();
        bb.reset();

        renderer.setup_passes(fg, bb, view, *mesh_storage);
        Rendering::add_imgui_pass(fg, bb, view.extent);
        Rendering::add_blit_pass(fg, bb, bb.get<forward_pass_resource>());

        fg.compile();

        Rendering::RenderContext rc;
        rc.command_buffer = device->get_current_command_buffer();
        rc.device         = device;
        rc.wsi            = wsi;
        TracyPlot("Draw Calls", (int64_t)render_settings.last_draw_count);
        TIMESTAMP_BEGIN();
        fg.execute(&rc, &rc);
        RENDER_TIMESTAMP("Frame End");
    }

    void teardown_application() override
    {
        material_registry.free_all(device);
        if (mesh_loader)
            mesh_loader->free_owned_resources();
        mesh_storage->finalize();
    }

private:
    Rendering::SceneView build_scene_view(double elapsed)
    {
        ZoneScoped;
        material_registry.upload_dirty(device);

        Rendering::SceneView view;
        view.camera      = &camera;
        view.settings    = &render_settings;
        view.elapsed     = elapsed;
        view.extent      = { device->screen_get_width(), device->screen_get_height() };
        view.skybox_mesh = render_settings.draw_skybox ? skybox_mesh : Rendering::INVALID_MESH;
        view.grid_mesh   = render_settings.draw_grid   ? grid_mesh   : Rendering::INVALID_MESH;

        render_settings.last_draw_count = 0;
        world.view<TransformComponent, MeshComponent>().each(
            [&](auto, TransformComponent& t, MeshComponent& m) {
                glm::mat4 model = t.get_model();
                if (m.local_aabb.valid()) {
                    Rendering::AABB world_aabb = Rendering::transform_aabb(m.local_aabb, model);
                    if (render_settings.draw_debug_aabbs)
                        Rendering::DebugDraw::get().add_aabb(world_aabb.min, world_aabb.max);
                    if (render_settings.frustum_culling && !camera.is_aabb_visible(world_aabb.min, world_aabb.max))
                        return;
                }
                Rendering::MeshInstance inst;
                inst.mesh          = m.mesh;
                inst.model         = model;
                inst.normal_matrix = t.get_normal_matrix();
                inst.category      = m.category;
                for (auto h : m.materials)
                    inst.material_sets.push_back(
                        h != Rendering::INVALID_MATERIAL
                            ? material_registry.get_uniform_set(h)
                            : RID());
                view.instances.push_back(std::move(inst));
                render_settings.last_draw_count++;
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

    // Material registry — uniform sets reference texture RIDs, destroyed before mesh_loader.
    Rendering::MaterialRegistry material_registry;

    // Scene
    entt::registry world;
    std::unique_ptr<Rendering::MeshStorage> mesh_storage = std::make_unique<Rendering::MeshStorage>();

    Rendering::MeshHandle light_mesh;
    Rendering::MeshHandle point_light_mesh;
    Rendering::MeshHandle grid_mesh;
    Rendering::MeshHandle skybox_mesh;

    // Sponza — mesh_loader owns image RIDs, must outlive material_registry
    std::unique_ptr<Rendering::MeshLoader>  mesh_loader;
    Util::SmallVector<Rendering::MaterialHandle>  sponza_mats;

    Camera camera;
    std::shared_ptr<EE::InputSystemInterface> input_system;

    Rendering::WSI*             wsi    = nullptr;
    Rendering::RenderingDevice* device = nullptr;

    RenderSettings render_settings;
    UIContext      ui_ctx;
    UILayer        ui_layer;

    FrameGraph           fg;
    FrameGraphBlackboard bb;

    // Renderer — declared last, destroyed first (uniform sets reference texture RIDs above).
    Rendering::ForwardRenderer renderer;
};

namespace EE
{
    Application* application_create(int, char**)
    {
        EE_APPLICATION_SETUP;

        try {
            return new ForwardApplication();
        }
        catch (const std::exception& e) {
            LOGE("application_create() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}
