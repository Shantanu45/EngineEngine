#pragma once

#include <string>
#include <memory>
#include "rendering/mesh_storage.h"
#include "rendering/gltf_loader.h"

namespace Rendering
{

	class MeshLoader
	{
	public:
		explicit MeshLoader(FilesystemInterface& fs)
			: gltf_loader(std::make_unique<GltfLoader>(fs))
		{
		}

		// Load a .gltf/.glb and upload it into MeshStorage.
		// vertex_format must already be created before calling this.
		// Returns INVALID_MESH on any failure.
		MeshHandle load_gltf(
			MeshStorage& storage,
			const std::string& path,
			const std::string& name,
			RenderingDevice::VertexFormatID           vertex_format,
			RenderingDeviceCommons::IndexBufferFormat index_format = RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32)
		{
			if (storage.has(name))
			{
				LOGW("MeshLoader: '{}' already loaded, returning existing handle.", name);
				return storage.get_handle(name);
			}

			if (gltf_loader->load(path) != OK)
			{
				LOGE("MeshLoader: failed to open '{}'", path);
				return INVALID_MESH;
			}

			auto prims = gltf_loader->primitives();
			if (prims.empty())
			{
				LOGE("MeshLoader: '{}' contains no primitives", path);
				return INVALID_MESH;
			}

			return _build_and_upload(storage, name, prims, vertex_format, index_format);
		}

	private:
		std::unique_ptr<GltfLoader> gltf_loader;

		MeshHandle _build_and_upload(
			MeshStorage& storage,
			const std::string& name,
			const std::vector<MeshPrimitive>& prims,
			RenderingDevice::VertexFormatID           vertex_format,
			RenderingDeviceCommons::IndexBufferFormat index_format)
		{
			const uint32_t index_stride = (index_format == RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16)
				? sizeof(uint16_t)
				: sizeof(uint32_t);

			std::vector<uint8_t>        vertex_data;
			std::vector<uint8_t>        index_data;
			std::vector<PrimitiveRange> ranges;

			uint32_t total_vertices = 0;
			uint32_t total_indices = 0;

			for (auto& p : prims)
			{
				DEBUG_ASSERT(!p.vertices.empty(), "Primitive has no vertices");
				DEBUG_ASSERT(!p.indices.empty(), "Primitive has no indices");

				PrimitiveRange r{};
				r.vertex_offset = total_vertices;
				r.vertex_byte_offset = static_cast<uint64_t>(total_vertices) * sizeof(Vertex);
				r.index_offset = total_indices;
				r.index_count = static_cast<uint32_t>(p.indices.size());
				r.vertex_count = static_cast<uint32_t>(p.vertices.size());
				ranges.push_back(r);

				// Append vertex bytes
				const uint64_t vb_size = p.vertices.size() * sizeof(Vertex);
				const size_t   vb_off = vertex_data.size();
				vertex_data.resize(vb_off + vb_size);
				std::memcpy(vertex_data.data() + vb_off, p.vertices.data(), vb_size);

				// Append index bytes
				const uint64_t ib_size = p.indices.size() * index_stride;
				const size_t   ib_off = index_data.size();
				index_data.resize(ib_off + ib_size);
				std::memcpy(index_data.data() + ib_off, p.indices.data(), ib_size);

				total_vertices += r.vertex_count;
				total_indices += r.index_count;
			}

			return storage.create_mesh(name, vertex_data, index_data, ranges, vertex_format, index_format);
		}
	};

} // namespace Rendering