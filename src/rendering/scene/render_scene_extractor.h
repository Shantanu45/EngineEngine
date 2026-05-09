#pragma once

#include "entt/entt.hpp"
#include "math/math_common.h"
#include "rendering/camera.h"
#include "rendering/render_asset_registry.h"
#include "rendering/rendering_device.h"
#include "rendering/uniform_set_builder.h"
#include "rendering/material.h"
#include "rendering/mesh_storage.h"
#include "rendering/render_settings.h"
#include "rendering/render_stats.h"
#include "rendering/renderers/renderer.h"

struct RenderSceneExtractInput {
	entt::registry& world;
	const Camera& camera;
	const RenderSettings& settings;
	const Rendering::RenderAssetRegistry& asset_registry;
	Rendering::MaterialRegistry& material_registry;
	Rendering::RenderingDevice* device = nullptr;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Size2i extent;
	double elapsed = 0.0;
};

struct RenderSceneExtractResult {
	Rendering::SceneView view;
	Rendering::RenderStats stats;
};

class RenderSceneExtractor {
public:
	RenderSceneExtractResult extract(RenderSceneExtractInput input) const;
};
