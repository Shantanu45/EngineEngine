#include "forward/forward_scene_setup.h"

#include "forward/scene/components.h"
#include "forward/scene/transform_system.h"
#include "rendering/gltf_scene_importer.h"
#include "rendering/mesh_loader.h"
#include "util/logger.h"

#include <utility>

void add_default_forward_lights(entt::registry& world, ForwardDemoLightMeshes meshes)
{
	auto dir_light = world.create();
	world.emplace<LocalTransform>(dir_light, LocalTransform{
		.position = glm::vec3(5.0f, 10.0f, 5.0f),
		.scale    = glm::vec3(0.2f) });
	world.emplace<MeshComponent>(dir_light, MeshComponent{
		.mesh     = meshes.directional,
		.category = SceneMeshCategory::LightVisualization,
	});
	world.emplace<LightComponent>(dir_light, LightComponent{
		.type      = SceneLightType::Directional,
		.direction = glm::vec3(0.0f, -1.0f, -0.5f),
		.color     = glm::vec3(1.0f),
		.range     = 15.0f,
		.intensity = 1.0f,
	});

	auto pt_light = world.create();
	world.emplace<LocalTransform>(pt_light, LocalTransform{
		.position = glm::vec3(1.0f, 1.0f, 1.0f),
		.scale    = glm::vec3(0.1f) });
	world.emplace<MeshComponent>(pt_light, MeshComponent{
		.mesh     = meshes.point,
		.category = SceneMeshCategory::LightVisualization,
	});
	world.emplace<LightComponent>(pt_light, LightComponent{
		.type      = SceneLightType::Point,
		.direction = glm::vec3(-0.5f, -1.0f, -0.5f),
		.color     = glm::vec3(1.0f, 0.0f, 0.0f),
		.range     = 15.0f,
		.intensity = 1.0f,
	});
}

ForwardSceneHandle instantiate_forward_gltf_scene(const ForwardGltfSceneRequest& request)
{
	ForwardSceneHandle handle;

	Rendering::MeshLoader mesh_loader(request.filesystem, request.device);
	if (!mesh_loader.load_file(request.path)) {
		LOGE("instantiate_forward_gltf_scene: failed to load '{}'", request.path);
		return handle;
	}

	Rendering::GltfSceneImporter gltf_scene_importer;
	auto* gltf_scene = mesh_loader.get_scene();
	auto imported_scene = gltf_scene_importer.import(Rendering::GltfSceneImportContext{
		.mesh_loader       = mesh_loader,
		.texture_cache     = request.resources.textures(),
		.mesh_storage      = request.resources.meshes(),
		.material_registry = request.resources.materials(),
		.device            = request.device,
		.fallback_texture  = request.fallback_texture,
		.shader_rid        = request.shader_rid,
		.pbr_shader_rid    = request.pbr_shader_rid,
		.vertex_format     = request.vertex_format,
		.source_path       = request.path,
		.name_prefix       = request.name_prefix,
	});

	Util::SmallVector<entt::entity> node_entities;
	node_entities.resize(gltf_scene->nodes.size(), entt::null);
	for (int node_index = 0; node_index < static_cast<int>(gltf_scene->nodes.size()); node_index++) {
		const auto& node = gltf_scene->nodes[node_index];
		auto entity = request.world.create();
		node_entities[node_index] = entity;

		request.world.emplace<LocalTransform>(entity, LocalTransform{
			.position = node.translation,
			.rotation = node.rotation,
			.scale    = node.scale,
		});
		request.world.emplace<Children>(entity);
		handle.entities.push_back(entity);
	}

	for (int node_index = 0; node_index < static_cast<int>(gltf_scene->nodes.size()); node_index++) {
		const auto& node = gltf_scene->nodes[node_index];
		auto node_entity = node_entities[node_index];
		for (int child_index : node.children) {
			auto child_entity = node_entities[child_index];
			request.world.emplace<Parent>(child_entity, Parent{ .entity = node_entity });
			request.world.get<Children>(node_entity).entities.push_back(child_entity);
		}
	}

	for (const auto& primitive : imported_scene.primitives) {
		auto entity = request.world.create();
		request.world.emplace<LocalTransform>(entity);
		if (primitive.node_index >= 0 && primitive.node_index < static_cast<int>(node_entities.size())) {
			auto node_entity = node_entities[primitive.node_index];
			request.world.emplace<Parent>(entity, Parent{ .entity = node_entity });
			request.world.get<Children>(node_entity).entities.push_back(entity);
		}
		Util::SmallVector<SceneMaterialAssetHandle> materials;
		for (auto material : primitive.materials)
			materials.push_back(request.resources.assets().register_material(material));
		request.world.emplace<MeshComponent>(entity, MeshComponent{
			.mesh       = request.resources.assets().register_mesh(primitive.mesh),
			.materials  = std::move(materials),
			.local_aabb = primitive.local_aabb,
		});
		handle.entities.push_back(entity);
	}

	update_world_transforms(request.world);

	return handle;
}
