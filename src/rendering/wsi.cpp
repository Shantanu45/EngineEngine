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
#include "rendering/utils.h"

namespace Rendering
{

    WSI::WSI()
    {
        auto event_manager = Services::get().get<EE::EventManager>();
        event_manager->register_handler<WSI, EE::WindowResizeEvent, &WSI::on_resize>(this);
    }

    WSI::~WSI()
    {
    }

    bool WSI::on_resize(const EE::WindowResizeEvent& e)
    {
        rendering_device->on_resize(active_window);
        return true;
    }

    Error WSI::initialize(
        const std::string& p_rendering_driver,
        DisplayServerEnums::WindowMode p_mode,
        DisplayServerEnums::VSyncMode  p_vsync_mode,
        uint32_t                       p_flags,
        const Vector2i* p_position,
        const Vector2i& p_resolution,
        int                            p_screen,
        DisplayServerEnums::Context    p_context,
        int64_t                        p_parent_window)
    {
        active_window = DisplayServerEnums::MAIN_WINDOW_ID;
        rendering_driver = p_rendering_driver;

        if (rendering_driver == "vulkan")
        {
            rendering_context = std::make_unique<Vulkan::RenderingContextDriverVulkan>();
        }
        else
        {
            DEBUG_ASSERT(false, "Unsupported rendering driver");
            return FAILED;
        }

        if (rendering_context != nullptr)
        {
            if (!rendering_context->init_loader_and_extensions(windows[active_window].platfform_data.platform))
            {
                LOGE("Failed to initialize Vulkan loader.\n");
                return FAILED;
            }

            if (rendering_context->initialize() == OK && _create_rendering_context_window(active_window, rendering_driver) == OK)
            {
                rendering_context->window_set_vsync_mode(active_window, DisplayServerEnums::VSYNC_DISABLED);
                rendering_device = RenderingDevice::get_singleton();

                if (rendering_device->initialize(rendering_context.get(), active_window) != OK)
                    return FAILED;
            }

            // Mesh subsystem
            //auto fs = Services::get().get<FilesystemInterface>();
            //mesh_storage = std::make_unique<MeshStorage>();
            ///mesh_storage->initialize(rendering_device);
            //mesh_loader = std::make_unique<MeshLoader>(*fs);
        }

        return OK;
    }

    void WSI::poll(void* e)
    {
        rendering_device->on_poll(e);
    }

    void WSI::blit_render_target_to_screen(RID p_scene_texture, RID p_imgui_ui_tex)
    {
        if (rd->is_blit_pass_active())
        {
            Rendering::BlitToScreen blit;
            blit.render_target = p_scene_texture;
            blit.ui = p_imgui_ui_tex;
            rd->blit_render_targets_to_screen(&blit);
        }
    }

    bool WSI::pre_frame_loop()
    {
        if (!rendering_context || !rendering_device)
            return false;

        ERR_FAIL_COND_V_MSG(rendering_device == nullptr, false, "Rendering device invalid.");

        rendering_device->screen_create(active_window);

        if (imgui_active)
        {
            ERR_FAIL_COND_V_MSG(
                rendering_device->iniitialize_imgui_device(get_wsi_platform_data(0).platfform_data) != OK,
                false, "imgui is set to active but could not initialize");
        }

        rd = std::make_unique<RendererCompositor>();
        rd->initailize(DisplayServerEnums::MAIN_WINDOW_ID);
        rendering_device->begin_frame();

        return true;
    }

