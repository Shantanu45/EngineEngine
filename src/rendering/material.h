#pragma once
#include "rendering_device.h"
#include "uniform_set_builder.h"

struct alignas(16) Material_UBO {
	glm::vec4 base_color_factor;
	float metallic_factor;
	float roughness_factor;
	float shininess;            // <-- for blinn phong
};

namespace Rendering
{
	using MaterialHandle = uint32_t;
	constexpr MaterialHandle INVALID_MATERIAL = ~0u;

	class Material
	{
	public:
		glm::vec4 base_color_factor = glm::vec4(1.0f);
		float     metallic_factor = 0.0f;
		float     roughness_factor = 0.5f;
		float     shininess = 32.0f;

		RID diffuse;
		RID metallic_roughness;
		RID normal;

		void create(RenderingDevice* device, const std::string& name = "Material UBO") {
			ubo.create(device, name.c_str());
		}

		void upload(RenderingDevice* device) {
			ubo.upload(device, Material_UBO{
				.base_color_factor = base_color_factor,
				.metallic_factor = metallic_factor,
				.roughness_factor = roughness_factor,
				.shininess = shininess,
				});
		}

		RID build_uniform_set(
			RenderingDevice* device,
			RID sampler,
			RID fallback,
			const std::string& shader_name)
		{
			return Rendering::UniformSetBuilder{}
				.add(ubo.as_uniform(0))
				.add_texture(1, sampler, diffuse.is_valid() ? diffuse : fallback)
				.add_texture(2, sampler, metallic_roughness.is_valid() ? metallic_roughness : fallback)
				.add_texture(3, sampler, normal.is_valid() ? normal : fallback)
				.build(device, device->get_shader_rid(shader_name), 2);
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
		MaterialHandle create(RenderingDevice* device, Material mat, RID sampler, RID fallback, const std::string& shader)
		{
			mat.create(device);
			RID us = mat.build_uniform_set(device, sampler, fallback, shader);

			MaterialHandle h = static_cast<MaterialHandle>(materials.size());
			materials.push_back(std::move(mat));
			uniform_sets.push_back(us);
			return h;
		}

		Material& get(MaterialHandle h) { return materials[h]; }
		RID       get_uniform_set(MaterialHandle h) { return uniform_sets[h]; }

		void upload_all(RenderingDevice* device) {
			for (auto& mat : materials)
				mat.upload(device);
		}

		void free_all(RenderingDevice* device) {
			for (auto& mat : materials)
				mat.free(device);
		}

	private:
		std::vector<Material> materials;
		std::vector<RID>      uniform_sets;
	};
}

