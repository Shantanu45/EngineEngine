#pragma once
#include "rendering/image_loader.h"
#include "rendering/texture_compressor.h"
#include "filesystem/filesystem.h"
#include <map>
#include <utility>

namespace Rendering {

class TextureCache {
public:
    // fs must be the full Filesystem (not just FilesystemInterface) for disk cache I/O.
    // bc7_cache: when true, textures are BC7-compressed and cached under cache://.
    void init(RenderingDevice* device, FileSystem::Filesystem& fs, bool bc7_cache = true) {
        device_    = device;
        fs_        = &fs;
        bc7_cache_ = bc7_cache;
        loader_    = std::make_unique<ImageLoader>(fs);
    }

    // Load from disk, compress, and cache.
    RID color(const std::string& path)  { return get(path, /*is_srgb=*/true);  }
    RID linear(const std::string& path) { return get(path, /*is_srgb=*/false); }

    // Upload pre-decoded RGBA8 ImageData. cache_key identifies the DDS cache file.
    // Use for GLTF images already decoded in memory (e.g. GltfImageData::pixels).
    RID raw(const std::string& cache_key, const ImageData& img, bool is_srgb) {
        auto key = std::make_pair(cache_key, is_srgb);
        auto it  = cache_.find(key);
        if (it != cache_.end()) return it->second;
        return cache_[key] = compress_and_upload(cache_key, img, is_srgb);
    }

    void free_all() {
        for (auto& [key, rid] : cache_)
            device_->free_rid(rid);
        cache_.clear();
    }

private:
    RID get(const std::string& path, bool is_srgb) {
        auto key = std::make_pair(path, is_srgb);
        auto it  = cache_.find(key);
        if (it != cache_.end()) return it->second;

        if (bc7_cache_) {
            // Check disk cache before loading the source image.
            const std::string cache_path = make_cache_path(path, is_srgb);
            FileSystem::FileStat st;
            if (fs_->stat(cache_path, st)) {
                auto mapping = fs_->open_readonly_mapping(cache_path);
                if (mapping) {
                    auto chain = dds_to_bc7_chain(mapping->data<uint8_t>(),
                                                   static_cast<size_t>(mapping->get_size()));
                    if (chain.valid())
                        return cache_[key] = upload_texture_bc7(device_, chain, path);
                }
            }
        }

        auto img = loader_->load_from_file(path);
        return cache_[key] = compress_and_upload(path, img, is_srgb);
    }

    // Shared path: check disk cache, then compress + save + upload if miss.
    RID compress_and_upload(const std::string& cache_key, const ImageData& img, bool is_srgb) {
        if (!bc7_cache_) {
            auto fmt = is_srgb ? RDC::DATA_FORMAT_R8G8B8A8_SRGB
                               : RDC::DATA_FORMAT_R8G8B8A8_UNORM;
            return upload_texture_2d(device_, img, fmt, cache_key);
        }

        const std::string cache_path = make_cache_path(cache_key, is_srgb);

        // Disk cache hit — load DDS, skip compression.
        FileSystem::FileStat st;
        if (fs_->stat(cache_path, st)) {
            auto mapping = fs_->open_readonly_mapping(cache_path);
            if (mapping) {
                auto chain = dds_to_bc7_chain(mapping->data<uint8_t>(),
                                               static_cast<size_t>(mapping->get_size()));
                if (chain.valid())
                    return upload_texture_bc7(device_, chain, cache_key);
            }
        }

        // Disk cache miss — compress, save, upload.
        auto chain = compress_bc7(img, is_srgb);
        if (!chain.valid()) return RID();

        auto dds = bc7_chain_to_dds(chain);
        fs_->write_buffer_to_file(cache_path, dds.data(), dds.size());

        return upload_texture_bc7(device_, chain, cache_key);
    }

    // Produces a stable cache:// path from a source path or key.
    // e.g. "assets://textures/rock.png" -> "cache://assets___textures_rock.png_srgb.bc7.dds"
    static std::string make_cache_path(const std::string& src, bool is_srgb) {
        std::string s = src;
        for (char& c : s) {
            if (c == '/' || c == '\\' || c == ':') c = '_';
        }
        s += is_srgb ? "_srgb.bc7.dds" : "_unorm.bc7.dds";
        return "cache://" + s;
    }

    using RDC = RenderingDeviceCommons;

    RenderingDevice*             device_    = nullptr;
    FileSystem::Filesystem*      fs_        = nullptr;
    std::unique_ptr<ImageLoader> loader_;
    bool                         bc7_cache_ = true;
    std::map<std::pair<std::string, bool>, RID> cache_;
};

} // namespace Rendering
