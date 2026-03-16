#include "rendering_device_driver.h"

namespace Rendering
{
	/**************/
	/**** MISC ****/
	/**************/

	uint64_t RenderingDeviceDriver::api_trait_get(ApiTrait p_trait) {
		// Sensible canonical defaults.
		switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return 1;
		case API_TRAIT_SHADER_CHANGE_INVALIDATION:
			return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
		case API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT:
			return 1;
		case API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP:
			return 1;
		case API_TRAIT_SECONDARY_VIEWPORT_SCISSOR:
			return 1;
		case API_TRAIT_CLEARS_WITH_COPY_ENGINE:
			return true;
		case API_TRAIT_USE_GENERAL_IN_COPY_QUEUES:
			return false;
		case API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS:
			return false;
		case API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS:
			return false;
		default:
			ERR_FAIL_V(0);
		}
	}

	RenderingDeviceDriver::~RenderingDeviceDriver() {}
}
