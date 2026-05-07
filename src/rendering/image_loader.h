/*****************************************************************//**
 * \file   image_loader.h
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "util/error_macros.h"
#include "util/small_vector.h"
#include "filesystem/filesystem.h"
#include "filesystem/path_utils.h"
#include "rendering_device.h"

//// stb_image
//#ifndef STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#endif
#include <stb_image.h>

namespace Rendering
{
	enum class Format
	{
		Unknown = 0,
		R8_UNORM,
		R8G8_UNORM,
		R8G8B8_UNORM,
		R8G8B8A8_UNORM
	};

	struct ImageData
	{
		int width = 0;
		int height = 0;
		int channels = 0;        // original channels
		int desired_channels = 0; // forced channels (e.g. 4)

		Format format = Format::Unknown;

		Util::SmallVector<uint8_t> pixels;

		bool is_valid() const
		{
			return !pixels.empty() && width > 0 && height > 0;
		}

		size_t get_pixel_size() const
		{
			return desired_channels > 0 ? desired_channels : channels;
		}

		size_t get_image_size_bytes() const
		{
			return static_cast<size_t>(width) *
				static_cast<size_t>(height) *
				get_pixel_size();
		}
	};
	// Upload an ImageData as a plain 2D sampled texture.
	// format is the only thing that varies per-texture (SRGB for colour, UNORM for normal/spec maps).
	inline RID upload_texture_2d(
		RenderingDevice* device,
		const ImageData& img,
		RenderingDeviceCommons::DataFormat format,
		const std::string& name = "")
	{
		RenderingDeviceCommons::TextureFormat tf;
		tf.width        = img.width;
		tf.height       = img.height;
		tf.format       = format;
		tf.texture_type = RenderingDeviceCommons::TEXTURE_TYPE_2D;
		tf.usage_bits   = RenderingDeviceCommons::TEXTURE_USAGE_SAMPLING_BIT
		                | RenderingDeviceCommons::TEXTURE_USAGE_CAN_UPDATE_BIT;

		RID rid = device->texture_create(tf, RenderingDevice::TextureView(), { img.pixels });
		if (!name.empty())
			device->set_resource_name(rid, name);
		return rid;
	}

	class ImageLoader {
	public:
		ImageLoader(::FileSystem::FilesystemInterface& iface);

		Format get_format_from_channels(int channels);

		ImageData load_from_file(const std::string& path, int forceChannels = 4);

		void flip_vertical(ImageData& image);

		inline bool has_alpha(const ImageData& image)
		{
			return image.get_pixel_size() == 4;
		}

		void convert_rgb_to_rgba(ImageData& image);

		size_t calculate_mip_levels(int width, int height);

		private:
			FileSystem::FilesystemInterface& fs_iface;
	};
}
