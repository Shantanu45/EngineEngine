#include "rendering/render_asset_registry.h"

namespace Rendering {

MeshAssetHandle RenderAssetRegistry::register_mesh(MeshHandle mesh)
{
	if (mesh == INVALID_MESH)
		return INVALID_MESH_ASSET;

	auto it = mesh_to_asset.find(mesh);
	if (it != mesh_to_asset.end())
		return MeshAssetHandle{ .id = it->second };

	uint32_t id = static_cast<uint32_t>(meshes.size() + 1);
	meshes.push_back(mesh);
	mesh_to_asset.emplace(mesh, id);
	return MeshAssetHandle{ .id = id };
}

MaterialAssetHandle RenderAssetRegistry::register_material(MaterialHandle material)
{
	if (material == INVALID_MATERIAL)
		return INVALID_MATERIAL_ASSET;

	auto it = material_to_asset.find(material);
	if (it != material_to_asset.end())
		return MaterialAssetHandle{ .id = it->second };

	uint32_t id = static_cast<uint32_t>(materials.size() + 1);
	materials.push_back(material);
	material_to_asset.emplace(material, id);
	return MaterialAssetHandle{ .id = id };
}

MeshHandle RenderAssetRegistry::resolve_mesh(MeshAssetHandle handle) const
{
	if (handle.id == 0 || handle.id > meshes.size())
		return INVALID_MESH;
	return meshes[handle.id - 1];
}

MaterialHandle RenderAssetRegistry::resolve_material(MaterialAssetHandle handle) const
{
	if (handle.id == 0 || handle.id > materials.size())
		return INVALID_MATERIAL;
	return materials[handle.id - 1];
}

void RenderAssetRegistry::clear()
{
	meshes.clear();
	materials.clear();
	mesh_to_asset.clear();
	material_to_asset.clear();
}

} // namespace Rendering
