#pragma once
// material.h must be included before gltf_loader.h.
// gltf_loader.h pulls in tiny_gltf.h -> Windows headers which define OPAQUE as 2,
// clobbering enum values and RenderingDevice symbols in material.h.
#include "material.h"
#include "gltf_loader.h"

namespace Rendering
{
	// Convert a loaded PBRMaterial to a runtime Material.
	// image_rids must be indexed by GltfScene::images — one RID per uploaded image.
	inline Material material_from_pbr(const PBRMaterial& pbr, const std::vector<RID>& image_rids)
	{
		auto get_rid = [&](const std::optional<TextureInfo>& ti) -> RID {
			if (!ti || ti->image_index < 0 || ti->image_index >= (int)image_rids.size())
				return RID{};
			return image_rids[ti->image_index];
		};

		Material mat;
		mat.base_color_factor  = pbr.base_color_factor;
		mat.metallic_factor    = pbr.metallic_factor;
		mat.roughness_factor   = pbr.roughness_factor;
		mat.emissive_factor    = pbr.emissive_factor;
		mat.normal_scale       = pbr.normal_scale;
		mat.occlusion_strength = pbr.occlusion_strength;
		mat.alpha_cutoff       = pbr.alpha_cutoff;
		mat.double_sided       = pbr.double_sided;

		if      (pbr.alpha_mode == "MASK")  mat.alpha_mode = AlphaMode::Mask;
		else if (pbr.alpha_mode == "BLEND") mat.alpha_mode = AlphaMode::Blend;
		else                                mat.alpha_mode = AlphaMode::Opaque;

		mat.diffuse            = get_rid(pbr.base_color_texture);
		mat.metallic_roughness = get_rid(pbr.metallic_roughness_texture);
		mat.normal             = get_rid(pbr.normal_texture);
		mat.occlusion          = get_rid(pbr.occlusion_texture);
		mat.emissive           = get_rid(pbr.emissive_texture);
		mat.dirty = true;
		return mat;
	}
}
