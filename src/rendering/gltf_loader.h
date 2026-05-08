// gltf_loader.h

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include "tiny_gltf.h"
#include "util/error_macros.h"
#include "filesystem/filesystem.h"
#include "filesystem/path_utils.h"
#include "math/math_common.h"
#include "util/small_vector.h"

namespace Rendering
{
	// ----------------------------------------------------------------
	// Geometry - same as before
	// ----------------------------------------------------------------
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
		glm::vec4 tangent;
	};

	struct MeshPrimitive {
		Util::SmallVector<Vertex>   vertices;
		Util::SmallVector<uint32_t> indices;
		int                   material_index = -1; // index into GltfScene::materials
	};

	struct Mesh {
		std::string                      name;
		Util::SmallVector<MeshPrimitive> primitives;
	};

	// ----------------------------------------------------------------
	// Materials + Textures
	// ----------------------------------------------------------------
	struct TextureInfo {
		int   image_index = -1; // index into GltfScene::images
		int   sampler_index = -1; // index into GltfScene::samplers
		int   tex_coord = 0;  // which TEXCOORD_N to use
	};

	struct PBRMaterial {
		std::string  name;

		// Base color
		glm::vec4    base_color_factor = glm::vec4(1.0f);
		std::optional<TextureInfo> base_color_texture;

		// Metallic/roughness
		float        metallic_factor = 1.0f;
		float        roughness_factor = 1.0f;
		std::optional<TextureInfo> metallic_roughness_texture;

		// Normal map
		std::optional<TextureInfo> normal_texture;
		float        normal_scale = 1.0f;

		// Occlusion
		std::optional<TextureInfo> occlusion_texture;
		float        occlusion_strength = 1.0f;

		// Emissive
		glm::vec3    emissive_factor = glm::vec3(0.0f);
		std::optional<TextureInfo> emissive_texture;

		// Alpha
		std::string  alpha_mode = "OPAQUE"; // OPAQUE, MASK, BLEND
		float        alpha_cutoff = 0.5f;
		bool         double_sided = false;
	};

	struct SamplerInfo {
		int mag_filter = -1;  // TINYGLTF_TEXTURE_FILTER_*
		int min_filter = -1;
		int wrap_s = TINYGLTF_TEXTURE_WRAP_REPEAT;
		int wrap_t = TINYGLTF_TEXTURE_WRAP_REPEAT;
	};

	struct GltfImageData {
		std::string            name;
		std::string            uri;
		int                    width = 0;
		int                    height = 0;
		int                    channels = 0;
		Util::SmallVector<uint8_t>   pixels;
	};

	// ----------------------------------------------------------------
	// Scene hierarchy
	// ----------------------------------------------------------------
	struct Node {
		std::string        name;
		int                mesh_index = -1;  // -1 = no mesh
		int                parent_index = -1;  // -1 = root
		Util::SmallVector<int>   children;

		// Local transform - glTF gives either TRS or a matrix
		glm::vec3          translation = glm::vec3(0.0f);
		glm::quat          rotation = glm::quat(1, 0, 0, 0);
		glm::vec3          scale = glm::vec3(1.0f);
		glm::mat4          local_matrix = glm::mat4(1.0f); // pre-multiplied if matrix was provided

		glm::mat4 get_local_transform() const {
			return glm::translate(glm::mat4(1.0f), translation)
				* glm::mat4_cast(rotation)
				* glm::scale(glm::mat4(1.0f), scale);
		}
	};

	struct Scene {
		std::string           name;
		Util::SmallVector<int> root_nodes;
	};

	// ----------------------------------------------------------------
	// Animation
	// ----------------------------------------------------------------
	enum class AnimationPath { Translation, Rotation, Scale, Weights };
	enum class AnimationInterpolation { Linear, Step, CubicSpline };

	struct AnimationSampler {
		Util::SmallVector<float>      times;      // keyframe timestamps
		Util::SmallVector<glm::vec4>  values;     // vec3 or quat packed into vec4
		AnimationInterpolation  interpolation = AnimationInterpolation::Linear;
	};

	struct AnimationChannel {
		int           target_node = -1;
		AnimationPath path = AnimationPath::Translation;
		int           sampler_index = -1;
	};

	struct Animation {
		std::string                        name;
		Util::SmallVector<AnimationSampler> samplers;
		Util::SmallVector<AnimationChannel> channels;
		float                         duration = 0.0f; // max timestamp across all samplers
	};

	// ----------------------------------------------------------------
	// Top-level result
	// ----------------------------------------------------------------
	struct GltfScene {
		Util::SmallVector<Mesh>          meshes;
		Util::SmallVector<PBRMaterial>   materials;
		Util::SmallVector<GltfImageData> images;
		Util::SmallVector<SamplerInfo>   samplers;
		Util::SmallVector<Node>          nodes;
		Util::SmallVector<Scene>         scenes;
		Util::SmallVector<Animation>     animations;
		int                      default_scene = 0;
	};

	// ----------------------------------------------------------------
	// Loader
	// ----------------------------------------------------------------
	class GltfLoader {
	public:
		GltfLoader(::FileSystem::FilesystemInterface& iface);

		Error           load(const std::string& path);
		GltfScene&       scene()       { return m_scene; }
		const GltfScene& scene() const { return m_scene; }

		// Compute world-space transform for a node by walking up the hierarchy
		glm::mat4 get_world_transform(int node_index) const;

	private:
		tinygltf::Model              m_model;
		GltfScene                    m_scene;
		FileSystem::FilesystemInterface& fs_iface;

		// Extraction helpers
		Mesh         _extract_mesh(const tinygltf::Mesh& mesh);
		MeshPrimitive _extract_primitive(const tinygltf::Primitive& prim);
		void         _generate_tangents(MeshPrimitive& primitive);
		PBRMaterial  _extract_material(const tinygltf::Material& mat);
		GltfImageData    _extract_image(const tinygltf::Image& img);
		SamplerInfo  _extract_sampler(const tinygltf::Sampler& smp);
		Node         _extract_node(int node_index);
		Animation    _extract_animation(const tinygltf::Animation& anim);

		std::optional<TextureInfo> _extract_texture_info(int texture_index, int tex_coord = 0);

		void _copy_attrib(const tinygltf::Primitive& prim,
			const std::string& semantic,
			Util::SmallVector<Vertex>& verts,
			size_t byteOffset,
			int numComponents);

		const tinygltf::Accessor& accessor(const tinygltf::Primitive& prim,
			const std::string& semantic);
	};
}
