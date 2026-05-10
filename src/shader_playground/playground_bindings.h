#pragma once

#include <memory>
#include <string>
#include <vector>

#include "rendering/texture_cache.h"
#include "rendering/uniform_buffer.h"
#include "rendering/uniform_set_builder.h"

namespace ShaderPlayground {

class PlaygroundBindings {
public:
	void initialize(Rendering::RenderingDevice* device, Rendering::TextureCache* textures, RID shader, RID sampler);
	void clear();

	template <typename T>
	PlaygroundBindings& ubo(uint32_t binding, const T& data, const char* debug_name = "Playground user UBO")
	{
		auto buffer = std::make_unique<TypedUniformBuffer<T>>();
		buffer->ubo.create(device, debug_name);
		buffer->ubo.upload(device, data);
		uniforms.add(buffer->ubo.as_uniform(binding));
		owned_buffers.push_back(std::move(buffer));
		dirty = true;
		return *this;
	}

	PlaygroundBindings& texture(uint32_t binding, const std::string& path, bool srgb = true);
	PlaygroundBindings& texture_linear(uint32_t binding, const std::string& path);

	RID build(uint32_t set_index = 1);
	RID get() const { return uniform_set; }

private:
	struct OwnedBuffer {
		virtual ~OwnedBuffer() = default;
	};

	template <typename T>
	struct TypedUniformBuffer final : OwnedBuffer {
		Rendering::UniformBuffer<T> ubo;
	};

	Rendering::RenderingDevice* device = nullptr;
	Rendering::TextureCache* textures = nullptr;
	RID shader;
	RID sampler;
	Rendering::UniformSetBuilder uniforms;
	std::vector<std::unique_ptr<OwnedBuffer>> owned_buffers;
	RIDHandle uniform_set;
	bool dirty = false;
};

} // namespace ShaderPlayground
