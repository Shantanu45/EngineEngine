#pragma once

#include "imgui.h"
#include "deferred/ui_layer.h"

struct MenuBarPanel : IUIPanel {
    void draw(UIContext& ctx) override {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Stats",          nullptr, &ctx.show_stats);
                if (ctx.settings) {
                    ImGui::MenuItem("Timings",        nullptr, &ctx.settings->show_timings);
                    ImGui::MenuItem("Timing Graphs",  nullptr, &ctx.settings->show_timing_graphs);
                }
                ImGui::EndMenu();
            }

            if (ctx.settings && ImGui::BeginMenu("Render")) {
                ImGui::MenuItem("Frustum Culling", nullptr, &ctx.settings->frustum_culling);
                ImGui::MenuItem("Wireframe",       nullptr, &ctx.settings->wireframe);
                ImGui::MenuItem("Grid",            nullptr, &ctx.settings->draw_grid);
                ImGui::MenuItem("Skybox",          nullptr, &ctx.settings->draw_skybox);
                ImGui::MenuItem("PBR Lighting",    nullptr, &ctx.settings->use_pbr_lighting);
                ImGui::MenuItem("Debug AABBs",     nullptr, &ctx.settings->draw_debug_aabbs);
                ImGui::Separator();
                ImGui::MenuItem("Render From Debug Camera", nullptr, &ctx.settings->render_from_debug_camera);
                ImGui::MenuItem("Use Debug Culling Camera", nullptr, &ctx.settings->use_debug_culling_camera);
                ImGui::MenuItem("Debug Camera Detached", nullptr, &ctx.settings->debug_camera_detached);
                ImGui::MenuItem("Draw Render Frustum", nullptr, &ctx.settings->draw_render_frustum);
                ImGui::MenuItem("Draw Culling Frustum", nullptr, &ctx.settings->draw_culling_frustum);
                ImGui::MenuItem("Draw Culling Results", nullptr, &ctx.settings->draw_culling_results);
                if (ctx.camera && ctx.debug_camera && ImGui::MenuItem("Copy Main Camera To Debug")) {
                    ctx.debug_camera->set_position(ctx.camera->get_position());
                    ctx.debug_camera->set_rotation(ctx.camera->get_rotation());
                    ctx.settings->debug_camera_detached = true;
                }
                ImGui::Separator();
                ImGui::SliderFloat("Exposure", &ctx.settings->exposure, 0.0f, 8.0f, "%.2f");
                ImGui::DragFloat("Directional Bias Scale", &ctx.settings->directional_shadow_bias_scale, 0.0001f, 0.0f, 0.02f, "%.5f");
                ImGui::DragFloat("Directional Bias Min", &ctx.settings->directional_shadow_bias_min, 0.00001f, 0.0f, 0.005f, "%.5f");
                ImGui::DragFloat("Point Bias Max", &ctx.settings->point_shadow_bias_max, 0.0001f, 0.0f, 0.05f, "%.5f");
                ImGui::DragFloat("Point Bias Min", &ctx.settings->point_shadow_bias_min, 0.00001f, 0.0f, 0.005f, "%.5f");
                int shadow_mode = static_cast<int>(ctx.settings->directional_shadow_mode);
                if (ImGui::Combo("Directional Shadow", &shadow_mode, "Single Map\0Cascaded\0"))
                    ctx.settings->directional_shadow_mode = static_cast<DirectionalShadowMode>(shadow_mode);
                int tone_mapper = static_cast<int>(ctx.settings->tone_mapper);
                if (ImGui::Combo("Tone Mapper", &tone_mapper, "None\0Reinhard\0ACES\0"))
                    ctx.settings->tone_mapper = static_cast<ToneMapper>(tone_mapper);
                int debug_view = static_cast<int>(ctx.settings->material_debug_view);
                if (ImGui::Combo("Material View", &debug_view,
                    "Lit\0Albedo\0Normal\0Roughness\0Metallic\0Ambient Occlusion\0Emissive\0Shadow Factor\0Light Count\0Depth\0Directional Shadow Map\0Light Space Coords\0"))
                    ctx.settings->material_debug_view = static_cast<MaterialDebugView>(debug_view);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
};
