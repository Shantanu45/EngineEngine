// scene/components.h
#pragma once
#include "entt/entt.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/scene/scene_asset_handles.h"
#include "rendering/aabb.h"
#include "util/small_vector.h"

struct LocalTransform {
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 scale    = glm::vec3(1.0f);

	glm::mat4 get_matrix() const {
		return glm::translate(glm::mat4(1.0f), position)
			* glm::mat4_cast(rotation)
			* glm::scale(glm::mat4(1.0f), scale);
	}
};

struct WorldTransform {
	glm::mat4 matrix = glm::mat4(1.0f);

	glm::mat4 get_normal_matrix() const {
		return glm::transpose(glm::inverse(matrix));
	}
};

struct Parent {
	entt::entity entity = entt::null;
};

struct Children {
	Util::SmallVector<entt::entity> entities;
};

enum class SceneMeshCategory {
	Opaque,
	LightVisualization,
};

enum class SceneLightType {
	Directional,
	Point,
	Spot,
};

struct MeshComponent {
	SceneMeshAssetHandle                   mesh;
	Util::SmallVector<SceneMaterialAssetHandle> materials;
	SceneMeshCategory                       category   = SceneMeshCategory::Opaque;
	Rendering::AABB                        local_aabb;  // local-space; invalid by default (valid() == false)
};

struct LightComponent {
	SceneLightType type = SceneLightType::Point;
	glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float range = 15.0f;
	float intensity = 1.0f;
	float inner_angle = 0.0f;
	float outer_angle = 0.0f;
};
