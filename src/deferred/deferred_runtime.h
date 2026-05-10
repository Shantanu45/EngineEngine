#pragma once

#include <array>
#include <memory>
#include <string>

#include "entt/entt.hpp"
#include "filesystem/filesystem.h"
#include "rendering/scene/render_scene_extractor.h"
#include "rendering/scene/scene_asset_handles.h"
#include "deferred/ui_layer.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/render_resource_store.h"
#include "rendering/render_settings.h"
#include "rendering/render_stats.h"
#include "rendering/renderers/deferred_render_pipeline.h"			// TODO
#include "rendering/rid_handle.h"
#include "rendering/wsi.h"

struct DeferredCameraConfig {
	float fov_degrees = 60.0f;
	float aspect = 16.0f / 9.0f;
	float near_plane = 0.1f;
	float far_plane = 1000.0f;
	CameraMode mode = CameraMode::Fly;
	bool reset_aspect_on_resize = true;
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 euler_degrees = glm::vec3(0.0f);
};

struct DeferredRuntimeConfig {
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
	DeferredCameraConfig camera;
	bool enable_default_lights = true;
	bool enable_default_ui = true;
	bool use_pbr_lighting = true;
};

class DeferredRuntime {

public:
	bool initialize(const DeferredRuntimeConfig& config);
	void render_frame(double frame_time, double elapsed_time);
	void shutdown();

private:
	void configure_camera(const DeferredCameraConfig& camera_config);
	void configure_wsi();
	void create_default_resources(FileSystem::Filesystem& filesystem, const std::array<std::string, 6>& skybox_faces);
	void load_scene(FileSystem::Filesystem& filesystem, const std::string& path, const std::string& name_prefix, bool add_default_lights);
	void register_default_ui();

	Rendering::RenderResourceStore resources;

	entt::registry world;

	Rendering::MeshHandle light_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle point_light_mesh = Rendering::INVALID_MESH;
	// Grid and skybox are render-pass helpers, not ECS scene objects, so they stay as renderer handles.
	Rendering::MeshHandle grid_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle skybox_mesh = Rendering::INVALID_MESH;
	SceneMeshAssetHandle light_mesh_asset = INVALID_SCENE_MESH_ASSET;
	SceneMeshAssetHandle point_light_mesh_asset = INVALID_SCENE_MESH_ASSET;

	Camera camera;
	Camera debug_camera;
	std::shared_ptr<EE::InputSystemInterface> input_system;

	Rendering::WSI* wsi = nullptr;
	Rendering::RenderingDevice* device = nullptr;

	RenderSettings render_settings;
	Rendering::RenderStats render_stats;
	bool render_imgui = true;
	RenderSceneExtractor scene_extractor;
	UIContext ui_ctx;
	UILayer ui_layer;

	// Render pipeline declared last, destroyed first (uniform sets reference texture RIDs above).
	Rendering::DeferredRenderPipeline render_pipeline;

};
