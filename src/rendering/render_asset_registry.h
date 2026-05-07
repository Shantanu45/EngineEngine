#pragma once

#include <cstdint>
#include <unordered_map>

#include "rendering/material.h"
#include "rendering/mesh_storage.h"
#include "util/small_vector.h"

namespace Rendering {

struct MeshAssetHandle {
	uint32_t id = 0;
};

struct MaterialAssetHandle {
	uint32_t id = 0;
};

inline constexpr MeshAssetHandle INVALID_MESH_ASSET{};
inline constexpr MaterialAssetHandle INVALID_MATERIAL_ASSET{};

class RenderAssetRegistry {
public:
	MeshAssetHandle register_mesh(MeshHandle mesh);
	MaterialAssetHandle register_material(MaterialHandle material);

	MeshHandle resolve_mesh(MeshAssetHandle handle) const;
	MaterialHandle resolve_material(MaterialAssetHandle handle) const;

	void clear();

private:
	Util::SmallVector<MeshHandle> meshes;
	Util::SmallVector<MaterialHandle> materials;
	std::unordered_map<MeshHandle, uint32_t> mesh_to_asset;
	std::unordered_map<MaterialHandle, uint32_t> material_to_asset;
};

} // namespace Rendering
