#pragma once
#include "application/application.h"
#include "rendering/renderer_compositor.h"
#include "rendering/fg/frame_graph.h"
#include "rendering/fg/blackboard.h"
#include "../utils.h"

using RD = Rendering::RenderingDevice;
using RDC = Rendering::RenderingDeviceCommons;
using RDD = Rendering::RenderingDeviceDriver;


enum TEXTURE_WRITE_FLAGS : uint8_t
{
	WRITE_COLOR,
	WRITE_DEPTH,
	WRITE_COUNT
};

enum TEXTURE_READ_FLAGS : uint8_t
{
	READ_COLOR,
	READ_DEPTH,
	READ_COUNT
};

namespace Rendering
{
	struct RenderContext
	{
		Rendering::RenderingDevice* device;
		RDD::CommandBufferID command_buffer;
		Rendering::WSI* wsi;
	};

	struct FrameGraphTexture {
		struct Desc {
			RDC::TextureFormat  texture_format;
			RD::TextureView     texture_view;
			std::string         texture_name;
			//VkExtent2D          texture_extent;
		};

		RID texture_rid;

		inline void create(const Desc& desc, void* ctx) {
			auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);

			if (texture_rid.is_valid()) {
				assert(false && "already created");
				return;
			}

			texture_rid = device.texture_create(
				desc.texture_format,
				desc.texture_view,
				{}
			);
		}

		inline void destroy(const Desc& desc, void* ctx) {
			auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);
			if (texture_rid.is_null()) {
				assert(false && "already destroyed");
				return;
			}

			device.free_rid(texture_rid);
		}

		inline void pre_read(const Desc& desc, uint32_t flags, void* ctx) {
			auto& rc = *static_cast<RenderContext*>(ctx);

			RDD::TextureBarrier barrier{};
			barrier.texture = rc.device->texture_id_from_rid(texture_rid); 

			if (flags == TEXTURE_READ_FLAGS::READ_COLOR) {
				barrier.src_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.dst_access = RDD::BARRIER_ACCESS_SHADER_READ_BIT;
				barrier.prev_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				barrier.next_layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				rc.device->apply_image_barrier(rc.command_buffer,
					RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					{ &barrier, 1 });
			}
			else if (flags == TEXTURE_READ_FLAGS::READ_DEPTH) {
				barrier.src_access = RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.dst_access = RDD::BARRIER_ACCESS_SHADER_READ_BIT;
				barrier.prev_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				barrier.next_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				barrier.subresources = { RDD::TEXTURE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

				rc.device->apply_image_barrier(rc.command_buffer,
					RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					{ &barrier, 1 });
			}
		}

		inline void pre_write(const Desc& desc, uint32_t flags, void* ctx) {
			auto& rc = *static_cast<RenderContext*>(ctx);

			RDD::TextureBarrier barrier{};
			barrier.texture = rc.device->texture_id_from_rid(texture_rid); 
			barrier.src_access = 0;
			barrier.prev_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;

			if (flags == TEXTURE_WRITE_FLAGS::WRITE_COLOR) {
				barrier.dst_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.next_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				barrier.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				rc.device->apply_image_barrier(rc.command_buffer,
					RDD::PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					{ &barrier, 1 });
			}
			else if (flags == TEXTURE_WRITE_FLAGS::WRITE_DEPTH) {
				barrier.dst_access = RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
					RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.next_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				barrier.subresources = { RDD::TEXTURE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

				rc.device->apply_image_barrier(rc.command_buffer,
					RDD::PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
					RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					{ &barrier, 1 });
			}
		}

		inline static std::string to_string(const Desc& d) {
			return d.texture_name;
		}
	};
}
