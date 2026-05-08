#include "forward/scene/transform_system.h"

#include "forward/scene/components.h"
#include "util/logger.h"

#include <cstdint>
#include <unordered_map>

namespace {

enum class VisitState : uint8_t {
	Visiting,
	Visited,
};

struct TransformUpdateContext {
	std::unordered_map<entt::entity, VisitState> states;
	bool reported_cycle = false;
	bool reported_orphan = false;
};

glm::mat4 update_entity_transform(entt::registry& world, entt::entity entity, TransformUpdateContext& context)
{
	if (auto state = context.states.find(entity); state != context.states.end()) {
		if (state->second == VisitState::Visited) {
			if (auto* world_transform = world.try_get<WorldTransform>(entity))
				return world_transform->matrix;
			if (auto* local = world.try_get<LocalTransform>(entity))
				return local->get_matrix();
			return glm::mat4(1.0f);
		}

		if (!context.reported_cycle) {
			LOGW("Transform hierarchy cycle detected; detaching one parent link to keep transforms valid.");
			context.reported_cycle = true;
		}
		world.remove<Parent>(entity);

		auto* local = world.try_get<LocalTransform>(entity);
		if (!local)
			return glm::mat4(1.0f);

		glm::mat4 world_matrix = local->get_matrix();
		world.emplace_or_replace<WorldTransform>(entity, WorldTransform{ .matrix = world_matrix });
		state->second = VisitState::Visited;
		return world_matrix;
	}

	auto* local = world.try_get<LocalTransform>(entity);
	if (!local)
		return glm::mat4(1.0f);

	context.states[entity] = VisitState::Visiting;

	glm::mat4 parent_matrix(1.0f);
	if (auto* parent = world.try_get<Parent>(entity)) {
		if (parent->entity != entt::null &&
			world.valid(parent->entity) &&
			world.all_of<LocalTransform>(parent->entity)) {
			parent_matrix = update_entity_transform(world, parent->entity, context);
			if (context.states[entity] == VisitState::Visited) {
				return world.get<WorldTransform>(entity).matrix;
			}
		}
		else {
			if (!context.reported_orphan) {
				LOGW("Transform hierarchy contains an invalid parent; detaching orphaned children.");
				context.reported_orphan = true;
			}
			world.remove<Parent>(entity);
		}
	}

	glm::mat4 world_matrix = parent_matrix * local->get_matrix();
	world.emplace_or_replace<WorldTransform>(entity, WorldTransform{ .matrix = world_matrix });
	context.states[entity] = VisitState::Visited;
	return world_matrix;
}

void prune_child_lists(entt::registry& world)
{
	world.view<Children>().each(
		[&](auto entity, Children& children) {
			for (auto it = children.entities.begin(); it != children.entities.end();) {
				entt::entity child = *it;
				if (child == entt::null ||
					!world.valid(child) ||
					!world.all_of<LocalTransform>(child)) {
					it = children.entities.erase(it);
					continue;
				}

				auto* parent = world.try_get<Parent>(child);
				if (!parent || parent->entity != entity) {
					it = children.entities.erase(it);
				}
				else {
					++it;
				}
			}
		});
}

} // namespace

void update_world_transforms(entt::registry& world)
{
	prune_child_lists(world);

	TransformUpdateContext context;

	world.view<LocalTransform>().each(
		[&](auto entity, LocalTransform&) {
			update_entity_transform(world, entity, context);
		});
}
