#pragma once

#include <array>
#include <memory>
#include <string>

#include "entt/entt.hpp"
#include "filesystem/filesystem.h"
#include "forward/render_scene_extractor.h"
#include "forward/ui_layer.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/render_resource_store.h"
#include "rendering/render_settings.h"
#include "rendering/render_stats.h"
#include "rendering/renderers/forward_render_pipeline.h"
#include "rendering/rid_handle.h"
#include "rendering/wsi.h"

struct ForwardRuntimeConfig {
	Rendering::WSI* wsi = nullptr;
	FileSystem::Filesystem& filesystem;
	std::shared_ptr<EE::InputSystemInterface> input_system;
	std::string scene_path = "assets://gltf/Sponza/glTF/Sponza.gltf";
	std::string scene_name_prefix = "sponza";
	std::array<std::string, 6> skybox_faces = {
		"assets://textures/skybox/right.jpg",
		"assets://textures/skybox/left.jpg",
		"assets://textures/skybox/top.jpg",
		"assets://textures/skybox/bottom.jpg",
		"assets://textures/skybox/front.jpg",
		"assets://textures/skybox/back.jpg",
	};
};

class ForwardRuntime {
public:
	bool initialize(const ForwardRuntimeConfig& config);
	void render_frame(double frame_time, double elapsed_time);
	void shutdown();

private:
	void configure_camera();
	void configure_wsi();
	void create_default_resources(FileSystem::Filesystem& filesystem, const std::array<std::string, 6>& skybox_faces);
	void load_scene(FileSystem::Filesystem& filesystem, const std::string& path, const std::string& name_prefix);
	void register_default_ui();

	RIDHandle cubemap_uniform;
	Rendering::RenderResourceStore resources;

	entt::registry world;

	Rendering::MeshHandle light_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle point_light_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;

	Camera camera;
	std::shared_ptr<EE::InputSystemInterface> input_system;

	Rendering::WSI* wsi = nullptr;
	Rendering::RenderingDevice* device = nullptr;

	RenderSettings render_settings;
	Rendering::RenderStats render_stats;
	RenderSceneExtractor scene_extractor;
	UIContext ui_ctx;
	UILayer ui_layer;

	// Render pipeline declared last, destroyed first (uniform sets reference texture RIDs above).
	Rendering::ForwardRenderPipeline render_pipeline;
};
