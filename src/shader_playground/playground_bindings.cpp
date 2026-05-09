#include "shader_playground/playground_bindings.h"

namespace ShaderPlayground {

void PlaygroundBindings::initialize(Rendering::RenderingDevice* p_device, Rendering::TextureCache* p_textures, RID p_shader, RID p_sampler)
{
	device = p_device;
	textures = p_textures;
	shader = p_shader;
	sampler = p_sampler;
}

void PlaygroundBindings::clear()
{
	uniform_set.reset();
	owned_buffers.clear();
	uniforms = Rendering::UniformSetBuilder{};
	dirty = false;
}

PlaygroundBindings& PlaygroundBindings::texture(uint32_t binding, const std::string& path, bool srgb)
{
	RID texture = srgb ? textures->color(path) : textures->linear(path);
	uniforms.add_texture(binding, sampler, texture);
	dirty = true;
	return *this;
}

PlaygroundBindings& PlaygroundBindings::texture_linear(uint32_t binding, const std::string& path)
{
	return texture(binding, path, false);
}

RID PlaygroundBindings::build(uint32_t set_index)
{
	if (dirty) {
		uniform_set = RIDHandle(uniforms.build(device, shader, set_index));
		dirty = false;
	}
	return uniform_set;
}

} // namespace ShaderPlayground
