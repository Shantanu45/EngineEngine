#pragma once

#include "imgui.h"
#include "forward/ui_layer.h"
#include "util/timer.h"
#include "application/service_locator.h"

struct DebugStatsPanel : IUIPanel {
    void draw(UIContext& ctx) override {
        if (!ctx.show_stats) return;

        auto timer = Services::get().get<Util::FrameTimer>();
        ImGui::Text("FPS: %.1f  (avg %.1f)", timer->get_fps(), timer->get_fps_avg());
        ImGui::Text("CPU frame: %.3f ms", timer->get_frame_time() * 1000.0);
        if (ctx.settings)
            ImGui::Text("Draw calls: %d", ctx.settings->last_draw_count);

        if (ctx.settings && ctx.settings->show_timings && ctx.wsi) {
            ImGui::Separator();
            if (ctx.wsi->has_timing_data()) {
                ImGui::Text("GPU frame: %.3f ms", ctx.wsi->get_gpu_frame_time());
                ImGui::Text("CPU (render): %.3f ms", ctx.wsi->get_cpu_frame_time());
            } else {
                ImGui::TextDisabled("Collecting...");
            }
        }
    }
};
