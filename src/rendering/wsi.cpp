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
			// TODO: my be move swap chain creation to the renderer compositor?
			rendering_device->screen_create(active_window);
			rd = std::make_unique<RendererCompositor>();
			rd->initailize(DisplayServerEnums::MAIN_WINDOW_ID);
			rendering_device->begin_frame();

			return true;
		}
		return false;
	}

	void WSI::blit_render_target_to_screen(RID texture)
	{
		if (rd->is_blit_pass_active())
		{
			Rendering::BlitToScreen blit;
			blit.render_target = texture;

			rd->blit_render_targets_to_screen(&blit);
		}
	}

	bool WSI::pre_begin_frame()
	{
		return true;
	}

	bool WSI::begin_frame()
	{

		rendering_device->_submit_transfer_barriers(rendering_device->get_current_command_buffer());

		//rendering_device->screen_prepare_for_drawing(active_window);

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
		if (rd->is_blit_pass_active())
		{
			rd->end_frame(true);
		}
		else
		{
			// TODO: logic if not presenting on screen
		}
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

	RenderingDeviceCommons::VertexAttribute WSI::get_vertex_attribute(const uint32_t binding, const uint32_t location, const RenderingDeviceCommons::DataFormat format, const uint32_t offset, const uint32_t stride)
	{
		RenderingDeviceCommons::VertexAttribute va;
		va.format = format;
		va.stride = stride;
		va.binding = binding;
		va.location = location;
		va.offset = offset;
		return va;
	}

	RenderingShaderContainerFormat* WSI::create_shader_container_format() 
	{
		return new ::Vulkan::RenderingShaderContainerFormatVulkan();
	}

	void WSI::bind_and_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer, const std::string& p_mesh_name)
	{
		auto primitives = mesh_list[p_mesh_name].primitives;
		for (auto& p : primitives)
		{
			rendering_device->bind_vertex_array(p.second.vertex_array);
			rendering_device->bind_index_array(p.second.index_array);
			rendering_device->render_draw_indexed(p_command_buffer, p.second.index_count, 1, 0, 0, 0);
		}
	}

	void WSI::set_wsi_platform_data(DisplayServerEnums::WindowID window, WindowData data)
	{
		windows.insert({ window, data });
	}

	void WSI::create_new_vertex_format(const std::vector<RenderingDeviceCommons::VertexAttribute>& p_attributes, VERTEX_FORMAT_VARIATIONS p_type)
	{
		DEBUG_ASSERT(p_type < VERTEX_FORMAT_VARIATIONS::COUNT);
		vertex_format_map[p_type] = rendering_device->vertex_format_create(p_attributes);
	}

	Rendering::RenderingDevice::VertexFormatID WSI::get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS p_type)
	{
		DEBUG_ASSERT(vertex_format_map.contains(p_type),  "type does not exists, create vertex format via create_new_vertex_format() first");
		return vertex_format_map[p_type];
	}

	Error WSI::load_gltf(const std::string& p_path, const std::string& p_name, VERTEX_FORMAT_VARIATIONS p_type /*= VERTEX_FORMAT_VARIATIONS::DEFAULT*/)
	{
		if (gltf_loader->load(p_path) != OK)
			return ERR_FILE_NOT_FOUND;

		RenderingDevice::VertexFormatID vertex_format = get_vertex_format_by_type(p_type);

		uint32_t total_vertices = 0;
		uint32_t total_indices = 0;

		std::vector<uint8_t> vertex_data{};
		std::vector<uint8_t> index_data{};

		MeshData mesh_data;

		auto prims = gltf_loader->primitives();
		for (auto p : prims)
		{
			PrimitiveData range = { total_vertices, total_vertices * sizeof(Rendering::Vertex), total_indices, (uint32_t)p.indices.size() };
			mesh_data.primitives[mesh_owner.make_rid(p)] = range;

			uint64_t vbSize = p.vertices.size() * sizeof(Rendering::Vertex);
			uint64_t ibSize = p.indices.size() * ((index_data_format == Rendering::RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT16)  ?  sizeof(uint16_t) : sizeof(uint32_t));

			// add to buffer
			const size_t vb_offset = vertex_data.size();
			vertex_data.resize(vb_offset + vbSize);
			memcpy(vertex_data.data() + vb_offset, p.vertices.data(), vbSize);

			const size_t ib_offset = index_data.size();
			index_data.resize(ib_offset + ibSize);
			memcpy(index_data.data() + ib_offset, p.indices.data(), ibSize);

			total_vertices += p.vertices.size();
			total_indices += p.indices.size();
		}
		// TODO: should not stay here
		/*create_vertex_format({});*/


		DEBUG_ASSERT(mesh_data.primitives.size() > 0);
		DEBUG_ASSERT(!vertex_data.empty());

		RID vertex_buffer = rendering_device->vertex_buffer_create(vertex_data.size(), vertex_data);

		RID index_buffer = rendering_device->index_buffer_create(total_indices, index_data_format, index_data);

		for (auto& p : mesh_data.primitives)
		{
			auto prim = mesh_owner.get_or_null(p.first);
			p.second.vertex_array = rendering_device->vertex_array_create(prim->vertices.size(), vertex_format, { vertex_buffer }, { p.second.vertex_byte_offset });
			p.second.index_array = rendering_device->index_array_create(index_buffer, p.second.indexOffset, p.second.index_count);
		}

		mesh_list[p_name] = mesh_data;
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

	void WSI::submit_transfer_workers()
	{
		rendering_device->_submit_transfer_workers();
	}

	void WSI::set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat format)
	{
		index_data_format = format;
	}

	void WSI::teardown()
	{
		rd->finalize();
		for (auto m : mesh_list)
		{
			for (auto p : m.second.primitives)
			{
				rendering_device->_free_dependencies_of(p.second.vertex_array);
				rendering_device->_free_dependencies_of(p.second.index_array);
			}
			rendering_device->free_rid(m.second.primitives.begin()->second.vertex_array);
			rendering_device->free_rid(m.second.primitives.begin()->second.index_array);
		}
		//rendering_device->screen_free();
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
		//surface = rendering_context->surface_get_from_window(p_window_id);
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


}
