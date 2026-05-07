#include "image_loader.h"
#include "util/error_macros.h"
#include "util/small_vector.h"

namespace Rendering
{

	ImageLoader::ImageLoader(::FileSystem::FilesystemInterface& iface): fs_iface(iface)
	{

	}

	Rendering::Format ImageLoader::get_format_from_channels(int channels)
	{
		switch (channels)
		{
		case 1: return Format::R8_UNORM;
		case 2: return Format::R8G8_UNORM;
		case 3: return Format::R8G8B8_UNORM;
		case 4: return Format::R8G8B8A8_UNORM;
		default: return Format::Unknown;
		}
	}


	Rendering::ImageData ImageLoader::load_from_file(const std::string& path, int forceChannels /*= 4*/)
	{
		ImageData result{};

		int width, height, channels;

		const auto actual_path = fs_iface.get_filesystem_path(path);

		stbi_uc* data = stbi_load(
			actual_path.c_str(),
			&width,
			&height,
			&channels,
			forceChannels
		);

		ERR_FAIL_COND_V_MSG(!data, ImageData(), "Failed to load image");

		result.width = width;
		result.height = height;
		result.channels = channels;
		result.desired_channels = forceChannels > 0 ? forceChannels : channels;

		int finalChannels = result.desired_channels;
		result.format = get_format_from_channels(finalChannels);

		size_t size = static_cast<size_t>(width) *
			static_cast<size_t>(height) *
			finalChannels;

		result.pixels.resize(size);
		memcpy(result.pixels.data(), data, size);

		stbi_image_free(data);

		return result;
	}


	void ImageLoader::flip_vertical(ImageData& image)
	{
		const int row_size = image.width * image.get_pixel_size();
		Util::SmallVector<uint8_t> temp(row_size);

		for (int y = 0; y < image.height / 2; ++y)
		{
			uint8_t* rowTop = image.pixels.data() + y * row_size;
			uint8_t* rowBottom = image.pixels.data() + (image.height - 1 - y) * row_size;

			memcpy(temp.data(), rowTop, row_size);
			memcpy(rowTop, rowBottom, row_size);
			memcpy(rowBottom, temp.data(), row_size);
		}
	}


	void ImageLoader::convert_rgb_to_rgba(ImageData& image)
	{
		if (image.get_pixel_size() != 3)
			return;

		Util::SmallVector<uint8_t> new_pixels;
		new_pixels.resize(static_cast<size_t>(image.width) *
			image.height * 4);

		for (int i = 0, j = 0; i < image.width * image.height * 3; i += 3, j += 4)
		{
			new_pixels[j + 0] = image.pixels[i + 0];
			new_pixels[j + 1] = image.pixels[i + 1];
			new_pixels[j + 2] = image.pixels[i + 2];
			new_pixels[j + 3] = 255; // opaque alpha
		}

		image.pixels = std::move(new_pixels);
		image.desired_channels = 4;
		image.format = Format::R8G8B8A8_UNORM;
	}


	size_t ImageLoader::calculate_mip_levels(int width, int height)
	{
		size_t levels = 1;
		while (width > 1 || height > 1)
		{
			width = width > 1 ? width / 2 : 1;
			height = height > 1 ? height / 2 : 1;
			levels++;
		}
		return levels;
	}

}
