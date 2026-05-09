#pragma  once
#include "wsi.h"
#include "rid_handle.h"
#include "uniform_buffer.h"
#include "util/small_vector.h"

#include <glm/glm.hpp>

namespace Rendering
{
	struct BlitSettingsUBO {
		glm::vec4 tone_mapping = glm::vec4(1.0f, 2.0f, 0.0f, 0.0f);
	};

	struct BlitToScreen {
		RID render_target;
		RID ui;
		float exposure = 1.0f;
		uint32_t tone_mapper = 2;
		uint32_t material_debug_view = 0;
		Rect2 src_rect = Rect2(0.0, 0.0, 1.0, 1.0);
		Rect2i dst_rect;

		struct {
			bool use_layer = false;
			uint32_t layer = 0;
		} multi_view;

	};

	class RendererCompositor
	{
		struct Blit {
			RID       shader;       // borrowed — owned by RenderingDevice::shader_cache
			RIDHandle index_buffer;
			RIDHandle array;
			RIDHandle sampler;
			UniformBuffer<BlitSettingsUBO> settings_ubo;
		} blit;

	public:
		RendererCompositor();
		~RendererCompositor() = default;

		void blit_render_targets_to_screen(const BlitToScreen* p_render_targets);

		void begin_frame();
		void end_frame(bool p_present);
		void initailize(DisplayServerEnums::WindowID p_screen);

		bool is_blit_pass_active() const { return initialized; }

	private:
		// Transient blit uniform sets: keyed by frame-graph texture RID.
		// These uniform sets depend on the transient textures, so the device's
		// dependency cascade frees them automatically when the texture is freed
		// at end-of-frame. We must NOT own them (no RIDHandle) — just clear
		// this map each frame so stale entries don't accumulate.
		std::unordered_map<RID, RID> render_target_descriptors;

		RIDHandle blit_pipeline;

		Util::SmallVector<uint8_t> pv;

		RenderingDevice* rendering_device;
		DisplayServerEnums::WindowID screen;

		bool initialized = false;
	};
}
