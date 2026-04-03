/*****************************************************************//**
 * \file   wsi.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma  once

#include <map>
#include "rendering_device_driver.h"
#include "rendering_device.h"
#include "math/rect2.h"
#include "rendering/gltf_loader.h"
#include "rendering/utils.h"
#include "application/application_events.h"

namespace Rendering
{
	class RendererCompositor;

	// display server windows
	struct WindowData {
		WindowPlatformData platfform_data;
		Size2i window_resolution;
	};

	enum class VERTEX_FORMAT_VARIATIONS
	{
		DEFAULT,
		COUNT
	};

	class WSI : public EE::EventHandler
	{
		struct PrimitiveData {
			uint32_t vertexOffset;  // offset into the big vertex buffer
			uint64_t vertex_byte_offset;
			uint32_t indexOffset;   // offset into the big index buffer
			uint32_t index_count;

			RID vertex_array;
			RID index_array;
		};

		struct MeshData {
			std::unordered_map<RID, PrimitiveData> primitives;
		};

	public:
		enum class VERTEX_DATA_MODE
		{
			INTERLEVED_DATA,
			SEPERATE
		};
		
		WSI();

		bool on_resize(const EE::WindowResizeEvent& e);

		Error initialize(const std::string& p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i* p_position, const Vector2i& p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window);

		void poll(void* e);

		void blit_render_target_to_screen(RID p_scene_texture, RID p_imgui_ui_tex);

		bool pre_frame_loop();

		bool pre_begin_frame();

		double get_gpu_frame_time();
		double get_cpu_frame_time();
		bool begin_frame();

		bool end_render_pass(RDD::CommandBufferID p_cmd);

		bool end_frame(bool p_present);

		bool post_end_frame();

		bool post_frame_loop();

		void bind_and_draw_indexed(RenderingDeviceDriver::CommandBufferID p_command_buffer, const std::string& p_mesh_name);
		RenderingDevice* get_rendering_device() { return rendering_device; }

		void set_wsi_platform_data(DisplayServerEnums::WindowID p_window, WindowData p_data);
		WindowData get_wsi_platform_data(DisplayServerEnums::WindowID window)
		{
			return windows[window];
		}

		RenderingDeviceCommons::VertexAttribute get_vertex_attribute(const uint32_t p_binding, const uint32_t p_location, const RenderingDeviceCommons::DataFormat p_format, const uint32_t p_offset, const uint32_t p_stride);

		void create_new_vertex_format(const std::vector<RenderingDeviceCommons::VertexAttribute>& p_attributes, VERTEX_FORMAT_VARIATIONS p_type);

		RenderingDevice::VertexFormatID get_vertex_format_by_type(VERTEX_FORMAT_VARIATIONS p_type);

		std::vector<RenderingDeviceCommons::VertexAttribute> get_default_vertex_attribute();

		inline void set_vertex_data_mode(VERTEX_DATA_MODE p_mode)
		{
			vertex_data_mode = p_mode;
		};

		void set_index_buffer_format(RenderingDeviceCommons::IndexBufferFormat p_format);

		void submit_transfer_workers();

		Error load_gltf(const std::string& p_path, const std::string& p_name, VERTEX_FORMAT_VARIATIONS p_type = VERTEX_FORMAT_VARIATIONS::DEFAULT);

		RenderingShaderContainerFormat* create_shader_container_format();

		void teardown();

		~WSI();

		bool imgui_active = true;

	private:

		Error _create_rendering_context_window(DisplayServerEnums::WindowID p_window_id, const std::string& p_rendering_driver = "vulkan");
		void _destroy_rendering_context_window(DisplayServerEnums::WindowID p_window_id);

		void _free_pending_resources(int p_frame);
		std::vector<uint8_t> _get_attrib_interleaved(const std::vector<RenderingDeviceCommons::VertexAttribute>& p_attribs, const std::vector<uint8_t>& p_vertex_data);

	private:

		std::unique_ptr<RenderingContextDriver> rendering_context = nullptr;
		RenderingDevice* rendering_device = nullptr;

		std::map<DisplayServerEnums::WindowID, WindowData> windows;

		std::string rendering_driver;
		bool main_window_created = false;

		DisplayServerEnums::WindowID active_window = DisplayServerEnums::INVALID_WINDOW_ID;

		VERTEX_DATA_MODE vertex_data_mode = VERTEX_DATA_MODE::INTERLEVED_DATA;

		RenderingDeviceCommons::IndexBufferFormat index_data_format = RenderingDeviceCommons::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT32;

		RID_Owner<MeshPrimitive, true> mesh_owner;

		std::unordered_map<std::string, MeshData> mesh_list;

		std::unique_ptr<GltfLoader> gltf_loader = nullptr;

		std::unique_ptr<RendererCompositor> rd;

		std::unordered_map<VERTEX_FORMAT_VARIATIONS, RenderingDevice::VertexFormatID> vertex_format_map;

		std::vector<RenderUtilities::FrameProfileArea> frame_profile;
		uint64_t frame_profile_frame = 0;

		std::unordered_map<std::string, float> gpu_profile_task_time;
		std::unordered_map<std::string, float> cpu_profile_task_time;

	};
}
