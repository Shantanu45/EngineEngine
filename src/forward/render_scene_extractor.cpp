#include "forward/render_scene_extractor.h"

#include "forward/scene/components.h"
#include "rendering/debug_draw.h"
#include "util/profiler.h"

#include <cstdint>

namespace {

Rendering::MeshCategory to_render_mesh_category(SceneMeshCategory category)
{
	switch (category) {
	case SceneMeshCategory::Opaque:
		return Rendering::MeshCategory::Opaque;
	case SceneMeshCategory::LightVisualization:
		return Rendering::MeshCategory::LightVisualization;
	}
	return Rendering::MeshCategory::Opaque;
}

uint32_t to_render_light_type(SceneLightType type)
{
	switch (type) {
	case SceneLightType::Directional:
		return static_cast<uint32_t>(LightType::Directional);
	case SceneLightType::Point:
		return static_cast<uint32_t>(LightType::Point);
	case SceneLightType::Spot:
		return static_cast<uint32_t>(LightType::Spot);
	}
	return static_cast<uint32_t>(LightType::Point);
}

Light to_render_light(const LightComponent& light, const glm::mat4& transform)
{
	return Light{
		.position    = glm::vec4(glm::vec3(transform[3]), light.range),
		.direction   = glm::vec4(light.direction, light.inner_angle),
		.color       = glm::vec4(light.color, light.intensity),
		.type        = to_render_light_type(light.type),
		.outer_angle = light.outer_angle,
	};
}

} // namespace

RenderSceneExtractResult RenderSceneExtractor::extract(RenderSceneExtractInput input) const
{
	ZoneScoped;
	input.material_registry.upload_dirty(input.device);

	RenderSceneExtractResult result;
	auto& view = result.view;
	view.camera.view = input.camera.get_view();
	view.camera.proj = input.camera.get_projection();
	view.camera.cameraPos = input.camera.get_position();
	view.elapsed     = input.elapsed;
	view.extent      = input.extent;
	view.use_pbr_lighting = input.settings.use_pbr_lighting;
	view.skybox_mesh = input.settings.draw_skybox ? input.skybox_mesh : Rendering::INVALID_MESH;
	view.grid_mesh   = input.settings.draw_grid ? input.grid_mesh : Rendering::INVALID_MESH;

	auto emit_mesh = [&](MeshComponent& m, const glm::mat4& model, const glm::mat4& normal_matrix) {
			if (m.local_aabb.valid()) {
				Rendering::AABB world_aabb = Rendering::transform_aabb(m.local_aabb, model);
				if (input.settings.draw_debug_aabbs)
					Rendering::DebugDraw::get().add_aabb(world_aabb.min, world_aabb.max);
				if (input.settings.frustum_culling &&
					!input.camera.is_aabb_visible(world_aabb.min, world_aabb.max))
					return false;
			}

			Rendering::MeshInstance inst;
			inst.mesh          = input.asset_registry.resolve_mesh(m.mesh);
			inst.model         = model;
			inst.normal_matrix = normal_matrix;
			inst.category      = to_render_mesh_category(m.category);
			for (auto asset : m.materials) {
				auto h = input.asset_registry.resolve_material(asset);
				inst.material_sets.push_back(
					h != Rendering::INVALID_MATERIAL
						? input.material_registry.get_uniform_set(h, input.settings.use_pbr_lighting)
						: RID());
				inst.shadow_material_sets.push_back(
					h != Rendering::INVALID_MATERIAL
						? input.material_registry.get_shadow_uniform_set(h)
						: RID());
				inst.point_shadow_material_sets.push_back(
					h != Rendering::INVALID_MATERIAL
						? input.material_registry.get_point_shadow_uniform_set(h)
						: RID());
			}
			view.instances.push_back(std::move(inst));
			result.stats.draw_count++;
			return true;
		};

	input.world.view<WorldTransform, MeshComponent>().each(
		[&](auto, WorldTransform& t, MeshComponent& m) {
			emit_mesh(m, t.matrix, t.get_normal_matrix());
		});

	input.world.view<WorldTransform, LightComponent>().each(
		[&](auto, WorldTransform& t, LightComponent& l) {
			view.lights.push_back(to_render_light(l, t.matrix));
		});

	return result;
}
