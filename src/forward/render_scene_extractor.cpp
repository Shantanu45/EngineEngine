#include "forward/render_scene_extractor.h"

#include "forward/scene/components.h"
#include "rendering/debug_draw.h"
#include "util/profiler.h"

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
			inst.category      = m.category;
			for (auto asset : m.materials) {
				auto h = input.asset_registry.resolve_material(asset);
				inst.material_sets.push_back(
					h != Rendering::INVALID_MATERIAL
						? input.material_registry.get_uniform_set(h)
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
			Light gl = l.data;
			gl.position = glm::vec4(glm::vec3(t.matrix[3]), l.data.position.w);
			view.lights.push_back(gl);
		});

	return result;
}
