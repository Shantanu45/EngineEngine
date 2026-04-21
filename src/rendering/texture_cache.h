#pragma once
#include "rendering/image_loader.h"
#include <map>
#include <utility>

namespace Rendering {

class TextureCache {
public:
    void init(RenderingDevice* device, FileSystem::FilesystemInterface& fs) {
        device_ = device;
        loader_ = std::make_unique<ImageLoader>(fs);
    }

    // Colour/diffuse textures — gamma-corrected on GPU
    RID color(const std::string& path) {
        return get(path, RDC::DATA_FORMAT_R8G8B8A8_SRGB);
    }

    // Normal maps, specular, displacement — linear data, no gamma
    RID linear(const std::string& path) {
        return get(path, RDC::DATA_FORMAT_R8G8B8A8_UNORM);
    }

    void free_all() {
        for (auto& [key, rid] : cache_)
            device_->free_rid(rid);
        cache_.clear();
    }

private:
    RID get(const std::string& path, RDC::DataFormat format) {
        auto key = std::make_pair(path, format);
        auto it  = cache_.find(key);
        if (it != cache_.end()) return it->second;

        auto img = loader_->load_from_file(path);
        RID  rid = upload_texture_2d(device_, img, format, path);
        return cache_[key] = rid;
    }

    RenderingDevice*             device_ = nullptr;
    std::unique_ptr<ImageLoader> loader_;
    std::map<std::pair<std::string, RDC::DataFormat>, RID> cache_;
};

} // namespace Rendering
