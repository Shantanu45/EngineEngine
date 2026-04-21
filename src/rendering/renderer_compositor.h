#pragma  once
#include "wsi.h"
#include "rid_handle.h"

namespace Rendering
{
	struct BlitToScreen {
		RID render_target;
		RID ui;
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
		// Lookup map — raw RIDs, ownership tracked in uniform_set_handles below.
		std::unordered_map<RID, RID> render_target_descriptors;
		std::vector<RIDHandle>       uniform_set_handles;

		RIDHandle blit_pipeline;

		std::vector<uint8_t> pv;

		RenderingDevice* rendering_device;
		DisplayServerEnums::WindowID screen;

		bool initialized = false;
	};
}
