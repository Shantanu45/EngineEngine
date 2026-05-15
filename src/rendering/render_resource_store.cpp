#include "rendering/render_resource_store.h"

#include "rendering/default_textures.h"
#include "rendering/skybox.h"

namespace Rendering {

void RenderResourceStore::initialize(RenderingDevice* device_, FileSystem::Filesystem& filesystem)
{
	device = device_;
	this->filesystem = &filesystem;
	texture_cache.init(device, filesystem);
	mesh_storage.initialize(device);
	white_texture = RIDHandle(create_white_texture(device));
	metallic_roughness_texture = RIDHandle(create_metallic_roughness_texture(device));
	normal_texture = RIDHandle(create_flat_normal_texture(device));
	missing_texture = RIDHandle(create_missing_texture(device));
	texture_cache.set_missing_texture(missing_texture);
	material_fallbacks = MaterialFallbackTextures{
		.color = white_texture,
		.metallic_roughness = metallic_roughness_texture,
		.normal = normal_texture,
		.displacement = white_texture,
		.emissive = white_texture,
		.occlusion = white_texture,
		.missing = missing_texture,
	};
	initialized = true;
}

RID RenderResourceStore::load_skybox_cubemap(const std::array<std::string, 6>& faces)
{
	skybox_texture = RIDHandle(Rendering::load_skybox_cubemap(device, *filesystem, faces));
	return skybox_texture;
}

void RenderResourceStore::shutdown()
{
	if (!initialized)
		return;

	asset_registry.clear();
	material_registry.free_all(device);
	texture_cache.free_all();
	mesh_storage.finalize();
	skybox_texture.reset();
	material_fallbacks = {};
	texture_cache.set_missing_texture(RID());
	missing_texture.reset();
	normal_texture.reset();
	metallic_roughness_texture.reset();
	white_texture.reset();

	initialized = false;
	device = nullptr;
	filesystem = nullptr;
}

} // namespace Rendering
