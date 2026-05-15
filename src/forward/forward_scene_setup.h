#pragma once

#include <string>

#include "entt/entt.hpp"
#include "filesystem/filesystem.h"
#include "rendering/scene/scene_asset_handles.h"
#include "rendering/render_resource_store.h"
#include "rendering/mesh_storage.h"
#include "rendering/rendering_device.h"
#include "util/small_vector.h"

struct ForwardDemoLightMeshes {
	SceneMeshAssetHandle directional = INVALID_SCENE_MESH_ASSET;
	SceneMeshAssetHandle point = INVALID_SCENE_MESH_ASSET;
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
	RID fallback_normal_texture;
	RID shader_rid;
	RID pbr_shader_rid;
	RID shadow_shader_rid;
	RID point_shadow_shader_rid;
	RID transparent_shader_rid;
	RID transparent_pbr_shader_rid;
	Rendering::RenderingDevice::VertexFormatID vertex_format = 0;
	std::string path;
	std::string name_prefix;
};

void add_default_forward_lights(entt::registry& world, ForwardDemoLightMeshes meshes);
ForwardSceneHandle instantiate_forward_gltf_scene(const ForwardGltfSceneRequest& request);
