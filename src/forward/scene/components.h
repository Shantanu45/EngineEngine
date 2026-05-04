// scene/components.h
#pragma once
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/mesh_storage.h"
#include "rendering/rendering_device.h"
#include "rendering/light.h"
#include "rendering/material.h"
#include "rendering/mesh_category.h"
#include "rendering/aabb.h"

struct TransformComponent {
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f); // euler angles in degrees
	glm::vec3 scale    = glm::vec3(1.0f);
	std::optional<glm::mat4> matrix_override; // set when loading from GLTF node hierarchy

	glm::mat4 get_model() const {
		if (matrix_override) return *matrix_override;
		glm::mat4 m = glm::mat4(1.0f);
		m = glm::translate(m, position);
		m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
		m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
		m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
		m = glm::scale(m, scale);
		return m;
	}

	glm::mat4 get_normal_matrix() const {
		return glm::transpose(glm::inverse(get_model()));
	}
};

struct MeshComponent {
	Rendering::MeshHandle                  mesh;
	std::vector<Rendering::MaterialHandle> materials;
	Rendering::MeshCategory                category   = Rendering::MeshCategory::Opaque;
	Rendering::AABB                        local_aabb;  // local-space; invalid by default (valid() == false)
};

struct LightComponent {
	Light data;
};
