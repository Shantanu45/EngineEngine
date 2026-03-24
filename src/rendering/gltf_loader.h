// gltf_loader.h
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "tiny_gltf.h"
#include "util/error_macros.h"
#include "filesystem/filesystem.h"
#include "filesystem/path_utils.h"

namespace Rendering
{

	struct Vertex {
		float position[3];
		float normal[3];
		float texcoord[2];
		float tangent[4];
	};

	struct MeshPrimitive {
		std::vector<Vertex>   vertices;
		std::vector<uint32_t> indices;
	};

	class GltfLoader {
	public:
		GltfLoader(::FileSystem::FilesystemInterface& iface);

		Error load(const std::string& path);

		const std::vector<MeshPrimitive>& primitives() const { return m_primitives; }

	private:
		tinygltf::Model            m_model;
		std::vector<MeshPrimitive> m_primitives;

		MeshPrimitive extract_primitive(const tinygltf::Primitive& prim);

		void copy_attrib(const tinygltf::Primitive& prim,
			const std::string& semantic,
			std::vector<Vertex>& verts,
			size_t                      byteOffset,
			int                         numComponents);

		const tinygltf::Accessor& accessor(const tinygltf::Primitive& prim,
			const std::string& semantic);

		FileSystem::FilesystemInterface& fs_iface;
	};
}

