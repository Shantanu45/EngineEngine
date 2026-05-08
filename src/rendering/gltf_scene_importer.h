#pragma once

#include <string>

#include "rendering/aabb.h"
#include "rendering/material.h"
#include "rendering/mesh_loader.h"
#include "rendering/mesh_storage.h"
#include "rendering/texture_cache.h"
#include "util/small_vector.h"

namespace Rendering {

struct GltfPrimitiveInstance {
	MeshHandle mesh = INVALID_MESH;
	Util::SmallVector<MaterialHandle> materials;
	AABB local_aabb;
	int node_index = -1;
	int mesh_index = -1;
	int primitive_index = -1;
};

struct ImportedGltfScene {
	Util::SmallVector<MaterialHandle> materials;
	Util::SmallVector<GltfPrimitiveInstance> primitives;
};

struct GltfSceneImportContext {
	MeshLoader& mesh_loader;
	TextureCache& texture_cache;
	MeshStorage& mesh_storage;
	MaterialRegistry& material_registry;
	RenderingDevice* device = nullptr;
	RID fallback_texture;
	RID shader_rid;
	RID pbr_shader_rid;
	RID shadow_shader_rid;
	RID point_shadow_shader_rid;
	RenderingDevice::VertexFormatID vertex_format = 0;
	std::string source_path;
	std::string name_prefix;
};

class GltfSceneImporter {
public:
	ImportedGltfScene import(const GltfSceneImportContext& context) const;
};

} // namespace Rendering