    bool WSI::pre_begin_frame()
    {
        if (RenderUtilities::get_captured_timestamps_count())
        {
            std::vector<RenderUtilities::FrameProfileArea> new_profile;
            if (RenderUtilities::capturing_timestamps)
                new_profile.resize(RenderUtilities::get_captured_timestamps_count());

            uint64_t base_cpu = RenderUtilities::get_captured_timestamp_cpu_time(0);
            uint64_t base_gpu = RenderUtilities::get_captured_timestamp_gpu_time(0);

            for (uint32_t i = 0; i < RenderUtilities::get_captured_timestamps_count(); i++)
            {
                uint64_t time_cpu = RenderUtilities::get_captured_timestamp_cpu_time(i);
                uint64_t time_gpu = RenderUtilities::get_captured_timestamp_gpu_time(i);

                if (RenderUtilities::capturing_timestamps)
                {
                    new_profile[i].gpu_msec = double((time_gpu - base_gpu) / 1000) / 1000.0;
                    new_profile[i].cpu_msec = double(time_cpu - base_cpu) / 1000.0;
                    new_profile[i].name = RenderUtilities::get_captured_timestamp_name(i);
                }
            }

            frame_profile = new_profile;

            for (int i = 0; i < (int)frame_profile.size() - 1; i++)
            {
                const std::string& name = frame_profile[i].name;
                double time = frame_profile[i + 1].gpu_msec - frame_profile[i].gpu_msec;

                gpu_profile_task_time[name] += time;
                cpu_profile_task_time[name] += time;
            }
        }

        return true;
    }

    double WSI::get_gpu_frame_time()
    {
        ERR_FAIL_COND_V_MSG((!RenderUtilities::capturing_timestamps) || (frame_profile.size() < 2), 0.0,
            "frame profile does not have enough values!");
        return frame_profile[1].gpu_msec - frame_profile[0].gpu_msec;
    }

    double WSI::get_cpu_frame_time()
    {
        ERR_FAIL_COND_V_MSG((!RenderUtilities::capturing_timestamps) || (frame_profile.size() < 2), 0.0,
            "frame profile does not have enough values!");
        return frame_profile[1].cpu_msec - frame_profile[0].cpu_msec;
    }

    bool WSI::begin_frame()
    {
        rendering_device->_submit_transfer_barriers(rendering_device->get_current_command_buffer());
        return true;
    }

    bool WSI::end_render_pass(RDD::CommandBufferID p_cmd)
    {
        rendering_device->end_render_pass(p_cmd);
        return true;
    }

    bool WSI::end_frame(bool p_present)
    {
        rd->end_frame(p_present);
        return true;
    }

    bool WSI::post_end_frame()
    {
        return false;
    }

    bool WSI::post_frame_loop()
    {
        return false;
    }

    // -- Mesh ---------------------------------------------------------------------

    /*void WSI::draw_mesh(RenderingDeviceDriver::CommandBufferID p_cmd, MeshHandle p_handle)
    {
        const MeshGPU* mesh = mesh_storage->get(p_handle);
        if (!mesh)
        {
            LOGE("WSI::draw_mesh - invalid handle");
            return;
        }

        for (auto& prim : mesh->primitives)
        {
            rendering_device->bind_vertex_array(prim.vertex_array);
            rendering_device->bind_index_array(prim.index_array);
            rendering_device->render_draw_indexed(p_cmd, prim.index_count, 1, 0, 0, 0);
        }
    }

    void WSI::draw_mesh(RenderingDeviceDriver::CommandBufferID p_cmd, const std::string& p_name)
    {
        const MeshGPU* mesh = mesh_storage->get(p_name);
        if (!mesh)
        {
            LOGE("WSI::draw_mesh - mesh '{}' not found", p_name);
            return;
        }

        for (auto& prim : mesh->primitives)
        {
            rendering_device->bind_vertex_array(prim.vertex_array);
            rendering_device->bind_index_array(prim.index_array);
            rendering_device->render_draw_indexed(p_cmd, prim.index_count, 1, 0, 0, 0);
        }
    }*/

    // -- Vertex format -------------------------------------------------------------

    void WSI::set_wsi_platform_data(DisplayServerEnums::WindowID p_window, WindowData p_data)
    {
        windows.insert({ p_window, p_data });
    }

    RenderingDeviceCommons::VertexAttribute WSI::get_vertex_attribute(
        uint32_t p_binding, uint32_t p_location,
        RenderingDeviceCommons::DataFormat p_format,
        uint32_t p_offset, uint32_t p_stride)
    {
        RenderingDeviceCommons::VertexAttribute va;
        va.format = p_format;
        va.stride = p_stride;
        va.binding = p_binding;
        va.location = p_location;
        va.offset = p_offset;
        return va;
    }

