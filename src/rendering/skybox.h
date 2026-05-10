#pragma once
#include <array>
#include <string>
#include <vector>
#include "rendering/image_loader.h"
#include "filesystem/filesystem.h"
#include "util/small_vector.h"

namespace Rendering
{
    // Load 6 face images (right, left, top, bottom, front, back) into a
    // sRGB cubemap texture and return its RID.
    inline RID load_skybox_cubemap(
        RenderingDevice* device,
        FileSystem::FilesystemInterface& fs,
        const std::array<std::string, 6>& faces)
    {
        ImageLoader img_loader(fs);
        auto face0 = img_loader.load_from_file(faces[0]);

        RenderingDeviceCommons::TextureFormat tf;
        tf.width        = face0.width;
        tf.height       = face0.height;
        tf.array_layers = 6;
        tf.texture_type = RenderingDeviceCommons::TEXTURE_TYPE_CUBE;
        tf.usage_bits   = RenderingDeviceCommons::TEXTURE_USAGE_SAMPLING_BIT
                        | RenderingDeviceCommons::TEXTURE_USAGE_CAN_UPDATE_BIT;
        tf.format       = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_SRGB;

        Util::SmallVector<Util::SmallVector<uint8_t>> face_pixels;
        face_pixels.push_back(std::move(face0.pixels));
        for (int i = 1; i < 6; i++)
            face_pixels.push_back(img_loader.load_from_file(faces[i]).pixels);

        RID rid = device->texture_create(tf, RenderingDevice::TextureView(), face_pixels);
        device->set_resource_name(rid, "Skybox cubemap");
        return rid;
    }
}
