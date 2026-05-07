#include "forward/scene/transform_system.h"

#include "forward/scene/components.h"

namespace {

void update_subtree(entt::registry& world, entt::entity entity, const glm::mat4& parent_matrix)
{
	auto* local = world.try_get<LocalTransform>(entity);
	if (!local)
		return;

	glm::mat4 world_matrix = parent_matrix * local->get_matrix();
	world.emplace_or_replace<WorldTransform>(entity, WorldTransform{ .matrix = world_matrix });

	if (auto* children = world.try_get<Children>(entity)) {
		for (auto child : children->entities)
			update_subtree(world, child, world_matrix);
	}
}

} // namespace

void update_world_transforms(entt::registry& world)
{
	world.view<LocalTransform>(entt::exclude<Parent>).each(
		[&](auto entity, LocalTransform&) {
			update_subtree(world, entity, glm::mat4(1.0f));
		});

	world.view<LocalTransform>().each(
		[&](auto entity, LocalTransform&) {
			if (!world.all_of<WorldTransform>(entity))
				update_subtree(world, entity, glm::mat4(1.0f));
		});
}
