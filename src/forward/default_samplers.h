#pragma once
#include "rendering/rendering_device.h"

namespace Rendering
{
	// --- SamplerState builders ---

	// Nearest filter, clamp to edge on all axes. The default SamplerState.
	inline RDC::SamplerState sampler_state_nearest_clamp()
	{
		return RDC::SamplerState{};
	}

	// Linear mag/min/mip, clamp to edge. General-purpose for UI, post-process, blit.
	inline RDC::SamplerState sampler_state_linear_clamp()
	{
		RDC::SamplerState s;
		s.mag_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.mip_filter = RDC::SAMPLER_FILTER_LINEAR;
		return s; // repeat_* defaults to CLAMP_TO_EDGE
	}

	// Linear mag/min/mip, repeat on all axes. General-purpose for tiled textures.
	inline RDC::SamplerState sampler_state_linear_repeat()
	{
		RDC::SamplerState s;
		s.mag_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.mip_filter = RDC::SAMPLER_FILTER_LINEAR;
		s.repeat_u   = RDC::SAMPLER_REPEAT_MODE_REPEAT;
		s.repeat_v   = RDC::SAMPLER_REPEAT_MODE_REPEAT;
		s.repeat_w   = RDC::SAMPLER_REPEAT_MODE_REPEAT;
		return s;
	}

	// Linear, clamp, depth-compare (LESS_OR_EQUAL). For use with sampler2DShadow.
	inline RDC::SamplerState sampler_state_pcf_shadow()
	{
		RDC::SamplerState s;
		s.mag_filter     = RDC::SAMPLER_FILTER_LINEAR;
		s.min_filter     = RDC::SAMPLER_FILTER_LINEAR;
		s.mip_filter     = RDC::SAMPLER_FILTER_NEAREST;
		// repeat_* defaults to CLAMP_TO_EDGE
		s.enable_compare = true;
		s.compare_op     = RDC::COMPARE_OP_LESS_OR_EQUAL;
		return s;
	}

	// Nearest, clamp. For point-light shadow cubemaps (manual comparison in shader).
	inline RDC::SamplerState sampler_state_point_shadow()
	{
		return RDC::SamplerState{}; // nearest + clamp is already the default
	}

	// --- Convenience create helpers ---

	inline RID create_sampler_nearest_clamp(RenderingDevice* device)
	{
		return device->sampler_create(sampler_state_nearest_clamp());
	}

	inline RID create_sampler_linear_clamp(RenderingDevice* device)
	{
		return device->sampler_create(sampler_state_linear_clamp());
	}

	inline RID create_sampler_linear_repeat(RenderingDevice* device)
	{
		return device->sampler_create(sampler_state_linear_repeat());
	}

	inline RID create_sampler_pcf_shadow(RenderingDevice* device)
	{
		return device->sampler_create(sampler_state_pcf_shadow());
	}

	inline RID create_sampler_point_shadow(RenderingDevice* device)
	{
		return device->sampler_create(sampler_state_point_shadow());
	}
} // namespace Rendering
