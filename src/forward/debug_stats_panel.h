#pragma once

#include "imgui.h"
#include "forward/ui_layer.h"
#include "util/timer.h"
#include "application/service_locator.h"

#include <algorithm>
#include <cstdio>

struct DebugStatsPanel : IUIPanel {

    static constexpr int HISTORY = 128;

    struct RingBuffer {
        float data[HISTORY] = {};
        int   head          = 0;
        float max_val       = 0.0f;

        void push(float v) {
            data[head] = v;
            head       = (head + 1) % HISTORY;
            max_val    = *std::max_element(data, data + HISTORY);
        }
    };

    RingBuffer cpu_frame;
    RingBuffer gpu_frame;
    RingBuffer cpu_render;

    void draw(UIContext& ctx) override {
        if (!ctx.show_stats) return;

        auto  timer  = Services::get().get<Util::FrameTimer>();
        float cpu_ms = static_cast<float>(timer->get_frame_time() * 1000.0);
        cpu_frame.push(cpu_ms);

        bool graphs = ctx.settings && ctx.settings->show_timing_graphs;
        bool timings = ctx.settings && ctx.settings->show_timings && ctx.wsi;

        ImGui::Text("FPS: %.1f  (avg %.1f)", timer->get_fps(), timer->get_fps_avg());
        if (ctx.settings)
            ImGui::Text("Draw calls: %d", ctx.settings->last_draw_count);

        ImGui::Separator();

        float graph_w = ImGui::GetContentRegionAvail().x;
        ImVec2 graph_size{ graph_w, 60.0f };
        char overlay[32];

        if (graphs) {
            float scale = std::max(33.3f, cpu_frame.max_val * 1.2f);
            snprintf(overlay, sizeof(overlay), "%.2f ms", cpu_ms);
            ImGui::Text("CPU frame");
            ImGui::PlotLines("##cpu_frame", cpu_frame.data, HISTORY, cpu_frame.head,
                             overlay, 0.0f, scale, graph_size);
        } else {
            ImGui::Text("CPU frame: %.3f ms", cpu_ms);
        }

        if (timings) {
            if (ctx.wsi->has_timing_data()) {
                float gpu_ms    = static_cast<float>(ctx.wsi->get_gpu_frame_time());
                float cpurnd_ms = static_cast<float>(ctx.wsi->get_cpu_frame_time());
                gpu_frame.push(gpu_ms);
                cpu_render.push(cpurnd_ms);

                if (graphs) {
                    float gpu_scale    = std::max(33.3f, gpu_frame.max_val  * 1.2f);
                    float render_scale = std::max(33.3f, cpu_render.max_val * 1.2f);

                    snprintf(overlay, sizeof(overlay), "%.2f ms", gpu_ms);
                    ImGui::Text("GPU frame");
                    ImGui::PlotLines("##gpu_frame", gpu_frame.data, HISTORY, gpu_frame.head,
                                     overlay, 0.0f, gpu_scale, graph_size);

                    snprintf(overlay, sizeof(overlay), "%.2f ms", cpurnd_ms);
                    ImGui::Text("CPU render");
                    ImGui::PlotLines("##cpu_render", cpu_render.data, HISTORY, cpu_render.head,
                                     overlay, 0.0f, render_scale, graph_size);
                } else {
                    ImGui::Text("GPU frame:    %.3f ms", gpu_ms);
                    ImGui::Text("CPU render:   %.3f ms", cpurnd_ms);
                }
            } else {
                ImGui::TextDisabled("Collecting GPU timings...");
            }
        }
    }
};
