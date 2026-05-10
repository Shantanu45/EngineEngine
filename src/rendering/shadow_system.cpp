#include "rendering/shadow_system.h"

#include <glm/gtc/matrix_transform.hpp>

#define SHADOW_MAP_CASCADE_COUNT 4

namespace Rendering {
namespace {

uint32_t to_light_type(LightType type)
{
	return static_cast<uint32_t>(type);
}

Util::SmallVector<RID> shadow_material_sets_for_projection(
	const ShadowCasterInstance& instance,
	ShadowProjection projection)
{
	switch (projection) {
	case ShadowProjection::Directional2D:
		return instance.shadow_material_sets;
	case ShadowProjection::PointCube:
		return instance.point_shadow_material_sets;
	}
	return {};
}

} // namespace

Rendering::ShadowBuildResult ShadowSystem::build_shadow_buffer(
	const Util::SmallVector<Light>& lights,
	const CameraData& camera,
	DirectionalShadowMode directional_shadow_mode)
{
	constexpr float point_shadow_near = 0.1f;

	ShadowBuildResult result{};

	for (const auto& light : lights) {
		if (result.buffer.count >= MAX_LIGHTS) {
			break;
		}

		ShadowData& shadow = result.buffer.shadows[result.buffer.count];

		if (light.type == to_light_type(LightType::Directional)) {
			result.directional_shadow_index = result.buffer.count;
			const glm::vec3 position = glm::vec3(light.position);
			if (directional_shadow_mode == DirectionalShadowMode::Cascaded) {
				shadow = _update_cascades(camera, glm::vec3(light.direction));
			}
			else
			{
				const glm::mat4 projection = glm::orthoRH_ZO(-8.0f, 8.0f, -8.0f, 8.0f, 1.0f, 30.0f);
				const glm::mat4 view = glm::lookAt(position, position + glm::vec3(light.direction), glm::vec3(0, 1, 0));
				shadow.matrices[0] = projection * view;
			}
		}
		else if (light.type == to_light_type(LightType::Point)) {
			result.point_shadow_index = result.buffer.count;
			const glm::vec3 position = glm::vec3(light.position);
			const float far_plane = light.position.w;
			const glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, point_shadow_near, far_plane);
			shadow.matrices[0] = projection * glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
			shadow.matrices[1] = projection * glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
			shadow.matrices[2] = projection * glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
			shadow.matrices[3] = projection * glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
			shadow.matrices[4] = projection * glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
			shadow.matrices[5] = projection * glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
			shadow.light_pos = glm::vec4(position, far_plane);
		}

		shadow.light_index = result.buffer.count;
		result.buffer.count++;
	}

	return result;
}

std::vector<Drawable> ShadowSystem::build_shadow_drawables(
	const SceneView& view,
	const ShadowShaderBinding& binding)
{
	std::vector<Drawable> drawables;
	drawables.reserve(view.shadow_casters.size());

	for (const auto& instance : view.shadow_casters) {
		if (instance.category != MeshCategory::Opaque) {
			continue;
		}

		drawables.push_back(Drawable::make(
			binding.pipeline,
			instance.mesh,
			PushConstantData::from(ObjectData_UBO{ instance.model, instance.normal_matrix }),
			{ { binding.uniform_set_0, 0 } },
			shadow_material_sets_for_projection(instance, binding.projection)));
	}

	sort_drawables_for_state_reuse(drawables);
	return drawables;
}


ShadowData ShadowSystem::_update_cascades(const CameraData& camera, const glm::vec3& light_dir)
{
	constexpr float cascade_split_lambda = 0.95f;
	ShadowData shadow{};
	float cascade_splits[SHADOW_MAP_CASCADE_COUNT];

	float near_clip = camera.near_clip;
	float far_clip = camera.far_clip;
	float clip_range = far_clip - near_clip;

	float min_z = near_clip;
	float max_z = near_clip + clip_range;

	float range = max_z - min_z;
	float ratio = max_z / min_z;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = min_z * std::pow(ratio, p);
		float uniform = min_z + range * p;
		float d = cascade_split_lambda * (log - uniform) + uniform;
		cascade_splits[i] = (d - near_clip) / clip_range;
	}

	// Calculate orthographic projection matrix for each cascade
	float last_split_dist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {

		float split_dist = cascade_splits[i];

		glm::vec3 frustum_corners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 inv_cam = glm::inverse(camera.proj * camera.view);
		for (uint32_t j = 0; j < 8; j++) {
			glm::vec4 invCorner = inv_cam * glm::vec4(frustum_corners[j], 1.0f);
			frustum_corners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++) {
			glm::vec3 dist = frustum_corners[j + 4] - frustum_corners[j];
			frustum_corners[j + 4] = frustum_corners[j] + (dist * split_dist);
			frustum_corners[j] = frustum_corners[j] + (dist * last_split_dist);
		}

		// Get frustum center
		glm::vec3 frustum_center = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++) {
			frustum_center += frustum_corners[j];
		}
		frustum_center /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++) {
			float distance = glm::length(frustum_corners[j] - frustum_center);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 max_extents = glm::vec3(radius);
		glm::vec3 min_extents = -max_extents;

		const glm::vec3 light_direction = glm::normalize(-light_dir);
		glm::mat4 light_view_matrix = glm::lookAt(frustum_center - light_direction * -min_extents.z, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 light_ortho_matrix = glm::orthoRH_ZO(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);

		// Store split distance and matrix in cascade
		shadow.cascade_splits[i] = (camera.near_clip + split_dist * clip_range);/* * -1.0f;*/
		shadow.matrices[i] = light_ortho_matrix * light_view_matrix;

		last_split_dist = cascade_splits[i];
	}
	return shadow;
}

} // namespace Rendering
