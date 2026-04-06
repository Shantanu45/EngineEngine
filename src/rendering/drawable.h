#pragma once
#include <array>
#include <cstring>
#include "rendering/rendering_device.h"
#include "rendering/mesh_storage.h"

namespace Rendering
{

	// ----------------------------------------------------------------
	// Push constants
	// ----------------------------------------------------------------
	struct PushConstantData {
		std::array<uint8_t, 128> data;
		uint32_t                 size = 0;

		template<typename T>
		static PushConstantData from(const T& value) {
			static_assert(sizeof(T) <= 128, "Push constant too large");
			PushConstantData pc;
			pc.size = sizeof(T);
			std::memcpy(pc.data.data(), &value, sizeof(T));
			return pc;
		}
	};

	// ----------------------------------------------------------------
	// Drawable
	// ----------------------------------------------------------------
	struct Drawable {
		RID                   pipeline;
		MeshHandle            mesh;
		RID                   uniform_set;
		uint32_t              uniform_set_index = 0;
		const char* shader_name;
		PushConstantData      push_constants;

		static Drawable make(
			RID pipeline,
			MeshHandle mesh,
			const char* shader_name,
			PushConstantData pc,
			RID uniform_set = RID(),
			uint32_t uniform_set_index = 0)
		{
			Drawable d;
			d.pipeline = pipeline;
			d.mesh = mesh;
			d.shader_name = shader_name;
			d.push_constants = pc;
			d.uniform_set = uniform_set;
			d.uniform_set_index = uniform_set_index;
			return d;
		}
	};

	// ----------------------------------------------------------------
	// Submit — lives here because it needs Drawable + MeshStorage
	// but has no dependency on WSI
	// ----------------------------------------------------------------
	inline void submit_drawable(
		RenderContext& rc,
		RDD::CommandBufferID cmd,
		const Drawable& drawable,
		MeshStorage& storage)
	{
		rc.device->bind_render_pipeline(cmd, drawable.pipeline);

		rc.device->set_push_constant(
			drawable.push_constants.data.data(),
			drawable.push_constants.size,
			rc.device->get_shader_rid(drawable.shader_name));

		if (drawable.uniform_set.is_valid()) {
			rc.device->bind_uniform_set(
				rc.device->get_shader_rid(drawable.shader_name),
				drawable.uniform_set,
				drawable.uniform_set_index);
		}

		const MeshGPU* mesh = storage.get(drawable.mesh);
		if (!mesh) {
			LOGE("submit_drawable: mesh handle {} not found in storage", drawable.mesh);
			return;
		}

		for (auto& prim : mesh->primitives) {
			rc.device->bind_vertex_array(prim.vertex_array);
			rc.device->bind_index_array(prim.index_array);
			rc.device->render_draw_indexed(cmd, prim.index_count, 1, 0, 0, 0);
		}
	}

} // namespace Rendering