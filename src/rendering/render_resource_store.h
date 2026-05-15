#pragma once

#include <array>
#include <string>

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
	RID load_skybox_cubemap(const std::array<std::string, 6>& faces);

	TextureCache& textures() { return texture_cache; }
	RenderAssetRegistry& assets() { return asset_registry; }
	MaterialRegistry& materials() { return material_registry; }
	MeshStorage& meshes() { return mesh_storage; }
	RID default_white_texture() const { return white_texture; }
	RID default_normal_texture() const { return normal_texture; }
	const MaterialFallbackTextures& default_material_fallbacks() const { return material_fallbacks; }
	RID skybox_cubemap() const { return skybox_texture; }

	const TextureCache& textures() const { return texture_cache; }
	const RenderAssetRegistry& assets() const { return asset_registry; }
	const MaterialRegistry& materials() const { return material_registry; }
	const MeshStorage& meshes() const { return mesh_storage; }

private:
	RenderingDevice* device = nullptr;
	FileSystem::FilesystemInterface* filesystem = nullptr;
	RIDHandle white_texture;
	RIDHandle metallic_roughness_texture;
	RIDHandle normal_texture;
	RIDHandle missing_texture;
	MaterialFallbackTextures material_fallbacks;
	RIDHandle skybox_texture;
	TextureCache texture_cache;
	RenderAssetRegistry asset_registry;
	MaterialRegistry material_registry;
	MeshStorage mesh_storage;
	bool initialized = false;
};

} // namespace Rendering
