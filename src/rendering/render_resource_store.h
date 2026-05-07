#pragma once

#include "filesystem/filesystem.h"
#include "rendering/material.h"
#include "rendering/mesh_storage.h"
#include "rendering/render_asset_registry.h"
#include "rendering/rid_handle.h"
#include "rendering/texture_cache.h"

namespace Rendering {

class RenderResourceStore {
public:
	void initialize(RenderingDevice* device, FileSystem::Filesystem& filesystem);
	void shutdown();

	TextureCache& textures() { return texture_cache; }
	RenderAssetRegistry& assets() { return asset_registry; }
	MaterialRegistry& materials() { return material_registry; }
	MeshStorage& meshes() { return mesh_storage; }
	RID default_white_texture() const { return white_texture; }

	const TextureCache& textures() const { return texture_cache; }
	const RenderAssetRegistry& assets() const { return asset_registry; }
	const MaterialRegistry& materials() const { return material_registry; }
	const MeshStorage& meshes() const { return mesh_storage; }

private:
	RenderingDevice* device = nullptr;
	RIDHandle white_texture;
	TextureCache texture_cache;
	RenderAssetRegistry asset_registry;
	MaterialRegistry material_registry;
	MeshStorage mesh_storage;
	bool initialized = false;
};

} // namespace Rendering
