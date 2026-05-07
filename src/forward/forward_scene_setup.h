#pragma once

#include <string>

#include "entt/entt.hpp"
#include "filesystem/filesystem.h"
#include "rendering/render_asset_registry.h"
#include "rendering/render_resource_store.h"
#include "rendering/mesh_storage.h"
#include "rendering/rendering_device.h"
#include "util/small_vector.h"

struct ForwardDemoLightMeshes {
	Rendering::MeshAssetHandle directional = Rendering::INVALID_MESH_ASSET;
	Rendering::MeshAssetHandle point = Rendering::INVALID_MESH_ASSET;
};

struct ForwardSceneHandle {
	Util::SmallVector<entt::entity> entities;
};

struct ForwardGltfSceneRequest {
	entt::registry& world;
	FileSystem::FilesystemInterface& filesystem;
	Rendering::RenderingDevice* device = nullptr;
	Rendering::RenderResourceStore& resources;
	RID fallback_texture;
	RID shader_rid;
	Rendering::RenderingDevice::VertexFormatID vertex_format = 0;
	std::string path;
	std::string name_prefix;
};

void add_default_forward_lights(entt::registry& world, ForwardDemoLightMeshes meshes);
ForwardSceneHandle load_forward_gltf_scene(const ForwardGltfSceneRequest& request);
