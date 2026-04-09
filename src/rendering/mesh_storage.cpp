#include "mesh_storage.h"
#include "libassert/assert.hpp"

namespace Rendering
{

	void MeshStorage::initialize(RenderingDevice* device)
	{
		DEBUG_ASSERT(device != nullptr, "MeshStorage::initialize - device is null");
		rd = device;
	}

	void MeshStorage::finalize()
	{
		// Collect handles first to avoid mutating the map while iterating
		std::vector<MeshHandle> handles;
		handles.reserve(meshes.size());
		for (auto& [handle, _] : meshes)
			handles.push_back(handle);

		for (auto h : handles)
			destroy_mesh(h);

		DEBUG_ASSERT(meshes.empty());
		DEBUG_ASSERT(name_to_handle.empty());
	}

	Rendering::MeshHandle MeshStorage::create_mesh( const std::string& name, std::span<uint8_t> vertex_data,
		std::span<uint8_t> index_data, const std::vector<PrimitiveRange>& ranges,
		RenderingDevice::VertexFormatID vertex_format, RenderingDeviceCommons::IndexBufferFormat index_format)
	{
		DEBUG_ASSERT(rd != nullptr, "MeshStorage not initialized");
		DEBUG_ASSERT(!vertex_data.empty(), "Vertex data is empty");
		DEBUG_ASSERT(!index_data.empty(), "Index data is empty");
		DEBUG_ASSERT(!ranges.empty(), "No primitive ranges provided");

		if (has(name))
		{
			LOGW("MeshStorage: mesh '{}' already exists, returning existing handle.", name);
			return get_handle(name);
		}

		const uint32_t index_stride = (index_format == RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16)
			? sizeof(uint16_t)
			: sizeof(uint32_t);

		const uint32_t total_indices = static_cast<uint32_t>(index_data.size() / index_stride);

		MeshGPU mesh;
		mesh.vertex_buffer = rd->vertex_buffer_create(vertex_data.size(), vertex_data);
		mesh.index_buffer = rd->index_buffer_create(total_indices, index_format, index_data);

		for (auto& r : ranges)
		{
			PrimitiveGPU prim{};
			prim.vertex_count = r.vertex_count;
			prim.index_count = r.index_count;
			prim.vertex_array = rd->vertex_array_create( r.vertex_count, vertex_format, { mesh.vertex_buffer }, { r.vertex_byte_offset });
			prim.index_array = rd->index_array_create( mesh.index_buffer, r.index_offset, r.index_count);

			mesh.primitives.push_back(prim);
		}

		MeshHandle handle = next_handle++;
		meshes[handle] = std::move(mesh);
		name_to_handle[name] = handle;
		return handle;
	}

	void MeshStorage::destroy_mesh(MeshHandle handle)
	{
		auto mesh_it = meshes.find(handle);
		if (mesh_it == meshes.end())
			return;

		auto& mesh = mesh_it->second;

		// Free per-primitive arrays first (they reference the shared buffers)
		for (auto& p : mesh.primitives)
		{
			rd->_free_dependencies_of(p.vertex_array);
			rd->_free_dependencies_of(p.index_array);
		}

		// TODO: should be this, instred of workaround done below.
		// Free the shared buffers once
		//rd->free_rid(mesh.vertex_buffer);
		//rd->free_rid(mesh.index_buffer);

		rd->free_rid(mesh.primitives[0].vertex_array);		// workaround
		rd->free_rid(mesh.primitives[0].index_array);		// workaround

		// Remove name -> handle mapping
		for (auto it = name_to_handle.begin(); it != name_to_handle.end(); ++it)
		{
			if (it->second == handle)
			{
				name_to_handle.erase(it);
				break;
			}
		}

		meshes.erase(mesh_it);
	}

	void MeshStorage::destroy_mesh(const std::string& name)
	{
		auto it = name_to_handle.find(name);
		if (it == name_to_handle.end())
			return;
		destroy_mesh(it->second);
	}

	const MeshGPU* MeshStorage::get(MeshHandle handle) const
	{
		auto it = meshes.find(handle);
		return it != meshes.end() ? &it->second : nullptr;
	}

	const MeshGPU* MeshStorage::get(const std::string& name) const
	{
		auto it = name_to_handle.find(name);
		if (it == name_to_handle.end())
			return nullptr;
		return get(it->second);
	}

	MeshHandle MeshStorage::get_handle(const std::string& name) const
	{
		auto it = name_to_handle.find(name);
		return it != name_to_handle.end() ? it->second : INVALID_MESH;
	}

	bool MeshStorage::has(const std::string& name) const
	{
		return name_to_handle.contains(name);
	}

	bool MeshStorage::has(MeshHandle handle) const
	{
		return meshes.contains(handle);
	}

} // namespace Rendering