// drawable.h

#include "rendering_device.h"
#include "mesh_loader.h"
#include "render_passes/framegraph_resources.h"

namespace Rendering
{
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

	struct Drawable 
	{
		RID                    pipeline;
		Rendering::MeshHandle  mesh;
		RID                    uniform_set;        // invalid RID = skip binding
		uint32_t               uniform_set_index = 0;
		const char* shader_name;
		PushConstantData       push_constants;

		// Convenience factory so construction reads clearly at the call site
		static Drawable make(RID pipeline,Rendering::MeshHandle mesh, const char* shader_name,
								PushConstantData pc, RID uniform_set = RID(), uint32_t uniform_set_index = 0)
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

	// The single draw function — all your per-object GPU state goes through here
	inline void submit_drawable(Rendering::RenderContext& rc, RDD::CommandBufferID cmd, const Drawable& drawable)
	{
		rc.device->bind_render_pipeline(cmd, drawable.pipeline);

		rc.device->set_push_constant( drawable.push_constants.data.data(), drawable.push_constants.size, rc.device->get_shader_rid(drawable.shader_name));

		if (drawable.uniform_set.is_valid()) {
			rc.device->bind_uniform_set(
				rc.device->get_shader_rid(drawable.shader_name),
				drawable.uniform_set,
				drawable.uniform_set_index);
		}

		rc.wsi->draw_mesh(cmd, drawable.mesh);
	}
}

