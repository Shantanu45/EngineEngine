#pragma once
#include <array>
#include <cstring>
#include <vector>
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
	struct UniformSetBinding {
		RID      set;
		uint32_t index = 0;
	};

	struct Drawable {
		RID                            pipeline;
		MeshHandle                     mesh;
		const char* shader_name;
		PushConstantData               push_constants;
		std::vector<UniformSetBinding> uniform_sets;  // replaces single set

		static Drawable make(
			RID pipeline,
			MeshHandle mesh,
			const char* shader_name,
			PushConstantData pc,
			std::vector<UniformSetBinding> uniform_sets = {})
		{
			Drawable d;
			d.pipeline = pipeline;
			d.mesh = mesh;
			d.shader_name = shader_name;
			d.push_constants = pc;
			d.uniform_sets = std::move(uniform_sets);
			return d;
		}

		void set_uniform_set(UniformSetBinding uniform_set)
		{
			uniform_sets.push_back(uniform_set);
		}
	};

	// ----------------------------------------------------------------
	// Submit
	// ----------------------------------------------------------------
	inline void submit_drawable(
		RenderContext& rc,
		RDD::CommandBufferID cmd,
		const Drawable& drawable,
		MeshStorage& storage)
	{
		rc.device->bind_render_pipeline(cmd, drawable.pipeline);
		if (drawable.push_constants.size > 0) {
			rc.device->set_push_constant(
				drawable.push_constants.data.data(),
				drawable.push_constants.size,
				rc.device->get_shader_rid(drawable.shader_name));
		}

		for (const auto& binding : drawable.uniform_sets) {
			if (binding.set.is_valid()) {
				rc.device->bind_uniform_set(
					rc.device->get_shader_rid(drawable.shader_name),
					binding.set,
					binding.index);
			}
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