// scene/components.h
#pragma once
#include "entt/entt.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/render_asset_registry.h"
#include "rendering/light.h"
#include "rendering/mesh_category.h"
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

struct MeshComponent {
	Rendering::MeshAssetHandle             mesh;
	Util::SmallVector<Rendering::MaterialAssetHandle> materials;
	Rendering::MeshCategory                category   = Rendering::MeshCategory::Opaque;
	Rendering::AABB                        local_aabb;  // local-space; invalid by default (valid() == false)
};

struct LightComponent {
	Light data;
};
