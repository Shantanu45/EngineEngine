#include "rendering/shadow_system.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Rendering {
namespace {

uint32_t to_light_type(LightType type)
{
	return static_cast<uint32_t>(type);
}

Util::SmallVector<RID> shadow_material_sets_for_projection(
	const MeshInstance& instance,
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

ShadowBuildResult ShadowSystem::build_shadow_buffer(const Util::SmallVector<Light>& lights)
{
	constexpr float point_shadow_near = 0.1f;

	ShadowBuildResult result{};

	for (const auto& light : lights) {
		if (result.buffer.count >= MAX_LIGHTS) {
			break;
		}

		ShadowData& shadow = result.buffer.shadows[result.buffer.count];
		shadow.light_index = result.buffer.count;

		if (light.type == to_light_type(LightType::Directional)) {
			result.directional_shadow_index = result.buffer.count;
			const glm::vec3 position = glm::vec3(light.position);
			const glm::mat4 projection = glm::orthoRH_ZO(-8.0f, 8.0f, -8.0f, 8.0f, 1.0f, 30.0f);
			const glm::mat4 view = glm::lookAt(position, position + glm::vec3(light.direction), glm::vec3(0, 1, 0));
			shadow.matrices[0] = projection * view;
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

		result.buffer.count++;
	}

	return result;
}

std::vector<Drawable> ShadowSystem::build_shadow_drawables(
	const SceneView& view,
	const ShadowShaderBinding& binding)
{
	std::vector<Drawable> drawables;
	drawables.reserve(view.instances.size());

	for (const auto& instance : view.instances) {
		if (instance.category != MeshCategory::Opaque || instance.transparent) {
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

} // namespace Rendering
