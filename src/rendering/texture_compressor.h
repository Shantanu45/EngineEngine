#pragma once
#include "image_loader.h"
#include "rendering_device.h"
#include <vector>

namespace Rendering {

struct BC7MipChain {
    int  width      = 0;
    int  height     = 0;
    int  mip_levels = 0;
    bool is_srgb    = false;
    std::vector<std::vector<uint8_t>> mips; // one entry per mip level

    bool valid() const { return !mips.empty() && width > 0 && height > 0; }
};

// Compress RGBA8 ImageData to a BC7 mip chain. Generates full mip pyramid via box filter.
BC7MipChain compress_bc7(const ImageData& img, bool is_srgb);

// Serialize/deserialize BC7MipChain as a DX10-extension DDS file.
std::vector<uint8_t> bc7_chain_to_dds(const BC7MipChain& chain);
BC7MipChain          dds_to_bc7_chain(const uint8_t* data, size_t size);

// Upload a BC7MipChain to the GPU. Returns RID.
RID upload_texture_bc7(RenderingDevice* device, const BC7MipChain& chain, const std::string& name = "");

} // namespace Rendering
