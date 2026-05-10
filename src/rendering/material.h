#pragma once
#include "rendering_device.h"
#include "uniform_set_builder.h"
#include "rid_handle.h"
#include "util/small_vector.h"

#include <cstdint>

// Must match the GLSL Material struct in lib/lighting.glsl (std140 layout).
struct alignas(16) Material_UBO {
	glm::vec4 base_color_factor;     // offset  0
	float     metallic_factor;       // offset 16
	float     roughness_factor;      // offset 20
	float     shininess;             // offset 24  (Blinn-Phong)
	float     alpha_cutoff;          // offset 28
	glm::vec4 emissive_and_normal;   // offset 32  xyz=emissive_factor, w=normal_scale
	float     occlusion_strength;    // offset 48
	uint32_t  alpha_mode;            // offset 52  Rendering::AlphaMode
	float     _pad1, _pad2;          // offset 56-60  (pad to 64 bytes)
};

namespace Rendering
{
	using MaterialHandle = uint32_t;
	constexpr MaterialHandle INVALID_MATERIAL = ~0u;

	enum class AlphaMode { Opaque, Mask, Blend };

	class Material
	{
	public:
		glm::vec4  base_color_factor  = glm::vec4(1.0f);
		float      metallic_factor    = 0.0f;
		float      roughness_factor   = 0.5f;
		float      shininess          = 32.0f;
		glm::vec3  emissive_factor    = glm::vec3(0.0f);
		float      normal_scale       = 1.0f;
		float      occlusion_strength = 1.0f;
		float      alpha_cutoff       = 0.5f;
		AlphaMode  alpha_mode         = AlphaMode::Opaque;
		bool       double_sided       = false;

		RID diffuse;
		RID metallic_roughness;
		RID normal;
		RID displacement;
		RID emissive;
		RID occlusion;

		bool dirty = true;  // set to true after mutating any field

		void create(RenderingDevice* device, const std::string& name = "Material UBO") {
			ubo.create(device, name.c_str());
		}

		void upload(RenderingDevice* device) {
			ubo.upload(device, Material_UBO{
				.base_color_factor   = base_color_factor,
				.metallic_factor     = metallic_factor,
				.roughness_factor    = roughness_factor,
				.shininess           = shininess,
				.alpha_cutoff        = alpha_cutoff,
				.emissive_and_normal = glm::vec4(emissive_factor, normal_scale),
				.occlusion_strength  = occlusion_strength,
				.alpha_mode          = static_cast<uint32_t>(alpha_mode),
				});
			dirty = false;
		}

		void upload_if_dirty(RenderingDevice* device) {
			if (dirty) upload(device);
		}

		RID build_uniform_set(
			RenderingDevice* device,
			RID fallback,
			RID shader_rid)
		{
			return Rendering::UniformSetBuilder{}
				.add(ubo.as_uniform(0))
				.add_texture_only(1, diffuse.is_valid()            ? diffuse            : fallback)
				.add_texture_only(2, metallic_roughness.is_valid() ? metallic_roughness : fallback)
				.add_texture_only(3, normal.is_valid()             ? normal             : fallback)
				.add_texture_only(4, displacement.is_valid()       ? displacement       : fallback)
				.add_texture_only(5, emissive.is_valid()           ? emissive           : fallback)
				.add_texture_only(6, occlusion.is_valid()          ? occlusion          : fallback)
				.build(device, shader_rid, 2);
		}

		void free(RenderingDevice* device) {
			ubo.free(device);
		}

	private:
		Rendering::UniformBuffer<Material_UBO> ubo;
	};

	class MaterialRegistry
	{
	public:
		MaterialHandle create(RenderingDevice* device, Material mat, RID fallback, RID shader_rid)
		{
			mat.create(device);
			RIDHandle us(mat.build_uniform_set(device, fallback, shader_rid));

			MaterialHandle h = static_cast<MaterialHandle>(materials.size());
			materials.push_back(std::move(mat));
			uniform_sets.push_back(std::move(us));
			pbr_uniform_sets.push_back(RIDHandle());
			shadow_uniform_sets.push_back(RIDHandle());
			point_shadow_uniform_sets.push_back(RIDHandle());
			transparent_uniform_sets.push_back(RIDHandle());
			transparent_pbr_uniform_sets.push_back(RIDHandle());
			return h;
		}

		MaterialHandle create(RenderingDevice* device, Material mat, RID fallback, RID shader_rid, RID pbr_shader_rid)
		{
			return create(device, std::move(mat), fallback, shader_rid, pbr_shader_rid, RID(), RID());
		}

