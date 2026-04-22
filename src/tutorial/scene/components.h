// scene/components.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/mesh_storage.h"
#include "rendering/rendering_device.h"
#include "rendering/pipeline_builder.h"
#include "rendering/light.h"
#include "rendering/material.h"

struct TransformComponent {
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f); // euler angles in degrees
	glm::vec3 scale = glm::vec3(1.0f);

	glm::mat4 get_model() const {
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
	Rendering::Pipeline                    pipeline;
	RID                                    uniform_set_0 = {};
	std::vector<Rendering::MaterialHandle> materials; // one per primitive slot
};

struct LightComponent {
	Light data;
};