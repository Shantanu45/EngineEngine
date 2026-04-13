#pragma once
#include "rendering/rendering_device.h"

namespace Rendering
{
	// Creates a 1x1 RGBA8 texture with the given RGBA byte values.
	inline RID create_1x1_texture(RenderingDevice* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a, const char* name)
	{
		RDC::TextureFormat tf;
		tf.texture_type  = RDC::TEXTURE_TYPE_2D;
		tf.width         = 1;
		tf.height        = 1;
		tf.array_layers  = 1;
		tf.usage_bits    = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
		tf.format        = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

		std::vector<uint8_t> pixel = { r, g, b, a };
		RID rid = device->texture_create(tf, RD::TextureView(), { pixel });
		device->set_resource_name(rid, name);
		return rid;
	}

	// 1x1 opaque white — general-purpose fallback for any colour/albedo slot.
	inline RID create_white_texture(RenderingDevice* device)
	{
		return create_1x1_texture(device, 255, 255, 255, 255, "fallback_white");
	}

	// 1x1 black — useful as a fallback for emissive, AO, or specular slots.
	inline RID create_black_texture(RenderingDevice* device)
	{
		return create_1x1_texture(device, 0, 0, 0, 255, "fallback_black");
	}

	// 1x1 flat normal (0.5, 0.5, 1.0) encoded as RGBA8 — fallback for normal maps.
	inline RID create_flat_normal_texture(RenderingDevice* device)
	{
		return create_1x1_texture(device, 128, 128, 255, 255, "fallback_normal");
	}

	// 1x1 magenta — intentionally ugly so missing textures are obvious in the viewport.
	inline RID create_missing_texture(RenderingDevice* device)
	{
		return create_1x1_texture(device, 255, 0, 255, 255, "fallback_missing");
	}
} // namespace Rendering
