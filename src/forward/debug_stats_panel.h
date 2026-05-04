#pragma once

#include "imgui.h"
#include "tutorial/ui_layer.h"
#include "util/timer.h"
#include "application/service_locator.h"

struct DebugStatsPanel : IUIPanel {
    void draw(UIContext& ctx) override {
        if (!ctx.show_stats) return;
        auto timer = Services::get().get<Util::FrameTimer>();
        ImGui::Text("FPS: %.1f", timer->get_fps());
        ImGui::Text("Frame Time: %.3f ms", timer->get_frame_time() * 1000.0);
    }
};