    void WSI::create_new_vertex_format(const std::vector<RenderingDeviceCommons::VertexAttribute>& p_attributes, VERTEX_FORMAT_VARIATIONS p_type)
    {
        DEBUG_ASSERT(p_type < VERTEX_FORMAT_VARIATIONS::COUNT);
        vertex_format_map[p_type] = rendering_device->vertex_format_create(p_attributes);
    }

    RenderingDevice::VertexFormatID WSI::get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS p_type)
    {
        DEBUG_ASSERT(vertex_format_map.contains(p_type), "type does not exist, call create_new_vertex_format() first");
        return vertex_format_map[p_type];
    }

    std::vector<RenderingDeviceCommons::VertexAttribute> WSI::get_default_vertex_attribute()
    {
        std::vector<RenderingDeviceCommons::VertexAttribute> attrs;
        attrs.emplace_back(get_vertex_attribute(0, 0, RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position), sizeof(Vertex)));
        attrs.emplace_back(get_vertex_attribute(0, 1, RenderingDeviceCommons::DATA_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal), sizeof(Vertex)));
        attrs.emplace_back(get_vertex_attribute(0, 2, RenderingDeviceCommons::DATA_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texcoord), sizeof(Vertex)));
        attrs.emplace_back(get_vertex_attribute(0, 3, RenderingDeviceCommons::DATA_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent), sizeof(Vertex)));
        return attrs;
    }

    void WSI::set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat p_format)
    {
        index_data_format = p_format;
    }

    void WSI::submit_transfer_workers()
    {
        rendering_device->_submit_transfer_workers();
    }

    RenderingShaderContainerFormat* WSI::create_shader_container_format()
    {
        return new ::Vulkan::RenderingShaderContainerFormatVulkan();
    }

    void WSI::teardown()
    {
        // Destroy compositor first — its RIDHandle members call free_rid in their destructors,
        // which must happen before the device is finalized.
        rd.reset();

        // MeshStorage handles all GPU resource cleanup
        //mesh_storage->finalize();

        rendering_device->finalize();
    }

    Error WSI::_create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver)
    {
        const auto wd = windows.at(p_window_id);

        Error err = rendering_context->window_create(p_window_id, &wd.platfform_data);
        ERR_FAIL_COND_V_MSG(err != OK, err, std::format("Failed to create {} window.", p_rendering_driver));
        rendering_context->window_set_size(p_window_id, wd.window_resolution.x, wd.window_resolution.y);
        return OK;
    }

    std::vector<uint8_t> WSI::_get_attrib_interleaved(
        const std::vector<RenderingDeviceCommons::VertexAttribute>& p_attribs,
        const std::vector<uint8_t>& p_vertex_data)
    {
        std::vector<uint8_t> interleaved_data;
        const uint32_t vert_num = p_vertex_data.size() / p_attribs[0].stride;
        const uint32_t stride = p_attribs[0].stride;
        interleaved_data.resize(p_vertex_data.size());

        for (uint32_t v = 0; v < vert_num; v++)
        {
            uint32_t src_attrib_offset = 0;

            for (int i = 0; i < (int)p_attribs.size() - 1; i++)
            {
                const uint32_t size = p_attribs[i + 1].offset - p_attribs[i].offset;
                const uint32_t dst_offset = (v * stride) + p_attribs[i].offset;
                const uint32_t src_offset = src_attrib_offset + (v * size);

                std::memcpy(interleaved_data.data() + dst_offset, p_vertex_data.data() + src_offset, size);
                src_attrib_offset += size * vert_num;
            }

            const uint32_t last_size = p_attribs.back().stride - p_attribs.back().offset;
            const uint32_t dst_offset = (v * stride) + p_attribs.back().offset;
            const uint32_t src_offset = src_attrib_offset + (v * last_size);

            std::memcpy(interleaved_data.data() + dst_offset, p_vertex_data.data() + src_offset, last_size);
        }

        return interleaved_data;
    }

} // namespace Rendering