#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include "rendering_device.h"

namespace Rendering
{

	using MeshHandle = uint64_t;
	static constexpr MeshHandle INVALID_MESH = 0;

	// Describes where in the shared VB/IB a single primitive lives
	struct PrimitiveRange
	{
		uint32_t vertex_offset;       // offset in vertices (not bytes)
		uint64_t vertex_byte_offset;  // offset in bytes into the vertex buffer
		uint32_t index_offset;        // offset in indices into the index buffer
		uint32_t index_count;
		uint32_t vertex_count;
	};

	// GPU-resident primitive - owns vertex_array and index_array RIDs
	struct PrimitiveGPU
	{
		RID      vertex_array;
		RID      index_array;
		uint32_t index_count;
		uint32_t vertex_count;
	};

	// GPU-resident mesh - owns the shared VB/IB and a list of primitives
	struct MeshGPU
	{
		std::vector<PrimitiveGPU> primitives;

		// Shared buffers - all primitives in this mesh reference into these
		RID vertex_buffer;
		RID index_buffer;
	};

	class MeshStorage
	{
	public:
		void initialize(RenderingDevice* device);
		void finalize();

		// Upload raw byte data and create GPU resources.
		// Returns INVALID_MESH on failure.
		MeshHandle create_mesh(
			const std::string& name,
			std::span<uint8_t> vertex_data,
			std::span<uint8_t> index_data,
			const std::vector<PrimitiveRange>& ranges,
			RenderingDevice::VertexFormatID vertex_format,
			RenderingDeviceCommons::IndexBufferFormat index_format);

		void destroy_mesh(MeshHandle handle);
		void destroy_mesh(const std::string& name);

		// Returns nullptr if not found
		const MeshGPU* get(MeshHandle handle) const;
		const MeshGPU* get(const std::string& name) const;

		MeshHandle get_handle(const std::string& name) const;

		bool has(const std::string& name) const;
		bool has(MeshHandle handle) const;

	private:
		RenderingDevice* rd = nullptr;

		std::unordered_map<std::string, MeshHandle> name_to_handle;
		std::unordered_map<MeshHandle, MeshGPU>     meshes;
		MeshHandle next_handle = 1;
	};

} // namespace Rendering