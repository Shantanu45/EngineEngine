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

		// Load all meshes from a .gltf/.glb into MeshStorage.
		// Each mesh gets its own handle, named "<name>/<mesh_name>" or
		// "<name>/mesh_0" if the mesh has no name in the file.
		// Returns the handle of the FIRST mesh (for simple single-mesh files).
		// Returns INVALID_MESH on any failure.
		MeshHandle load_gltf(
			MeshStorage& storage,
			const std::string& path,
			const std::string& name,
			RenderingDevice::VertexFormatID           vertex_format,
			RenderingDeviceCommons::IndexBufferFormat index_format
			= RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32)
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

			const auto& scene = gltf_loader->scene();

			if (scene.meshes.empty())
			{
				LOGE("MeshLoader: '{}' contains no meshes", path);
				return INVALID_MESH;
			}

			MeshHandle first = INVALID_MESH;

			for (size_t mi = 0; mi < scene.meshes.size(); ++mi)
			{
				const auto& mesh = scene.meshes[mi];

				if (mesh.primitives.empty())
				{
					LOGW("MeshLoader: mesh {} in '{}' has no primitives, skipping", mi, path);
					continue;
				}

				// Build a unique storage name per mesh
				// Single-mesh files: just use the caller's name directly
				// Multi-mesh files:  "<name>/<mesh_name_or_index>"
				std::string mesh_name = (scene.meshes.size() == 1)
					? name
					: name + "/" + (mesh.name.empty()
						? "mesh_" + std::to_string(mi)
						: mesh.name);

				MeshHandle h = _build_and_upload(
					storage, mesh_name, mesh.primitives, vertex_format, index_format);

				if (h != INVALID_MESH && first == INVALID_MESH)
					first = h;
			}

			return first;
		}

		// Convenience: load and retrieve all handles produced
		// Useful when you need to address individual meshes in a multi-mesh file
		std::vector<MeshHandle> load_gltf_all(
			MeshStorage& storage,
			const std::string& path,
			const std::string& name,
			RenderingDevice::VertexFormatID           vertex_format,
			RenderingDeviceCommons::IndexBufferFormat index_format
			= RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32)
		{
			std::vector<MeshHandle> handles;

			if (gltf_loader->load(path) != OK)
			{
				LOGE("MeshLoader: failed to open '{}'", path);
				return handles;
			}

			const auto& scene = gltf_loader->scene();

			for (size_t mi = 0; mi < scene.meshes.size(); ++mi)
			{
				const auto& mesh = scene.meshes[mi];
				if (mesh.primitives.empty()) continue;

				std::string mesh_name = name + "/" + (mesh.name.empty()
					? "mesh_" + std::to_string(mi)
					: mesh.name);

				MeshHandle h = _build_and_upload(
					storage, mesh_name, mesh.primitives, vertex_format, index_format);

				if (h != INVALID_MESH)
					handles.push_back(h);
			}

			return handles;
		}

		// Expose the loaded scene so the caller can access materials,
		// nodes, animations etc. after a load_gltf call
		const GltfScene* get_scene() const
		{
			return &gltf_loader->scene();
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
			if (storage.has(name))
			{
				LOGW("MeshLoader: '{}' already in storage, skipping upload.", name);
				return storage.get_handle(name);
			}

			const uint32_t index_stride =
				(index_format == RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16)
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

				// Append index bytes — uint16 path needs conversion
				if (index_format == RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16)
				{
					const size_t ib_off = index_data.size();
					index_data.resize(ib_off + p.indices.size() * sizeof(uint16_t));
					uint16_t* dst = reinterpret_cast<uint16_t*>(index_data.data() + ib_off);
					for (size_t i = 0; i < p.indices.size(); ++i)
						dst[i] = static_cast<uint16_t>(p.indices[i]);
				}
				else
				{
					const uint64_t ib_size = p.indices.size() * sizeof(uint32_t);
					const size_t   ib_off = index_data.size();
					index_data.resize(ib_off + ib_size);
					std::memcpy(index_data.data() + ib_off, p.indices.data(), ib_size);
				}

				total_vertices += r.vertex_count;
				total_indices += r.index_count;
			}

			return storage.create_mesh(
				name, vertex_data, index_data, ranges, vertex_format, index_format);
		}
	};

} // namespace Rendering