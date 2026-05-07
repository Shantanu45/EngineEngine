#include "texture_compressor.h"
#include "util/error_macros.h"

#include <bc7enc.h>
#include <dds_defs.h>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace Rendering {

// --- internal helpers ---

static void init_bc7enc_once() {
    static std::once_flag flag;
    std::call_once(flag, bc7enc_compress_block_init);
}

static void downsample_rgba(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const int sx  = x * 2;
            const int sy  = y * 2;
            const int sx1 = std::min(sx + 1, sw - 1);
            const int sy1 = std::min(sy + 1, sh - 1);
            for (int c = 0; c < 4; ++c) {
                int s = src[(sy  * sw + sx ) * 4 + c]
                      + src[(sy  * sw + sx1) * 4 + c]
                      + src[(sy1 * sw + sx ) * 4 + c]
                      + src[(sy1 * sw + sx1) * 4 + c];
                dst[(y * dw + x) * 4 + c] = uint8_t(s >> 2);
            }
        }
    }
}

static std::vector<uint8_t> compress_mip(const uint8_t* rgba, int w, int h,
                                          const bc7enc_compress_block_params& params) {
    const int bx = (w + 3) / 4;
    const int by = (h + 3) / 4;
    std::vector<uint8_t> blocks(static_cast<size_t>(bx) * by * 16);

    uint8_t tile[16 * 4];
    for (int ty = 0; ty < by; ++ty) {
        for (int tx = 0; tx < bx; ++tx) {
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int sx = std::min(tx * 4 + px, w - 1);
                    const int sy = std::min(ty * 4 + py, h - 1);
                    memcpy(tile + (py * 4 + px) * 4, rgba + (sy * w + sx) * 4, 4);
                }
            }
            bc7enc_compress_block(blocks.data() + (static_cast<size_t>(ty) * bx + tx) * 16,
                                  tile, &params);
        }
    }
    return blocks;
}

// --- public API ---

BC7MipChain compress_bc7(const ImageData& img, bool is_srgb) {
    ERR_FAIL_COND_V_MSG(img.desired_channels != 4, BC7MipChain{},
                        "BC7 compress requires RGBA8 input (4 channels)");
    ERR_FAIL_COND_V_MSG(!img.is_valid(), BC7MipChain{}, "BC7 compress: invalid image");

    init_bc7enc_once();

    bc7enc_compress_block_params params;
    bc7enc_compress_block_params_init(&params);

    BC7MipChain chain;
    chain.width   = img.width;
    chain.height  = img.height;
    chain.is_srgb = is_srgb;

    std::vector<uint8_t> current(img.pixels.begin(), img.pixels.end());
    int w = img.width, h = img.height;

    while (true) {
        chain.mips.push_back(compress_mip(current.data(), w, h, params));
        if (w == 1 && h == 1) break;
        const int nw = std::max(1, w / 2);
        const int nh = std::max(1, h / 2);
        std::vector<uint8_t> next(static_cast<size_t>(nw) * nh * 4);
        downsample_rgba(current.data(), w, h, next.data(), nw, nh);
        current = std::move(next);
        w = nw;
        h = nh;
    }

    chain.mip_levels = static_cast<int>(chain.mips.size());
    return chain;
}

std::vector<uint8_t> bc7_chain_to_dds(const BC7MipChain& chain) {
    ERR_FAIL_COND_V_MSG(!chain.valid(), {}, "bc7_chain_to_dds: invalid chain");

    DDSURFACEDESC2 hdr = {};
    hdr.dwSize        = cDDSSizeofDDSurfaceDesc2;
    hdr.dwFlags       = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                        DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
    hdr.dwHeight      = static_cast<uint32_t>(chain.height);
    hdr.dwWidth       = static_cast<uint32_t>(chain.width);
    hdr.dwLinearSize  = static_cast<uint32_t>(chain.mips[0].size());
    hdr.dwMipMapCount = static_cast<uint32_t>(chain.mip_levels);
    hdr.ddpfPixelFormat.dwSize   = sizeof(DDPIXELFORMAT);
    hdr.ddpfPixelFormat.dwFlags  = DDPF_FOURCC;
    hdr.ddpfPixelFormat.dwFourCC = PIXEL_FMT_FOURCC('D', 'X', '1', '0');
    hdr.ddsCaps.dwCaps           = DDSCAPS_COMPLEX | DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;

    DDS_HEADER_DXT10 dx10 = {};
    dx10.dxgiFormat        = chain.is_srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
    dx10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10.arraySize         = 1;

    size_t total_bc = 0;
    for (const auto& m : chain.mips) total_bc += m.size();

    std::vector<uint8_t> out;
    out.reserve(4 + sizeof(hdr) + sizeof(dx10) + total_bc);

    auto append = [&](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };

    uint32_t magic = cDDSFileSignature;
    append(&magic, 4);
    append(&hdr, sizeof(hdr));
    append(&dx10, sizeof(dx10));
    for (const auto& m : chain.mips) append(m.data(), m.size());
    return out;
}

