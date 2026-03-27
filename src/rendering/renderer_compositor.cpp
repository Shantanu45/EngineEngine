#include "renderer_compositor.h"

namespace Rendering
{

	RendererCompositor::RendererCompositor()
	{
		rendering_device = RenderingDevice::get_singleton();
	}


	RendererCompositor::~RendererCompositor()
	{
		finalize();
	}

	void RendererCompositor::blit_render_targets_to_screen(const BlitToScreen* p_render_targets)
	{
		rendering_device->begin_for_screen(screen);

		RID rd_texture = p_render_targets[0].render_target;		// 0 for now

		std::unordered_map<RID, RID>::iterator it = render_target_descriptors.find(rd_texture);
		if (it == render_target_descriptors.end() || !rendering_device->uniform_set_is_valid(it->second)) {
			std::vector<RenderingDevice::Uniform> uniforms;
			RenderingDevice::Uniform u;
			u.uniform_type = RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = 0;
			u.append_id(blit.sampler);
			u.append_id(rd_texture);
			uniforms.push_back(u);
			RID uniform_set = rendering_device->uniform_set_create(uniforms, blit.shader, 0);

			it = render_target_descriptors.insert({ rd_texture, uniform_set }).first;
		}

		Size2 screen_size(rendering_device->screen_get_width(screen), rendering_device->screen_get_height(screen));

		rendering_device->bind_render_pipeline(rendering_device->get_current_command_buffer(), blit_pipeline);
		rendering_device->bind_index_array(blit.array);
		rendering_device->bind_uniform_set(blit.shader, it->second, 0);
		
		rendering_device->render_draw_indexed(rendering_device->get_current_command_buffer(), 6, 1, 0, 0, 0);
	}


	void RendererCompositor::begin_frame()
	{

	}


	void RendererCompositor::end_frame(bool p_present)
	{
		rendering_device->swap_buffers(p_present);
	}


	void RendererCompositor::initailize(DisplayServerEnums::WindowID p_screen)
	{
		screen = p_screen;
		blit.shader = rendering_device->create_program({ "assets://shaders/blit.vert", "assets://shaders/blit.frag" });
		ERR_FAIL_COND_MSG(blit.shader.is_null(), "could not create blit shader module");
		// blit.shader_version = blit.shader.version_create();

		RenderingDeviceCommons::PipelineRasterizationState rs;
		rs.front_face = RenderingDeviceCommons::POLYGON_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.cull_mode = RenderingDeviceCommons::POLYGON_CULL_DISABLED;

		auto blend_state = RenderingDeviceCommons::PipelineColorBlendState::create_blend();

		blit_pipeline = rendering_device->create_swapchain_pipeline(screen, blit.shader,
			-1, RenderingDeviceCommons::RENDER_PRIMITIVE_TRIANGLES,
			rs, RenderingDeviceCommons::PipelineMultisampleState(),
			RenderingDeviceCommons::PipelineDepthStencilState(), blend_state,
			0);

		std::vector<uint8_t> pv;
		pv.resize(6 * 4);
		{
			uint8_t* w = pv.data();
			uint32_t* p32 = (uint32_t*)w;
			p32[0] = 0;
			p32[1] = 1;
			p32[2] = 2;
			p32[3] = 0;
			p32[4] = 2;
			p32[5] = 3;
		}
		blit.index_buffer = rendering_device->index_buffer_create(6, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32, pv);
		blit.array = rendering_device->index_array_create(blit.index_buffer, 0, 6);

		blit.sampler = rendering_device->sampler_create(RenderingDevice::SamplerState());

		rendering_device->_submit_transfer_workers();
	}


	void RendererCompositor::finalize()
	{
		//blit.shader.version_free(blit.shader_version);
		rendering_device->free_rid(blit.index_buffer);
		for (auto uniform: render_target_descriptors)
		{
			rendering_device->free_rid(uniform.second);
			rendering_device->free_rid(uniform.first);
		}
		rendering_device->free_rid(blit.sampler);
		rendering_device->free_rid(blit_pipeline);
	}

}
