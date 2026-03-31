#pragma  once
#include "wsi.h"

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
			//BlitPushConstant push_constant;
			RID shader;
			//RID shader_version;
			//HashMap<RenderingDevice::FramebufferFormatID, BlitPipelines> pipelines_by_format;
			RID index_buffer;
			RID array;
			RID sampler;
		} blit;

	public:
		RendererCompositor();
		~RendererCompositor();

		void blit_render_targets_to_screen(const BlitToScreen* p_render_targets);

		void begin_frame();

		void end_frame(bool p_present);

		void initailize(DisplayServerEnums::WindowID p_screen);

		bool is_blit_pass_active() const
		{
			return initialized;
		}

		void finalize();
	private:

		std::unordered_map<RID, RID> render_target_descriptors;
		RID blit_pipeline;

		std::vector<uint8_t> pv;

		RenderingDevice* rendering_device;
		DisplayServerEnums::WindowID screen;

		bool initialized = false;
	};
}
