#pragma once
#include "application/application.h"
#include "rendering/renderer_compositor.h"
#include "rendering/fg/frame_graph.h"
#include "rendering/fg/blackboard.h"

using RD = Rendering::RenderingDevice;
using RDC = Rendering::RenderingDeviceCommons;
using RDD = Rendering::RenderingDeviceDriver;


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
			RDC::TextureFormat texture_format;
			const RD::TextureView texture_view;
			std::string texture_name;
		};

		inline void create(const Desc& desc, void* ctx) {
			auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);
			texture = device.texture_create(desc.texture_format, desc.texture_view, {});
		}

		inline void destroy(const Desc& desc, void* ctx) {
			auto& device = *static_cast<Rendering::RenderingDevice*>(ctx);
			device.free_rid(texture);
		}

		inline void pre_read(const Desc& desc, uint32_t flags, void* ctx) {
			auto& rc = *static_cast<RenderContext*>(ctx);

			RDD::TextureBarrier barrier2;
			barrier2.src_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier2.dst_access = RDD::BARRIER_ACCESS_SHADER_READ_BIT;
			barrier2.next_layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier2.prev_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier2.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier2.texture = rc.device->texture_id_from_rid(texture);
			rc.device->apply_image_barrier(rc.command_buffer, RDD::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				RDD::PipelineStageBits::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { &barrier2, 1 });
		}

		inline void pre_write(const Desc& desc, uint32_t flags, void* ctx) {
			auto& rc = *static_cast<RenderContext*>(ctx);

			RDD::TextureBarrier barrier;
			barrier.src_access = 0;
			barrier.dst_access = RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.prev_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			barrier.next_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.subresources = { RDD::TEXTURE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier.texture = rc.device->texture_id_from_rid(texture);
			rc.device->apply_image_barrier(rc.command_buffer, RDD::PipelineStageBits::PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				RDD::PipelineStageBits::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, { &barrier, 1 });
		}

		// to_string is used by the Graphviz dot exporter
		inline static std::string to_string(const Desc& d) {
			return d.texture_name;
		}

		RID texture;
	};
}
