#pragma once

#include "rendering/render_asset_registry.h"

using SceneMeshAssetHandle = Rendering::MeshAssetHandle;
using SceneMaterialAssetHandle = Rendering::MaterialAssetHandle;

inline constexpr SceneMeshAssetHandle INVALID_SCENE_MESH_ASSET = Rendering::INVALID_MESH_ASSET;
inline constexpr SceneMaterialAssetHandle INVALID_SCENE_MATERIAL_ASSET = Rendering::INVALID_MATERIAL_ASSET;