		MaterialHandle create(RenderingDevice* device, Material mat, RID fallback, RID shader_rid,
			RID pbr_shader_rid, RID shadow_shader_rid, RID point_shadow_shader_rid,
			RID transparent_shader_rid = RID(), RID transparent_pbr_shader_rid = RID())
		{
			mat.create(device);
			RIDHandle us(mat.build_uniform_set(device, fallback, shader_rid));
			RIDHandle pbr_us(pbr_shader_rid.is_valid()
				? mat.build_uniform_set(device, fallback, pbr_shader_rid)
				: RID());
			RIDHandle shadow_us(shadow_shader_rid.is_valid()
				? mat.build_uniform_set(device, fallback, shadow_shader_rid)
				: RID());
			RIDHandle point_shadow_us(point_shadow_shader_rid.is_valid()
				? mat.build_uniform_set(device, fallback, point_shadow_shader_rid)
				: RID());
			RIDHandle transparent_us(transparent_shader_rid.is_valid()
				? mat.build_uniform_set(device, fallback, transparent_shader_rid)
				: RID());
			RIDHandle transparent_pbr_us(transparent_pbr_shader_rid.is_valid()
				? mat.build_uniform_set(device, fallback, transparent_pbr_shader_rid)
				: RID());

			MaterialHandle h = static_cast<MaterialHandle>(materials.size());
			materials.push_back(std::move(mat));
			uniform_sets.push_back(std::move(us));
			pbr_uniform_sets.push_back(std::move(pbr_us));
			shadow_uniform_sets.push_back(std::move(shadow_us));
			point_shadow_uniform_sets.push_back(std::move(point_shadow_us));
			transparent_uniform_sets.push_back(std::move(transparent_us));
			transparent_pbr_uniform_sets.push_back(std::move(transparent_pbr_us));
			return h;
		}

		Material& get(MaterialHandle h) { return materials[h]; }
		RID       get_uniform_set(MaterialHandle h) { return uniform_sets[h]; }
		RID       get_uniform_set(MaterialHandle h, bool use_pbr_lighting) {
			if (use_pbr_lighting && pbr_uniform_sets[h].is_valid())
				return pbr_uniform_sets[h];
			return uniform_sets[h];
		}
		RID       get_shadow_uniform_set(MaterialHandle h) {
			if (shadow_uniform_sets[h].is_valid())
				return shadow_uniform_sets[h];
			return uniform_sets[h];
		}
		RID       get_point_shadow_uniform_set(MaterialHandle h) {
			if (point_shadow_uniform_sets[h].is_valid())
				return point_shadow_uniform_sets[h];
			return uniform_sets[h];
		}
		RID       get_transparent_uniform_set(MaterialHandle h, bool use_pbr_lighting) {
			if (use_pbr_lighting && transparent_pbr_uniform_sets[h].is_valid())
				return transparent_pbr_uniform_sets[h];
			if (transparent_uniform_sets[h].is_valid())
				return transparent_uniform_sets[h];
			return get_uniform_set(h, use_pbr_lighting);
		}
		bool      is_blend(MaterialHandle h) const {
			return h != INVALID_MATERIAL && materials[h].alpha_mode == AlphaMode::Blend;
		}

		void upload_all(RenderingDevice* device) {
			for (auto& mat : materials)
				mat.upload(device);
		}

		void upload_dirty(RenderingDevice* device) {
			for (auto& mat : materials)
				mat.upload_if_dirty(device);
		}

		void free_all(RenderingDevice* device) {
			// Uniform sets must be freed before their dependency UBOs.
			// Destroy in reverse order: uniform_sets first, then materials (which own the UBOs).
			for (int i = (int)point_shadow_uniform_sets.size() - 1; i >= 0; --i)
				point_shadow_uniform_sets[i].reset();
			for (int i = (int)transparent_pbr_uniform_sets.size() - 1; i >= 0; --i)
				transparent_pbr_uniform_sets[i].reset();
			for (int i = (int)transparent_uniform_sets.size() - 1; i >= 0; --i)
				transparent_uniform_sets[i].reset();
			for (int i = (int)shadow_uniform_sets.size() - 1; i >= 0; --i)
				shadow_uniform_sets[i].reset();
			for (int i = (int)pbr_uniform_sets.size() - 1; i >= 0; --i)
				pbr_uniform_sets[i].reset();
			for (int i = (int)uniform_sets.size() - 1; i >= 0; --i)
				uniform_sets[i].reset();
			for (auto& mat : materials)
				mat.free(device);
		}

	private:
		// materials (owns UBOs) must be declared before uniform_sets so that
		// uniform_sets are destroyed first — preventing cascade double-free.
		Util::SmallVector<Material>   materials;
		Util::SmallVector<RIDHandle>  uniform_sets;
		Util::SmallVector<RIDHandle>  pbr_uniform_sets;
		Util::SmallVector<RIDHandle>  shadow_uniform_sets;
		Util::SmallVector<RIDHandle>  point_shadow_uniform_sets;
		Util::SmallVector<RIDHandle>  transparent_uniform_sets;
		Util::SmallVector<RIDHandle>  transparent_pbr_uniform_sets;
	};
}

