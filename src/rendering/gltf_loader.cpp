
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "gltf_loader.h"

namespace Rendering
{

	GltfLoader::GltfLoader(::FileSystem::FilesystemInterface& iface) : fs_iface(iface)
	{

	}

	Error GltfLoader::load(const std::string& path)
	{
		tinygltf::TinyGLTF loader;
		std::string err, warn;

		const auto actual_path = fs_iface.get_filesystem_path(path);
		bool is_glb = (FileSystem::Path::ext(actual_path) == "glb");
		bool result = is_glb ? loader.LoadBinaryFromFile(&m_model, &err, &warn, actual_path) : loader.LoadASCIIFromFile(&m_model, &err, &warn, actual_path);


		if (!result)
		{
			LOGE("[gltf]: %s\n", err.c_str());
			return ERR_FILE_NOT_FOUND;
		}

		if (!warn.empty())
		{
			LOGW("[gltf]: %s\n", warn.c_str());
		}

		for (auto& mesh : m_model.meshes)
			for (auto& prim : mesh.primitives)
				m_primitives.push_back(extract_primitive(prim));

		return OK;
	}

	Rendering::MeshPrimitive GltfLoader::extract_primitive(const tinygltf::Primitive& prim)
	{
		MeshPrimitive out;

		auto& posAcc = accessor(prim, "POSITION");
		out.vertices.resize(posAcc.count);

		copy_attrib(prim, "POSITION", out.vertices, offsetof(Vertex, position), 3);
		copy_attrib(prim, "NORMAL", out.vertices, offsetof(Vertex, normal), 3);
		copy_attrib(prim, "TEXCOORD_0", out.vertices, offsetof(Vertex, texcoord), 2);
		copy_attrib(prim, "TANGENT", out.vertices, offsetof(Vertex, tangent), 4);

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

	void GltfLoader::copy_attrib(const tinygltf::Primitive& prim, const std::string& semantic, std::vector<Vertex>& verts, size_t byteOffset, int numComponents)
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

	const tinygltf::Accessor& GltfLoader::accessor(const tinygltf::Primitive& prim, const std::string& semantic)
	{
		auto it = prim.attributes.find(semantic);
		if (it == prim.attributes.end())
			throw std::runtime_error("Missing required attribute: " + semantic);
		return m_model.accessors[it->second];
	}

}
