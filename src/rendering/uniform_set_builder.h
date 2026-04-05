// uniform_set_builder.h

#pragma once
#include "rendering/rendering_device.h"
#include "rendering/uniform_buffer.h"

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

		// Add a sampler + texture pair
		UniformSetBuilder& add_texture(uint32_t binding, RID sampler, RID texture) {
			RD::Uniform u;
			u.uniform_type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = binding;
			u.append_id(sampler);
			u.append_id(texture);
			uniforms.push_back(u);
			return *this;
		}

		RID build(RenderingDevice* device, RID shader, uint32_t set_index = 0) {
			return device->uniform_set_create(uniforms, shader, set_index);
		}

	private:
		std::vector<RD::Uniform> uniforms;
	};

} // namespace Rendering