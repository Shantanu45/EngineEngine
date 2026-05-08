#pragma once

#include "application/service_locator.h"
#include "forward/ui_layer.h"
#include "imgui.h"
#include "util/timer.h"

#include <algorithm>
#include <cstdio>

struct FrameStatsPanel : IUIPanel {
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

	RingBuffer cpu_frame;

	void draw(UIContext& ctx) override
	{
		if (!ctx.show_stats)
			return;

		auto timer = Services::get().get<Util::FrameTimer>();
		float cpu_ms = static_cast<float>(timer->get_frame_time() * 1000.0);
		cpu_frame.push(cpu_ms);

		ImGui::Text("FPS: %.1f  (avg %.1f)", timer->get_fps(), timer->get_fps_avg());
		if (ctx.stats)
			ImGui::Text("Draw calls: %d", ctx.stats->draw_count);

		ImGui::Separator();

		if (ctx.settings && ctx.settings->show_timing_graphs) {
			float scale = std::max(33.3f, cpu_frame.max_val * 1.2f);
			char overlay[32];
			snprintf(overlay, sizeof(overlay), "%.2f ms", cpu_ms);
			ImGui::Text("CPU frame");
			ImGui::PlotLines(
				"##cpu_frame",
				cpu_frame.data,
				HISTORY,
				cpu_frame.head,
				overlay,
				0.0f,
				scale,
				ImVec2{ ImGui::GetContentRegionAvail().x, 60.0f });
		}
		else {
			ImGui::Text("CPU frame: %.3f ms", cpu_ms);
		}
	}
};
