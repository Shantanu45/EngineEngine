// scene/components.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/mesh_storage.h"
#include "rendering/rendering_device.h"
#include "rendering/light.h"

struct MaterialData {
	RID uniform_set;
	RID diffuse_texture;
	RID specular_texture;   // or metallic/roughness for PBR
	RID normal_texture;
	// add whatever channels your shader needs
};

//struct alignas(16) PointLight_UBO {
//	glm::vec3 position;
//	float     constant;
//
//	glm::vec3 ambient;
//	float     linear;
//
//	glm::vec3 diffuse;
//	float     quadratic;
//
//	glm::vec3 specular;
//	float     _pad;
//};


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
	Rendering::MeshHandle mesh;
	RID                   pipeline;
	const char* shader;
	RID                   uniform_set;
	MaterialData          material;
};

struct LightComponent {
	Light data;
};

struct MaterialComponent {
	float shininess = 32.0f;
};