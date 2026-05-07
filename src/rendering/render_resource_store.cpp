#include "rendering/render_resource_store.h"

#include "rendering/default_textures.h"

namespace Rendering {

void RenderResourceStore::initialize(RenderingDevice* device_, FileSystem::Filesystem& filesystem)
{
	device = device_;
	texture_cache.init(device, filesystem);
	mesh_storage.initialize(device);
	white_texture = RIDHandle(create_white_texture(device));
	initialized = true;
}

void RenderResourceStore::shutdown()
{
	if (!initialized)
		return;

	asset_registry.clear();
	material_registry.free_all(device);
	texture_cache.free_all();
	mesh_storage.finalize();
	white_texture.reset();

	initialized = false;
	device = nullptr;
}

} // namespace Rendering
