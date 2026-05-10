#include "rendering/gltf_scene_importer.h"

#include <functional>

#include "rendering/gltf_material_bridge.h"

namespace Rendering {

namespace {

ImageData make_rgba8_image(const GltfImageData& gi)
{
	ImageData img;
	img.width            = gi.width;
	img.height           = gi.height;
	img.channels         = gi.channels;
	img.desired_channels = 4;
	img.format           = Format::R8G8B8A8_UNORM;

	const size_t pixel_count = static_cast<size_t>(gi.width) * static_cast<size_t>(gi.height);
	img.pixels.resize(pixel_count * 4);

	const int src_channels = gi.channels > 0 ? gi.channels : 4;
	for (size_t i = 0; i < pixel_count; i++) {
		const size_t src = i * static_cast<size_t>(src_channels);
		const size_t dst = i * 4;
		img.pixels[dst + 0] = src_channels > 0 && src + 0 < gi.pixels.size() ? gi.pixels[src + 0] : 255;
		img.pixels[dst + 1] = src_channels > 1 && src + 1 < gi.pixels.size() ? gi.pixels[src + 1] : img.pixels[dst + 0];
		img.pixels[dst + 2] = src_channels > 2 && src + 2 < gi.pixels.size() ? gi.pixels[src + 2] : img.pixels[dst + 0];
		img.pixels[dst + 3] = src_channels > 3 && src + 3 < gi.pixels.size() ? gi.pixels[src + 3] : 255;
	}

	return img;
}

} // namespace

ImportedGltfScene GltfSceneImporter::import(const GltfSceneImportContext& context) const
{
	ImportedGltfScene imported;
	GltfScene* scene = context.mesh_loader.get_scene();

	const std::string gltf_key_base = context.source_path + ":";
	Util::SmallVector<RID> color_image_rids;
	Util::SmallVector<RID> linear_image_rids;
	for (int i = 0; i < static_cast<int>(scene->images.size()); i++) {
		auto& gi = scene->images[i];
		ImageData img = make_rgba8_image(gi);
		color_image_rids.push_back(context.texture_cache.raw(
			gltf_key_base + std::to_string(i),
			img,
			/*is_srgb=*/true));
		linear_image_rids.push_back(context.texture_cache.raw(
			gltf_key_base + std::to_string(i),
			img,
			/*is_srgb=*/false));
		{ auto _ = std::move(gi.pixels); }
	}

	for (const auto& pbr : scene->materials) {
		Material mat = material_from_pbr(pbr, color_image_rids, linear_image_rids);
		imported.materials.push_back(context.material_registry.create(
			context.device,
			std::move(mat),
			context.fallback_texture,
			context.shader_rid,
			context.pbr_shader_rid,
			context.shadow_shader_rid,
			context.point_shadow_shader_rid,
			context.transparent_shader_rid,
			context.transparent_pbr_shader_rid));
	}

	Util::SmallVector<Util::SmallVector<MeshHandle>> primitive_handles(scene->meshes.size());
	for (int mesh_index = 0; mesh_index < static_cast<int>(scene->meshes.size()); mesh_index++) {
		const auto& mesh = scene->meshes[mesh_index];
		primitive_handles[mesh_index].resize(mesh.primitives.size(), INVALID_MESH);
		for (int primitive_index = 0; primitive_index < static_cast<int>(mesh.primitives.size()); primitive_index++) {
			std::string name = context.name_prefix + "/m" + std::to_string(mesh_index) +
				"/p" + std::to_string(primitive_index);
			primitive_handles[mesh_index][primitive_index] = context.mesh_loader.upload_primitive(
				context.mesh_storage,
				name,
				mesh.primitives[primitive_index],
				context.vertex_format);
		}
	}

	std::function<void(int)> visit = [&](int node_index) {
		const auto& node = scene->nodes[node_index];

		if (node.mesh_index >= 0 && node.mesh_index < static_cast<int>(primitive_handles.size())) {
			const auto& mesh = scene->meshes[node.mesh_index];
			for (int primitive_index = 0; primitive_index < static_cast<int>(mesh.primitives.size()); primitive_index++) {
				const auto& primitive = mesh.primitives[primitive_index];
				GltfPrimitiveInstance instance;
				instance.mesh = primitive_handles[node.mesh_index][primitive_index];
				instance.materials.push_back(
					primitive.material_index >= 0
						? imported.materials[primitive.material_index]
						: INVALID_MATERIAL);
				instance.local_aabb = compute_aabb_single(primitive);
				instance.node_index = node_index;
				instance.mesh_index = node.mesh_index;
				instance.primitive_index = primitive_index;
				imported.primitives.push_back(std::move(instance));
			}
		}

		for (int child : node.children)
			visit(child);
	};

	if (!scene->scenes.empty()) {
		for (int root : scene->scenes[scene->default_scene].root_nodes)
			visit(root);
	}

	return imported;
}

} // namespace Rendering
