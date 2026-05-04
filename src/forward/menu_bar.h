#pragma once

#include "imgui.h"
#include "forward/ui_layer.h"

struct MenuBarPanel : IUIPanel {
    void draw(UIContext& ctx) override {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Stats",   nullptr, &ctx.show_stats);
                if (ctx.settings)
                    ImGui::MenuItem("Timings", nullptr, &ctx.settings->show_timings);
                ImGui::EndMenu();
            }

            if (ctx.settings && ImGui::BeginMenu("Render")) {
                ImGui::MenuItem("Frustum Culling", nullptr, &ctx.settings->frustum_culling);
                ImGui::MenuItem("Wireframe",       nullptr, &ctx.settings->wireframe);
                ImGui::MenuItem("Grid",            nullptr, &ctx.settings->draw_grid);
                ImGui::MenuItem("Skybox",          nullptr, &ctx.settings->draw_skybox);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
};
