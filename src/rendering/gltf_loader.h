// gltf_loader.h
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "util/error_macros.h"

#include "tiny_gltf.h"

namespace Renderer
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
	};
}