BC7MipChain dds_to_bc7_chain(const uint8_t* data, size_t size) {
    const size_t min_hdr = 4 + cDDSSizeofDDSurfaceDesc2 + sizeof(DDS_HEADER_DXT10);
    ERR_FAIL_COND_V_MSG(size < min_hdr, BC7MipChain{}, "dds_to_bc7_chain: file too small");

    uint32_t magic;
    memcpy(&magic, data, 4);
    ERR_FAIL_COND_V_MSG(magic != cDDSFileSignature, BC7MipChain{}, "dds_to_bc7_chain: bad magic");

    DDSURFACEDESC2 hdr;
    memcpy(&hdr, data + 4, sizeof(hdr));

    DDS_HEADER_DXT10 dx10;
    memcpy(&dx10, data + 4 + sizeof(hdr), sizeof(dx10));

    ERR_FAIL_COND_V_MSG(dx10.dxgiFormat != DXGI_FORMAT_BC7_UNORM &&
                        dx10.dxgiFormat != DXGI_FORMAT_BC7_UNORM_SRGB,
                        BC7MipChain{}, "dds_to_bc7_chain: not a BC7 DDS");

    BC7MipChain chain;
    chain.width      = static_cast<int>(hdr.dwWidth);
    chain.height     = static_cast<int>(hdr.dwHeight);
    chain.mip_levels = static_cast<int>(hdr.dwMipMapCount);
    chain.is_srgb    = dx10.dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB;

    const uint8_t* ptr = data + 4 + sizeof(hdr) + sizeof(dx10);
    int w = chain.width, h = chain.height;

    for (int m = 0; m < chain.mip_levels; ++m) {
        const int bx       = (w + 3) / 4;
        const int by       = (h + 3) / 4;
        const size_t msz   = static_cast<size_t>(bx) * by * 16;
        ERR_FAIL_COND_V_MSG(ptr + msz > data + size, BC7MipChain{}, "dds_to_bc7_chain: truncated mip data");
        chain.mips.emplace_back(ptr, ptr + msz);
        ptr += msz;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
    return chain;
}

RID upload_texture_bc7(RenderingDevice* device, const BC7MipChain& chain, const std::string& name) {
    ERR_FAIL_COND_V_MSG(!chain.valid(), RID(), "upload_texture_bc7: invalid chain");

    using RDC = RenderingDeviceCommons;
    RDC::TextureFormat tf;
    tf.width        = static_cast<uint32_t>(chain.width);
    tf.height       = static_cast<uint32_t>(chain.height);
    tf.mipmaps      = static_cast<uint32_t>(chain.mip_levels);
    tf.format       = chain.is_srgb ? RDC::DATA_FORMAT_BC7_SRGB_BLOCK
                                    : RDC::DATA_FORMAT_BC7_UNORM_BLOCK;
    tf.texture_type = RDC::TEXTURE_TYPE_2D;
    tf.usage_bits   = RDC::TEXTURE_USAGE_SAMPLING_BIT | RDC::TEXTURE_USAGE_CAN_UPDATE_BIT;

    // texture_create expects data.size() == array_layers (1 for 2D),
    // with all mip levels concatenated into that single slice.
    Util::SmallVector<uint8_t> flat;
    for (const auto& m : chain.mips)
        flat.insert(flat.end(), m.data(), m.data() + m.size());

    Util::SmallVector<Util::SmallVector<uint8_t>> mip_data;
    mip_data.push_back(std::move(flat));

    RID rid = device->texture_create(tf, RenderingDevice::TextureView(), mip_data);
    if (!name.empty())
        device->set_resource_name(rid, name);
    return rid;
}

} // namespace Rendering
