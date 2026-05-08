#pragma once

#include "forward/ui_layer.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>

struct RendererTimingsPanel : IUIPanel {
	static constexpr int HISTORY = 128;

	struct RingBuffer {
		float data[HISTORY] = {};
		int head = 0;
		float max_val = 0.0f;

		void push(float value)
		{
			data[head] = value;
			head = (head + 1) % HISTORY;
			max_val = *std::max_element(data, data + HISTORY);
		}
	};

	RingBuffer gpu_frame;
	RingBuffer cpu_render;

	void draw(UIContext& ctx) override
	{
		bool timings = ctx.settings && ctx.settings->show_timings && ctx.wsi;
		if (!ctx.show_stats || !timings)
			return;

		if (!ctx.wsi->has_timing_data()) {
			ImGui::TextDisabled("Collecting GPU timings...");
			return;
		}

		float gpu_ms = static_cast<float>(ctx.wsi->get_gpu_frame_time());
		float cpu_render_ms = static_cast<float>(ctx.wsi->get_cpu_frame_time());
		gpu_frame.push(gpu_ms);
		cpu_render.push(cpu_render_ms);

		if (ctx.settings->show_timing_graphs) {
			float graph_w = ImGui::GetContentRegionAvail().x;
			ImVec2 graph_size{ graph_w, 60.0f };
			char overlay[32];

			float gpu_scale = std::max(33.3f, gpu_frame.max_val * 1.2f);
			snprintf(overlay, sizeof(overlay), "%.2f ms", gpu_ms);
			ImGui::Text("GPU frame");
			ImGui::PlotLines("##gpu_frame", gpu_frame.data, HISTORY, gpu_frame.head, overlay, 0.0f, gpu_scale, graph_size);

			float render_scale = std::max(33.3f, cpu_render.max_val * 1.2f);
			snprintf(overlay, sizeof(overlay), "%.2f ms", cpu_render_ms);
			ImGui::Text("CPU render");
			ImGui::PlotLines("##cpu_render", cpu_render.data, HISTORY, cpu_render.head, overlay, 0.0f, render_scale, graph_size);
		}
		else {
			ImGui::Text("GPU frame:    %.3f ms", gpu_ms);
			ImGui::Text("CPU render:   %.3f ms", cpu_render_ms);
		}
	}
};
