#pragma once

#include "imgui.h"
#include "tutorial/ui_layer.h"

struct MenuBarPanel : IUIPanel {
    void draw(UIContext& ctx) override {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Stats", nullptr, &ctx.show_stats);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
};
