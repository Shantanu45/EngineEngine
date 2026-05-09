#pragma once

#include <string>
#include <vector>

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "rendering/camera.h"
#include "rendering/drawable.h"
#include "rendering/texture_cache.h"
#include "rendering/uniform_buffer.h"
#include "rendering/wsi.h"
#include "shader_playground/playground_bindings.h"

namespace ShaderPlayground {

struct PlaygroundFrameUBO {
	glm::vec2 resolution = glm::vec2(1.0f);
	float time = 0.0f;
	float delta_time = 0.0f;
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 proj = glm::mat4(1.0f);
	glm::vec4 mouse = glm::vec4(0.0f);
};

struct PlaygroundObjectPC {
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 normal_matrix = glm::mat4(1.0f);
};

enum class PlaygroundMesh {
	Fullscreen,
	Quad,
	Plane,
	Cube,
	Sphere,
};

class PlaygroundObject {
public:
	PlaygroundObject& texture(uint32_t binding, const std::string& path, bool srgb = true);
	PlaygroundObject& texture_linear(uint32_t binding, const std::string& path);

	template <typename T>
	PlaygroundObject& ubo(uint32_t binding, const T& data, const char* debug_name = "Playground user UBO")
	{
		bindings.ubo(binding, data, debug_name);
		return *this;
	}

	PlaygroundObject& position(glm::vec3 value);
	PlaygroundObject& rotation(glm::vec3 degrees);
	PlaygroundObject& scale(glm::vec3 value);

private:
	friend class PlaygroundScene;

	Rendering::Pipeline pipeline;
	Rendering::MeshHandle mesh = Rendering::INVALID_MESH;
	PlaygroundMesh mesh_type = PlaygroundMesh::Quad;
	PlaygroundBindings bindings;
	RIDHandle frame_uniform_set;
	glm::vec3 local_position = glm::vec3(0.0f);
	glm::vec3 local_rotation_degrees = glm::vec3(0.0f);
	glm::vec3 local_scale = glm::vec3(1.0f);
	bool uses_object_push_constants = true;
};

class PlaygroundScene {
public:
	bool initialize(Rendering::WSI* wsi, FileSystem::Filesystem& filesystem);
	void shutdown();
	void clear();

	PlaygroundObject& fullscreen(const std::string& fragment_shader);
	PlaygroundObject& quad(const std::string& vertex_shader, const std::string& fragment_shader);
	PlaygroundObject& plane(const std::string& vertex_shader, const std::string& fragment_shader);
	PlaygroundObject& cube(const std::string& vertex_shader, const std::string& fragment_shader);
	PlaygroundObject& sphere(const std::string& vertex_shader, const std::string& fragment_shader);

	void camera(glm::vec3 position, glm::vec3 euler_degrees, float fov_degrees = 60.0f);
	void render(double frame_time, double elapsed_time);

private:
	PlaygroundObject& add_object(
		PlaygroundMesh mesh_type,
		const std::string& vertex_shader,
		const std::string& fragment_shader,
		bool uses_object_push_constants);

	void configure_wsi();
	void create_frame_resources();
	void create_builtin_meshes();
	void create_scene_pass(FrameGraph& fg, FrameGraphBlackboard& bb, Size2i extent, const std::vector<Rendering::Drawable>& drawables);
	Rendering::PushConstantData object_push_constants(const PlaygroundObject& object) const;

	Rendering::WSI* wsi = nullptr;
	Rendering::RenderingDevice* device = nullptr;

	Camera scene_camera;
	Rendering::TextureCache textures;
	Rendering::MeshStorage mesh_storage;
	Rendering::UniformBuffer<PlaygroundFrameUBO> frame_ubo;
	RIDHandle sampler;
	RIDHandle missing_texture;
	Rendering::RenderingDevice::FramebufferFormatID framebuffer_format = Rendering::RenderingDevice::INVALID_FORMAT_ID;

	Rendering::MeshHandle fullscreen_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle quad_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle plane_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle cube_mesh = Rendering::INVALID_MESH;
	Rendering::MeshHandle sphere_mesh = Rendering::INVALID_MESH;

	std::vector<PlaygroundObject> objects;

	FrameGraph fg;
	FrameGraphBlackboard bb;
};

} // namespace ShaderPlayground
