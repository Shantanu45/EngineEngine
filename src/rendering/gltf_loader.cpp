// gltf_loader.cpp

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "gltf_loader.h"

namespace Rendering
{
	GltfLoader::GltfLoader(::FileSystem::FilesystemInterface& iface) : fs_iface(iface) {}

	Error GltfLoader::load(const std::string& path)
	{
		tinygltf::TinyGLTF loader;
		std::string err, warn;

		const auto actual_path = fs_iface.get_filesystem_path(path);
		bool is_glb = (FileSystem::Path::ext(actual_path) == "glb");
		bool result = is_glb
			? loader.LoadBinaryFromFile(&m_model, &err, &warn, actual_path)
			: loader.LoadASCIIFromFile(&m_model, &err, &warn, actual_path);

		if (!result) {
			LOGE("[gltf]: %s\n", err.c_str());
			return ERR_FILE_NOT_FOUND;
		}
		if (!warn.empty())
			LOGW("[gltf]: %s\n", warn.c_str());

		// Images
		for (auto& img : m_model.images)
			m_scene.images.push_back(_extract_image(img));

		// Samplers
		for (auto& smp : m_model.samplers)
			m_scene.samplers.push_back(_extract_sampler(smp));

		// Materials
		for (auto& mat : m_model.materials)
			m_scene.materials.push_back(_extract_material(mat));

		// Meshes
		for (auto& mesh : m_model.meshes)
			m_scene.meshes.push_back(_extract_mesh(mesh));

		// Nodes
		for (int i = 0; i < (int)m_model.nodes.size(); ++i)
			m_scene.nodes.push_back(_extract_node(i));

		// Scenes
		for (auto& s : m_model.scenes) {
			Scene sc;
			sc.name = s.name;
			sc.root_nodes = s.nodes;
			m_scene.scenes.push_back(sc);
		}
		m_scene.default_scene = m_model.defaultScene >= 0 ? m_model.defaultScene : 0;

		// Animations
		for (auto& anim : m_model.animations)
			m_scene.animations.push_back(_extract_animation(anim));

		return OK;
	}

	// ----------------------------------------------------------------
	// Mesh / primitive — same core logic, now sets material_index
	// ----------------------------------------------------------------
	Mesh GltfLoader::_extract_mesh(const tinygltf::Mesh& mesh)
	{
		Mesh out;
		out.name = mesh.name;
		for (auto& prim : mesh.primitives)
			out.primitives.push_back(_extract_primitive(prim));
		return out;
	}

	MeshPrimitive GltfLoader::_extract_primitive(const tinygltf::Primitive& prim)
	{
		MeshPrimitive out;
		out.material_index = prim.material;

		auto& posAcc = accessor(prim, "POSITION");
		out.vertices.resize(posAcc.count);

		_copy_attrib(prim, "POSITION", out.vertices, offsetof(Vertex, position), 3);
		_copy_attrib(prim, "NORMAL", out.vertices, offsetof(Vertex, normal), 3);
		_copy_attrib(prim, "TEXCOORD_0", out.vertices, offsetof(Vertex, texcoord), 2);
		_copy_attrib(prim, "TANGENT", out.vertices, offsetof(Vertex, tangent), 4);

		if (prim.indices >= 0) {
			auto& idxAcc = m_model.accessors[prim.indices];
			out.indices.resize(idxAcc.count);
			const auto& bv = m_model.bufferViews[idxAcc.bufferView];
			const uint8_t* src = m_model.buffers[bv.buffer].data.data()
				+ bv.byteOffset + idxAcc.byteOffset;

			for (size_t i = 0; i < idxAcc.count; ++i) {
				switch (idxAcc.componentType) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					out.indices[i] = src[i]; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					out.indices[i] = reinterpret_cast<const uint16_t*>(src)[i]; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					out.indices[i] = reinterpret_cast<const uint32_t*>(src)[i]; break;
				default:
					throw std::runtime_error("Unsupported index component type");
				}
			}
		}
		else {
			out.indices.resize(out.vertices.size());
			for (size_t i = 0; i < out.vertices.size(); ++i)
				out.indices[i] = (uint32_t)i;
		}

