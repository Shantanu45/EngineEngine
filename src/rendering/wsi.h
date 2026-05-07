/*****************************************************************//**
 * \file   wsi.h
 * \brief
 *
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once

#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"
#include "math/rect2.h"
#include "rendering/mesh_storage.h"
#include "rendering/mesh_loader.h"
#include "rendering/utils.h"
#include "application/application_events.h"
#include "util/small_vector.h"

namespace Rendering
{
    class RendererCompositor;

    struct WindowData
    {
        WindowPlatformData platfform_data;
        Size2i             window_resolution;
    };

    enum class VERTEX_FORMAT_VARIATIONS
    {
        DEFAULT,
        COUNT
    };

    class WSI : public EE::EventHandler
    {
    public:
        enum class VERTEX_DATA_MODE
        {
            INTERLEVED_DATA,
            SEPERATE
        };

        WSI();
        ~WSI();

        bool on_resize(const EE::WindowResizeEvent& e);

        Error initialize(
            const std::string& p_rendering_driver,
            DisplayServerEnums::WindowMode p_mode,
            DisplayServerEnums::VSyncMode  p_vsync_mode,
            uint32_t                       p_flags,
            const Vector2i* p_position,
            const Vector2i& p_resolution,
            int                            p_screen,
            DisplayServerEnums::Context    p_context,
            int64_t                        p_parent_window);

        void poll(void* e);

        void blit_render_target_to_screen(RID p_scene_texture, RID p_imgui_ui_tex);

        bool pre_frame_loop();
        bool pre_begin_frame();
        bool begin_frame();
        bool end_render_pass(RDD::CommandBufferID p_cmd);
        bool end_frame(bool p_present);
        bool post_end_frame();
        bool post_frame_loop();

        double get_gpu_frame_time();
        double get_cpu_frame_time();
        bool   has_timing_data() const { return frame_profile.size() >= 2; }

        // -- Mesh API --------------------------------------------------
        // Load a gltf/glb into MeshStorage. Returns INVALID_MESH on failure.

        // Draw all primitives of a mesh by handle or name
        //void draw_mesh(RenderingDeviceDriver::CommandBufferID p_cmd, MeshHandle p_handle);
        //void draw_mesh(RenderingDeviceDriver::CommandBufferID p_cmd, const std::string& p_name);

        //MeshStorage* get_mesh_storage() { return mesh_storage.get(); }
        // --------------------------------------------------------------

        // -- Vertex format API -----------------------------------------
        void create_new_vertex_format(const Util::SmallVector<RenderingDeviceCommons::VertexAttribute>& p_attributes,
            VERTEX_FORMAT_VARIATIONS p_type);
        RenderingDevice::VertexFormatID get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS p_type);
        Util::SmallVector<RenderingDeviceCommons::VertexAttribute> get_default_vertex_attribute();
        RenderingDeviceCommons::VertexAttribute get_vertex_attribute(uint32_t p_binding, 
            uint32_t p_location, RenderingDeviceCommons::DataFormat p_format, uint32_t p_offset, uint32_t p_stride);
        // --------------------------------------------------------------

        RenderingDevice* get_rendering_device() { return rendering_device; }

        void set_wsi_platform_data(DisplayServerEnums::WindowID p_window, WindowData p_data);
        WindowData get_wsi_platform_data(DisplayServerEnums::WindowID p_window) { return windows[p_window]; }

        void set_vertex_data_mode(VERTEX_DATA_MODE p_mode) { vertex_data_mode = p_mode; }
        void set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat p_format);

        void submit_transfer_workers();

        RenderingShaderContainerFormat* create_shader_container_format();

        void teardown();

        bool imgui_active = true;

    private:
        Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, 
            const std::string& p_rendering_driver = "vulkan");
        void  _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);
        void  _free_pending_resources(int p_frame);
        Util::SmallVector<uint8_t> _get_attrib_interleaved(const Util::SmallVector<RenderingDeviceCommons::VertexAttribute>& p_attribs,
            const Util::SmallVector<uint8_t>& p_vertex_data);

    private:
        std::unique_ptr<RenderingContextDriver> rendering_context = nullptr;
        RenderingDevice* rendering_device = nullptr;

        std::map<DisplayServerEnums::WindowID, WindowData> windows;

        std::string                  rendering_driver;
        bool                         main_window_created = false;
        DisplayServerEnums::WindowID active_window = DisplayServerEnums::INVALID_WINDOW_ID;

        VERTEX_DATA_MODE vertex_data_mode = VERTEX_DATA_MODE::INTERLEVED_DATA;
        RenderingDeviceCommons::IndexBufferFormat index_data_format = RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32;

        // -- Mesh ------------------------------------------------------
        // --------------------------------------------------------------

        std::unique_ptr<RendererCompositor> rd;

        std::unordered_map<VERTEX_FORMAT_VARIATIONS, RenderingDevice::VertexFormatID> vertex_format_map;

        Util::SmallVector<RenderUtilities::FrameProfileArea> frame_profile;
        uint64_t frame_profile_frame = 0;

        std::unordered_map<std::string, float> gpu_profile_task_time;
        std::unordered_map<std::string, float> cpu_profile_task_time;
    };

} // namespace Rendering