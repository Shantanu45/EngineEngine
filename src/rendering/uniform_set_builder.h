// uniform_set_builder.h

#pragma once
#include "rendering/rendering_device.h"
#include "rendering/uniform_buffer.h"
#include "util/small_vector.h"

namespace Rendering {
	using RD = RenderingDevice;
	using RDC = RenderingDeviceCommons;


	class UniformSetBuilder {
	public:

		// Add a pre-built uniform directly (works with UniformBuffer<T>::as_uniform())
		UniformSetBuilder& add(RD::Uniform uniform) {
			uniforms.push_back(uniform);
			return *this;
		}

		// Add a sampler + texture pair (combined descriptor)
		UniformSetBuilder& add_texture(uint32_t binding, RID sampler, RID texture) {
			RD::Uniform u;
			u.uniform_type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = binding;
			u.append_id(sampler);
			u.append_id(texture);
			uniforms.push_back(u);
			return *this;
		}

		// Add a standalone sampler (no texture)
		UniformSetBuilder& add_sampler(uint32_t binding, RID sampler) {
			RD::Uniform u;
			u.uniform_type = RDC::UNIFORM_TYPE_SAMPLER;
			u.binding = binding;
			u.append_id(sampler);
			uniforms.push_back(u);
			return *this;
		}

		// Add a texture without a sampler (pair with a standalone sampler in the shader)
		UniformSetBuilder& add_texture_only(uint32_t binding, RID texture) {
			RD::Uniform u;
			u.uniform_type = RDC::UNIFORM_TYPE_TEXTURE;
			u.binding = binding;
			u.append_id(texture);
			uniforms.push_back(u);
			return *this;
		}

		RID build(RenderingDevice* device, RID shader, uint32_t set_index = 0) {
			return device->uniform_set_create(uniforms, shader, set_index);
		}

		RID build_cached(RenderingDevice* device, RID shader, uint32_t set_index = 0) {
			return device->uniform_set_get_or_create(uniforms, shader, set_index);
		}

	private:
		Util::SmallVector<RD::Uniform> uniforms;
	};

} // namespace Rendering
