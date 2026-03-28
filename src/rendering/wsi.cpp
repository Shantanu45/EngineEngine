/*****************************************************************//**
 * \file   wsi.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "wsi.h"
#include "vulkan/vulkan_device.h"
#include "libassert/assert.hpp"
#include "compiler/compiler.h"
#include "application/service_locator.h"
#include "rendering/renderer_compositor.h"
#include "pipeline_builder.h"

namespace Rendering
{
	WSI::WSI()
	{

	}

	Error WSI::initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window)
	{
		active_window = DisplayServerEnums::MAIN_WINDOW_ID;
		rendering_driver = p_rendering_driver;
		if (rendering_driver == "vulkan") {
			rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
		}
		else
		{
			// api not supported
			DEBUG_ASSERT(false);
			return FAILED;
		}

		if (rendering_context != nullptr) {
			if (!rendering_context->init_loader_and_extensions(windows[active_window].platfform_data.platform))
			{
				LOGE("Failed to initialize Vulkan loader.\n");
				return FAILED;
			}
			if (rendering_context->initialize() == OK && _create_rendering_context_window(active_window, rendering_driver) == OK)
			{
				rendering_device = RenderingDevice::get_singleton();
				// device initialization happens in function call below
				if (!(rendering_device->initialize(rendering_context.get(), active_window) == OK))
				{
					return FAILED;
				}
			}
			
			// initialize TinyGltf
			auto fs = Services::get().get<FilesystemInterface>();
			gltf_loader = std::make_unique<GltfLoader>(*fs);
		}
		return OK;
	}

	bool WSI::pre_frame_loop()
	{
		if (rendering_context && rendering_device) {
			
			DEV_ASSERT(rendering_device != nullptr);

			rendering_device->screen_create(active_window);
			rd = std::make_unique<RendererCompositor>();
			rd->initailize(DisplayServerEnums::MAIN_WINDOW_ID);
			return true;
		}
		return false;
	}

	void WSI::blit_render_target_to_screen(RID texture)
	{
		Rendering::BlitToScreen blit;
		blit.render_target = texture;
		rd->blit_render_targets_to_screen(&blit);
	}

	bool WSI::pre_begin_frame()
	{
		return true;
	}

	bool WSI::begin_frame()
	{
		rendering_device->begin_frame();
		rd->begin_frame();
		rendering_device->_submit_transfer_barriers(rendering_device->get_current_command_buffer());

		rendering_device->screen_prepare_for_drawing(active_window);

		return true;
	}

	bool WSI::end_render_pass(RDD::CommandBufferID cmd)
	{
		rendering_device->end_render_pass(cmd);
		return true;
	}

	bool WSI::end_frame(bool p_present)
	{
		//rendering_device->swap_buffers(p_present);
		rd->end_frame(true);
		return true;
	}

	bool WSI::post_end_frame()
	{
		// TODO
		return false;
	}

	bool WSI::post_frame_loop()
	{
		// TODO
		return false;
	}

	void WSI::set_program(const std::string& p_shader_name, const std::vector<std::string> programs)
	{
		RDShaderSource* shaders = new RDShaderSource();
		shaders->set_language(RenderingDeviceCommons::SHADER_LANGUAGE_GLSL);
		for (auto shader_path: programs)
		{
			auto stage = shader_stage_from_compiler_stage(Compiler::stage_from_path(shader_path));
			ERR_FAIL_COND_MSG(stage == RenderingDeviceCommons::SHADER_STAGE_MAX, "could not evaluate shader stage from path!!");
			shaders->set_stage_source(stage, shader_path);
		}
		shader_program = rendering_device->shader_create_from_spirv(rendering_device->shader_compile_spirv_from_shader_source(shaders), p_shader_name);
	}

	RenderingDeviceCommons::VertexAttribute WSI::get_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride)
	{
		RenderingDeviceCommons::VertexAttribute va;
		va.format = format;
		va.stride = stride;// RenderingDeviceCommons::get_format_vertex_size(format);
		va.binding = binding;
		va.location = location;
		va.offset = offset;
		return va;
	}

	RID WSI::get_current_pipeline()
	{
		return pipeline;
	}

	RenderingShaderContainerFormat* WSI::create_shader_container_format() 
	{
		return new ::Vulkan::RenderingShaderContainerFormatVulkan();
	}

	void WSI::bind_and_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer)
	{
		bool flag = false;
		for (auto& p : primitives)
		{
			rendering_device->bind_vertex_array(vertex_arrays[p.first]);
			rendering_device->bind_index_array(index_arrays[p.first]);
			rendering_device->render_draw_indexed(p_command_buffer, p.second.index_count, 1, 0, 0, 0);
		}
	}

	void WSI::set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data)
	{
		windows.insert({ window, data });
	}

	Error WSI::load_gltf(std::string path)
	{
		if (gltf_loader->load(path) != OK)
			return ERR_FILE_NOT_FOUND;

		auto prims = gltf_loader->primitives();
		for (auto p : prims)
		{
			MeshRange range = { total_vertices, total_vertices * sizeof(Rendering::Vertex), total_indices, (uint32_t)p.indices.size() };
			primitives.insert({ mesh_owner.make_rid(p), range });

			uint64_t vbSize = p.vertices.size() * sizeof(Rendering::Vertex);
			uint64_t ibSize = p.indices.size() * ((index_data_format == Rendering::RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT16)  ?  sizeof(uint16_t) : sizeof(uint32_t));
			push_vertex_data(p.vertices.data(), vbSize);
			push_index_data(p.indices.data(), ibSize);

			total_vertices += p.vertices.size();
			total_indices += p.indices.size();
		}
		return OK;
	}

	std::vector<RenderingDeviceCommons::VertexAttribute> WSI::get_default_vertex_attribute()
	{
		std::vector<RenderingDeviceCommons::VertexAttribute> vertex_attributes;
		vertex_attributes.emplace_back(get_vertex_attribute(0, 0, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, offsetof(Rendering::Vertex, position), sizeof(Rendering::Vertex)));
		vertex_attributes.emplace_back(get_vertex_attribute(0, 1, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, offsetof(Rendering::Vertex, normal), sizeof(Rendering::Vertex)));
		vertex_attributes.emplace_back(get_vertex_attribute(0, 2, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32_SFLOAT, offsetof(Rendering::Vertex, texcoord), sizeof(Rendering::Vertex)));
		vertex_attributes.emplace_back(get_vertex_attribute(0, 3, Rendering::RenderingDeviceCommons::DATA_FORMAT_R32G32B32A32_SFLOAT, offsetof(Rendering::Vertex, tangent), sizeof(Rendering::Vertex)));
		return vertex_attributes;
	}

	void WSI::push_vertex_data(void* data, size_t size)
	{
		const size_t offset = vertex_data.size();
		vertex_data.resize(offset + size);
		memcpy(vertex_data.data() + offset, data, size);
	}

	void WSI::push_index_data(void* data, size_t size)
	{
		const size_t offset = index_data.size();
		index_data.resize(offset + size);
		memcpy(index_data.data() + offset, data, size);

		switch (index_data_format)
		{
		case Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT16:
			index_count = index_data.size() / sizeof(uint16_t);
			break;
		case Rendering::RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32:
			index_count = index_data.size() / sizeof(uint32_t);
			break;
		default:
			index_count = 0;
			break;
		}
	}

	void WSI::pipeline_create()
	{
		RenderingDevice::AttachmentFormat attachment;
		attachment.format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;
		attachment.samples = RenderingDeviceCommons::TEXTURE_SAMPLES_1;
		attachment.usage_flags = RenderingDeviceCommons::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		std::vector<RenderingDevice::AttachmentFormat> screen_attachment;
		screen_attachment.push_back(attachment);
		auto fb_format = rendering_device->framebuffer_format_create(screen_attachment);

		vertex_format = rendering_device->vertex_format_create(get_default_vertex_attribute());

		pipeline = PipelineBuilder{}
			.set_shader({ "assets://shaders/triangle_v2.vert", "assets://shaders/triangle_v2.frag" }, "triangle_shader")
			.set_vertex_format(vertex_format)
			.build(fb_format);

		RenderingDeviceCommons::TextureFormat tf;
		tf.width = rendering_device->screen_get_width();
		tf.height = rendering_device->screen_get_height(); 
		tf.array_layers = 1;
		tf.texture_type = RenderingDeviceCommons::TEXTURE_TYPE_2D;
		tf.usage_bits = RenderingDeviceCommons::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RenderingDeviceCommons::TEXTURE_USAGE_SAMPLING_BIT; ;
		tf.format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;

		texture_fb = rendering_device->texture_create(tf, Rendering::RenderingDevice::TextureView(), { });
		render_pass = rendering_device->render_pass_from_format_id(fb_format);
		frame_buffer = rendering_device->create_framebuffer_from_format_id(fb_format, { texture_fb }, rendering_device->screen_get_width(), rendering_device->screen_get_height());

		_create_vertex_and_index_buffers();
		rendering_device->_submit_transfer_workers();
	}

	void WSI::set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat format)
	{
		index_data_format = format;
	}

	void WSI::teardown()
	{
		rd->finalize();
		rendering_device->finalize();
	}

	WSI::~WSI()
	{

	}

	Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
	{
		// TODO: check if window entry exists

		auto wd = windows.at(p_window_id);

		Error err = rendering_context->window_create(p_window_id, &wd.platfform_data);
		ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create {} window.", p_rendering_driver));
		rendering_context->window_set_size(p_window_id, wd.window_resolution.x, wd.window_resolution.y);
		surface = rendering_context->surface_get_from_window(p_window_id);
		return OK;
	}

	std::vector<uint8_t> WSI::_get_attrib_interleaved(const std::vector<RenderingDeviceCommons::VertexAttribute>& attribs, std::vector<uint8_t> vertex_data)
	{
		std::vector<uint8_t> interleved_data;
		uint32_t vert_num = vertex_data.size() / attribs[0].stride;
		uint32_t stride = attribs[0].stride;
		interleved_data.resize(vertex_data.size());
		for (int v = 0; v < vert_num; v++)
		{
			auto vert_pos = v;
			uint32_t dst_offset;
			uint32_t src_offset;

			auto src_attrib_offset = 0;
			// for each vertex
			for (int i = 0; i < attribs.size() - 1; i++)
			{
				auto size = attribs[i + 1].offset - attribs[i].offset;		// size of the attribute

				dst_offset = (vert_pos * stride) + attribs[i].offset;
				src_offset = (i  * src_attrib_offset) + (vert_pos * size);

				memcpy(interleved_data.data() + dst_offset, vertex_data.data() + src_offset, size);
				src_attrib_offset += (size * vert_num);
			}
			auto last_attrib_size = attribs.back().stride - attribs.back().offset;

			dst_offset = (vert_pos * stride) + attribs.back().offset;
			src_offset = (src_attrib_offset) + (vert_pos * last_attrib_size);

			memcpy(interleved_data.data() + dst_offset, vertex_data.data() + src_offset, last_attrib_size);

		}

		return interleved_data;
	}

	void WSI::_create_vertex_and_index_buffers()
	{
		DEBUG_ASSERT(primitives.size() > 0);
		DEBUG_ASSERT(!vertex_data.empty());

		PackedByteArray interleved;

		//if (vertex_data_mode == VERTEX_DATA_MODE::SEPERATE)
		//{
		//	 interleved = _get_attrib_interleaved(vertex_attributes, vertex_data);
		//}
		//else
		//{
			interleved = vertex_data;
		//}

		RID vertex_buffer = rendering_device->vertex_buffer_create(interleved.size(), interleved);

		RID index_buffer = rendering_device->index_buffer_create(total_indices, index_data_format, index_data);

		for (auto& p: primitives)
		{
			auto prim = mesh_owner.get_or_null(p.first);
			vertex_arrays.insert({ p.first, rendering_device->vertex_array_create(prim->vertices.size(), vertex_format, {vertex_buffer}, { p.second.vertex_byte_offset  }) });
			index_arrays.insert({ p.first, rendering_device->index_array_create(index_buffer, p.second.indexOffset, p.second.index_count) });
		}
	}

}
