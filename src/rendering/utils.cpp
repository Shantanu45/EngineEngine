#include "utils.h"

namespace RenderUtilities
{

	void capture_timestamps_begin()
	{
		RD::get_singleton()->capture_timestamp("Frame Begin");
	}

	void capture_timestamp(const std::string& p_name)
	{
		RD::get_singleton()->capture_timestamp(p_name);
	}

	uint32_t get_captured_timestamps_count(){
		return RD::get_singleton()->get_captured_timestamps_count();
	}

	uint64_t get_captured_timestamps_frame(){
		return RD::get_singleton()->get_captured_timestamps_frame();
	}

	uint64_t get_captured_timestamp_gpu_time(uint32_t p_index){
		return RD::get_singleton()->get_captured_timestamp_gpu_time(p_index);
	}

	uint64_t get_captured_timestamp_cpu_time(uint32_t p_index){
		return RD::get_singleton()->get_captured_timestamp_cpu_time(p_index);
	}

	std::string get_captured_timestamp_name(uint32_t p_index){
		return RD::get_singleton()->get_captured_timestamp_name(p_index);
	}
}