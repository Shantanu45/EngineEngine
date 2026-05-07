#pragma once
#include <array>
#include <cstring>
#include <vector>
#include "rendering/rendering_device.h"
#include "rendering/mesh_storage.h"
#include "rendering/pipeline_builder.h"
#include "util/small_vector.h"

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
		Pipeline                       pipeline;
		MeshHandle                     mesh;
		PushConstantData               push_constants;
		Util::SmallVector<UniformSetBinding> uniform_sets;
		Util::SmallVector<RID>               material_sets; // per-primitive Set 2, indexed by primitive slot

		static Drawable make(
			Pipeline pipeline,
			MeshHandle mesh,
			PushConstantData pc,
			Util::SmallVector<UniformSetBinding> uniform_sets = {},
			Util::SmallVector<RID> material_sets = {})
		{
			Drawable d;
			d.pipeline = pipeline;
			d.mesh = mesh;
			d.push_constants = pc;
			d.uniform_sets = std::move(uniform_sets);
			d.material_sets = std::move(material_sets);
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
		rc.device->bind_render_pipeline(cmd, drawable.pipeline.pipeline_rid);
		if (drawable.push_constants.size > 0) {
			rc.device->set_push_constant(
				drawable.push_constants.data.data(),
				drawable.push_constants.size,
				drawable.pipeline.shader_rid);
		}

		for (const auto& binding : drawable.uniform_sets) {
			if (binding.set.is_valid()) {
				rc.device->bind_uniform_set(
					drawable.pipeline.shader_rid,
					binding.set,
					binding.index);
			}
		}

		const MeshGPU* mesh = storage.get(drawable.mesh);
		if (!mesh) {
			LOGE("submit_drawable: mesh handle {} not found in storage", drawable.mesh);
			return;
		}
		for (size_t i = 0; i < mesh->primitives.size(); ++i) {
			const auto& prim = mesh->primitives[i];
			if (i < drawable.material_sets.size() && drawable.material_sets[i].is_valid())
				rc.device->bind_uniform_set(drawable.pipeline.shader_rid, drawable.material_sets[i], 2);
			rc.device->bind_vertex_array(prim.vertex_array);
			rc.device->bind_index_array(prim.index_array);
			rc.device->render_draw_indexed(cmd, prim.index_count, 1, 0, 0, 0);
		}
	}
} // namespace Rendering