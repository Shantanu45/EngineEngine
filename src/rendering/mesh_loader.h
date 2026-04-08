#pragma once
#include <string>
#include <memory>
#include "rendering/mesh_storage.h"
#include "rendering/gltf_loader.h"

namespace Rendering
{
	using RD = RenderingDevice;
	using RDC = RenderingDeviceCommons;
	class MeshLoader
	{
	public:
		explicit MeshLoader(FilesystemInterface& fs, RenderingDevice* device)
			: gltf_loader(std::make_unique<GltfLoader>(fs)), device(device)
		{
		}

		// Load all meshes from a .gltf/.glb into MeshStorage.
		// Each mesh gets its own handle, named "<name>/<mesh_name>" or
		// "<name>/mesh_0" if the mesh has no name in the file.
		// Returns the handle of the FIRST mesh (for simple single-mesh files).
		// Returns INVALID_MESH on any failure.
		MeshHandle load_gltf( MeshStorage& storage, const std::string& path, const std::string& name,
			RenderingDevice::VertexFormatID vertex_format, RDC::IndexBufferFormat index_format = RDC::INDEX_BUFFER_FORMAT_UINT32)
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
		std::vector<MeshHandle> load_gltf_all( MeshStorage& storage, const std::string& path, const std::string& name,
			RenderingDevice::VertexFormatID vertex_format, RDC::IndexBufferFormat index_format = RDC::INDEX_BUFFER_FORMAT_UINT32)
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

		RD::SamplerState sampler_from_gltf(const GltfScene* scene, int sampler_index)
		{
			RD::SamplerState ss{};  // sensible defaults

			if (sampler_index < 0 || sampler_index >= (int)scene->samplers.size())
				return ss;

			auto& smp = scene->samplers[sampler_index];

			// Wrap modes
			auto wrap = [](int w) {
				switch (w) {
				case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:   return RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
				case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return RD::SAMPLER_REPEAT_MODE_MIRRORED_REPEAT;
				default:                                     return RD::SAMPLER_REPEAT_MODE_REPEAT;
				}
				};
			ss.repeat_u = wrap(smp.wrap_s);
			ss.repeat_v = wrap(smp.wrap_t);

			// Filter modes
			ss.min_filter = (smp.min_filter == TINYGLTF_TEXTURE_FILTER_NEAREST ||
				smp.min_filter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
				? RD::SAMPLER_FILTER_NEAREST : RD::SAMPLER_FILTER_LINEAR;
			ss.mag_filter = (smp.mag_filter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				? RD::SAMPLER_FILTER_NEAREST : RD::SAMPLER_FILTER_LINEAR;

			return ss;
		}

		RID upload_cached(const GltfScene* scene, int image_index, const std::string& file_path)
		{
			std::string key = file_path + ":" + std::to_string(image_index);

			auto it = image_cache.find(key);
			if (it != image_cache.end())
				return it->second;

			auto& img = scene->images[image_index];

			RDC::TextureFormat tf{};
			tf.width = img.width;
			tf.height = img.height;
			tf.array_layers = 1;
			tf.texture_type = RDC::TEXTURE_TYPE_2D;
			tf.usage_bits = RDC::TEXTURE_USAGE_SAMPLING_BIT
				| RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;
			tf.format = RDC::DATA_FORMAT_R8G8B8A8_UNORM;

			RID rid = device->texture_create(tf, RD::TextureView(), { img.pixels });
			image_cache[key] = rid;
			return rid;
		}

		// upload_texture then just delegates to it:
		RID upload_texture(const GltfScene* scene,
			const std::optional<TextureInfo>& tex_info,
			RID fallback,
			const std::string& file_path)
		{
			if (!tex_info.has_value())
				return fallback;
			return upload_cached(scene, tex_info->image_index, file_path);
		}

		void free_owned_resources() {
			for (auto& [key, rid] : image_cache)
				device->free_rid(rid);
			image_cache.clear();
		}

	private:
		std::unique_ptr<GltfLoader> gltf_loader;
		RenderingDevice* device;
		std::unordered_map<std::string, RID> image_cache;

		MeshHandle _build_and_upload( MeshStorage& storage, const std::string& name,
			const std::vector<MeshPrimitive>& prims, RD::VertexFormatID vertex_format, RDC::IndexBufferFormat index_format)
		{
			if (storage.has(name))
			{
				LOGW("MeshLoader: '{}' already in storage, skipping upload.", name);
				return storage.get_handle(name);
			}

			const uint32_t index_stride =
				(index_format == RDC::INDEX_BUFFER_FORMAT_UINT16)
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
				if (index_format == RDC::INDEX_BUFFER_FORMAT_UINT16)
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