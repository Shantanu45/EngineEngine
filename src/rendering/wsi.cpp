#include "wsi.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_device.h"
#include "libassert/assert.hpp"
#include "compiler/compiler.h"

namespace Rendering
{
	WSI::WSI()
	{

	}

	Error WSI::initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window)
	{
		rendering_driver = p_rendering_driver;
		if (rendering_driver == "vulkan") {
			rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
		}
		else
		{
			// api not supported
			DEBUG_ASSERT(false);
		}

		if (rendering_context != nullptr) {
			rendering_context->set_platform_surface_extension(platform->get_instance_extensions());
			if (rendering_context->initialize() == OK) {
				DEBUG_ASSERT(platform || main_window_created);

				if (_create_rendering_context_window(DisplayServerEnums::MAIN_WINDOW_ID, rendering_driver) == OK) {
					rendering_device = RenderingDevice::get_singleton();
					// device initialization happens in function call below
					if (!(rendering_device->initialize(rendering_context.get(), DisplayServerEnums::MAIN_WINDOW_ID) == OK)) {
						return FAILED;
					}
				}
			}
		}

		if (rendering_context) {
			DEV_ASSERT(rendering_device != nullptr);

			rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
		}
		return OK;
	}


	void WSI::set_platform(WSIPlatform* p_platform)
	{
		platform = p_platform;
	}

	Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
	{
		WindowData& wd = windows[p_window_id];
		wd.platfform_data = platform->get_window_platform_data(p_window_id);

		Error err = rendering_context->window_create(p_window_id, &wd.platfform_data);
		ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create %s window.", p_rendering_driver));
		rendering_context->window_set_size(p_window_id, platform->get_surface_width(), platform->get_surface_height());
		surface = rendering_context->surface_get_from_window(p_window_id);
		return OK;
	}

	// draw viewport
	void WSI::draw_viewport(bool p_swap_buffers)
	{
		// blit_render_targets_to_screen
			// screen_prepare_for_drawing
		rendering_device->screen_prepare_for_drawing(DisplayServerEnums::MAIN_WINDOW_ID);
		
	}

	void WSI::set_program(const std::vector<std::string> programs)
	{
		RDShaderSource* shaders = new RDShaderSource();
		shaders->set_language(RenderingDeviceCommons::SHADER_LANGUAGE_GLSL);
		for (auto shader_path: programs)
		{
			auto stage = shader_stage_from_compiler_stage(Compiler::stage_from_path(shader_path));
			ERR_FAIL_COND_MSG(stage == RenderingDeviceCommons::SHADER_STAGE_MAX, "could not evaluate shader stage from path!!");
			shaders->set_stage_source(stage, shader_path);
		}
		shader_program = rendering_device->shader_create_from_spirv(rendering_device->shader_compile_spirv_from_shader_source(shaders), "traingle_shader");
	}

	RenderingShaderContainerFormat* WSI::create_shader_container_format() 
	{
		return new ::Vulkan::RenderingShaderContainerFormatVulkan();
	}

	void WSI::teardown()
	{
	}

	WSI::~WSI()
	{

	}

}
