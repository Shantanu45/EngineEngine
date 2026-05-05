#pragma once
#include <cstdint>
#include "rendering_device.h"
#include "util/profiler.h"

#define GPU_SCOPE(cmd, name, color) \
	auto debug_marker = Rendering::RenderingDevice::ScopedDebugMarker(Rendering::RenderingDevice::get_singleton(), cmd, name, color); \
	ZoneScopedN(name);

using RD = Rendering::RenderingDevice;

namespace RenderUtilities
{
	struct FrameProfileArea {
		std::string name;
		double gpu_msec;
		double cpu_msec;
	};

	void capture_timestamps_begin();
	void capture_timestamp(const std::string& p_name);
	uint32_t get_captured_timestamps_count();
	uint64_t get_captured_timestamps_frame();
	uint64_t get_captured_timestamp_gpu_time(uint32_t p_index);
	uint64_t get_captured_timestamp_cpu_time(uint32_t p_index);
	std::string get_captured_timestamp_name(uint32_t p_index);

	inline bool capturing_timestamps = false;


}

#define TIMESTAMP_BEGIN() \
	{ \
		if (RenderUtilities::capturing_timestamps) \
			RenderUtilities::capture_timestamps_begin(); \
	}

#define RENDER_TIMESTAMP(m_text) \
	{ \
		if (RenderUtilities::capturing_timestamps) \
			RenderUtilities::capture_timestamp(m_text); \
	}