		return out;
	}

	// ----------------------------------------------------------------
	// Materials
	// ----------------------------------------------------------------
	PBRMaterial GltfLoader::_extract_material(const tinygltf::Material& mat)
	{
		PBRMaterial out;
		out.name = mat.name;
		out.alpha_mode = mat.alphaMode;
		out.alpha_cutoff = (float)mat.alphaCutoff;
		out.double_sided = mat.doubleSided;

		auto& pbr = mat.pbrMetallicRoughness;

		out.base_color_factor = glm::vec4(
			pbr.baseColorFactor[0], pbr.baseColorFactor[1],
			pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
		out.metallic_factor = (float)pbr.metallicFactor;
		out.roughness_factor = (float)pbr.roughnessFactor;

		out.base_color_texture = _extract_texture_info(pbr.baseColorTexture.index, pbr.baseColorTexture.texCoord);
		out.metallic_roughness_texture = _extract_texture_info(pbr.metallicRoughnessTexture.index, pbr.metallicRoughnessTexture.texCoord);
		out.normal_texture = _extract_texture_info(mat.normalTexture.index, mat.normalTexture.texCoord);
		out.occlusion_texture = _extract_texture_info(mat.occlusionTexture.index, mat.occlusionTexture.texCoord);
		out.emissive_texture = _extract_texture_info(mat.emissiveTexture.index, mat.emissiveTexture.texCoord);

		out.normal_scale = (float)mat.normalTexture.scale;
		out.occlusion_strength = (float)mat.occlusionTexture.strength;
		out.emissive_factor = glm::vec3(
			mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2]);

		return out;
	}

	std::optional<TextureInfo> GltfLoader::_extract_texture_info(int texture_index, int tex_coord)
	{
		if (texture_index < 0)
			return std::nullopt;

		auto& tex = m_model.textures[texture_index];
		TextureInfo info;
		info.image_index = tex.source;
		info.sampler_index = tex.sampler;
		info.tex_coord = tex_coord;
		return info;
	}

	// ----------------------------------------------------------------
	// Images + Samplers
	// ----------------------------------------------------------------
	GltfImageData GltfLoader::_extract_image(const tinygltf::Image& img)
	{
		GltfImageData out;
		out.name = img.name;
		out.uri = img.uri;
		out.width = img.width;
		out.height = img.height;
		out.channels = img.component;
		out.pixels = img.image; // tinygltf already decoded it via stb_image
		return out;
	}

	SamplerInfo GltfLoader::_extract_sampler(const tinygltf::Sampler& smp)
	{
		SamplerInfo out;
		out.mag_filter = smp.magFilter;
		out.min_filter = smp.minFilter;
		out.wrap_s = smp.wrapS;
		out.wrap_t = smp.wrapT;
		return out;
	}

	// ----------------------------------------------------------------
	// Nodes
	// ----------------------------------------------------------------
	Node GltfLoader::_extract_node(int node_index)
	{
		auto& n = m_model.nodes[node_index];
		Node out;
		out.name = n.name;
		out.mesh_index = n.mesh;
		out.children = n.children;

		// Set parent index on children (we do it here since we have the index)
		for (int child : n.children)
			m_scene.nodes[child].parent_index = node_index;

		if (!n.matrix.empty()) {
			// glTF matrix is column-major, same as glm
			glm::mat4 m;
			for (int col = 0; col < 4; ++col)
				for (int row = 0; row < 4; ++row)
					m[col][row] = (float)n.matrix[col * 4 + row];
			// Decompose into TRS so get_local_transform() works uniformly
			out.translation = glm::vec3(m[3]);
			out.scale = glm::vec3(
				glm::length(glm::vec3(m[0])),
				glm::length(glm::vec3(m[1])),
				glm::length(glm::vec3(m[2])));
			glm::mat3 rot(
				glm::vec3(m[0]) / out.scale.x,
				glm::vec3(m[1]) / out.scale.y,
				glm::vec3(m[2]) / out.scale.z);
			out.rotation = glm::quat_cast(rot);
		}
		else {
			if (!n.translation.empty())
				out.translation = glm::vec3(n.translation[0], n.translation[1], n.translation[2]);
			if (!n.rotation.empty())
				out.rotation = glm::quat((float)n.rotation[3], (float)n.rotation[0],
					(float)n.rotation[1], (float)n.rotation[2]);
			if (!n.scale.empty())
				out.scale = glm::vec3(n.scale[0], n.scale[1], n.scale[2]);
		}

		return out;
	}

	glm::mat4 GltfLoader::get_world_transform(int node_index) const
	{
		glm::mat4 transform = m_scene.nodes[node_index].get_local_transform();
		int parent = m_scene.nodes[node_index].parent_index;
		while (parent >= 0) {
			transform = m_scene.nodes[parent].get_local_transform() * transform;
			parent = m_scene.nodes[parent].parent_index;
		}
		return transform;
	}

	// ----------------------------------------------------------------
	// Animations
	// ----------------------------------------------------------------
	Animation GltfLoader::_extract_animation(const tinygltf::Animation& anim)
	{
		Animation out;
		out.name = anim.name;

		// Samplers
		for (auto& s : anim.samplers) {
			AnimationSampler smp;

			// Interpolation
			if (s.interpolation == "STEP")        smp.interpolation = AnimationInterpolation::Step;
			else if (s.interpolation == "CUBICSPLINE") smp.interpolation = AnimationInterpolation::CubicSpline;
			else                                        smp.interpolation = AnimationInterpolation::Linear;

			// Input = timestamps
			auto& timeAcc = m_model.accessors[s.input];
			auto& timeBv = m_model.bufferViews[timeAcc.bufferView];
			const float* timeData = reinterpret_cast<const float*>(
				m_model.buffers[timeBv.buffer].data.data()
				+ timeBv.byteOffset + timeAcc.byteOffset);
			smp.times.assign(timeData, timeData + timeAcc.count);
			out.duration = std::max(out.duration, smp.times.back());

			// Output = values (vec3 or vec4)
			auto& valAcc = m_model.accessors[s.output];
			auto& valBv = m_model.bufferViews[valAcc.bufferView];
			const float* valData = reinterpret_cast<const float*>(
				m_model.buffers[valBv.buffer].data.data()
				+ valBv.byteOffset + valAcc.byteOffset);
			int components = (valAcc.type == TINYGLTF_TYPE_VEC4) ? 4 : 3;
			smp.values.resize(valAcc.count);
			for (size_t i = 0; i < valAcc.count; ++i) {
				smp.values[i] = glm::vec4(
					valData[i * components + 0],
					valData[i * components + 1],
					valData[i * components + 2],
					components == 4 ? valData[i * components + 3] : 0.0f);
			}

			out.samplers.push_back(std::move(smp));
		}

		// Channels
		for (auto& ch : anim.channels) {
			AnimationChannel channel;
			channel.target_node = ch.target_node;
			channel.sampler_index = ch.sampler;
			if (ch.target_path == "translation") channel.path = AnimationPath::Translation;
			else if (ch.target_path == "rotation")    channel.path = AnimationPath::Rotation;
			else if (ch.target_path == "scale")       channel.path = AnimationPath::Scale;
			else if (ch.target_path == "weights")     channel.path = AnimationPath::Weights;
			out.channels.push_back(channel);
		}

		return out;
	}

	// ----------------------------------------------------------------
	// Unchanged helpers
	// ----------------------------------------------------------------
	void GltfLoader::_copy_attrib(const tinygltf::Primitive& prim, const std::string& semantic,
		std::vector<Vertex>& verts, size_t byteOffset, int numComponents)
	{
		auto it = prim.attributes.find(semantic);
		if (it == prim.attributes.end()) return;
		auto& acc = m_model.accessors[it->second];
		auto& bv = m_model.bufferViews[acc.bufferView];
		const uint8_t* src = m_model.buffers[bv.buffer].data.data()
			+ bv.byteOffset + acc.byteOffset;
		size_t stride = bv.byteStride ? bv.byteStride : numComponents * sizeof(float);
		for (size_t i = 0; i < acc.count; ++i) {
			const float* in = reinterpret_cast<const float*>(src + i * stride);
			float* out = reinterpret_cast<float*>(
				reinterpret_cast<uint8_t*>(&verts[i]) + byteOffset);
			for (int c = 0; c < numComponents; ++c)
				out[c] = in[c];
		}
	}

	const tinygltf::Accessor& GltfLoader::accessor(const tinygltf::Primitive& prim,
		const std::string& semantic)
	{
		auto it = prim.attributes.find(semantic);
		if (it == prim.attributes.end())
			throw std::runtime_error("Missing required attribute: " + semantic);
		return m_model.accessors[it->second];
	}
}