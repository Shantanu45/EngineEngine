#pragma once
#include "rendering_device.h"

namespace Rendering
{
	struct FramebufferKey {
		RDD::RenderPassID    render_pass;
		int        width, height;
		uint32_t layers;
		std::vector<RID> attachments;

		bool operator==(const FramebufferKey& o) const {
			return render_pass == o.render_pass &&
				width == o.width &&
				height == o.height &&
				layers == o.layers &&
				attachments == o.attachments;
		}
	};

	struct FramebufferKeyHash {
		size_t operator()(const FramebufferKey& k) const {
			XXH3_state_t* state = XXH3_createState();
			XXH3_64bits_reset(state);

			XXH3_64bits_update(state, &k.render_pass.id, sizeof(k.render_pass.id));
			XXH3_64bits_update(state, &k.width, sizeof(k.width));
			XXH3_64bits_update(state, &k.height, sizeof(k.height));
			XXH3_64bits_update(state, &k.layers, sizeof(k.layers));

			if (!k.attachments.empty()) {
				XXH3_64bits_update(
					state,
					k.attachments.data(),
					k.attachments.size() * sizeof(k.attachments[0])
				);
			}

			size_t result = (size_t)XXH3_64bits_digest(state);
			XXH3_freeState(state);
			return result;
		}
	};

	class FramebufferCache
	{
	public:
		explicit FramebufferCache(RenderingDevice* p_device) : device(p_device) {}
		~FramebufferCache();

		RDD::FramebufferID get(const FramebufferKey& key);
		void touch(RDD::FramebufferID p_fb);

		void tick(uint32_t p_max_age = 8);

		void clear();

	private:
		struct CacheEntry
		{
			RDD::FramebufferID framebuffer;
			uint64_t last_used_frame = 0;
		};

		RDD::FramebufferID _create(const FramebufferKey& key);

		uint64_t current_frame = 0;

		RenderingDevice* device;

		std::unordered_map<FramebufferKey, CacheEntry, FramebufferKeyHash> cache;
	};


	class TransientTextureCache
	{
	public:
		TransientTextureCache(RenderingDevice* p_device) : device(p_device) {}

		RID acquire(const RDD::TextureFormat& p_format, const RenderingDevice::TextureView& p_view, const std::vector<std::vector<uint8_t>>& p_data);

		void release(RID rid);

		void flush(Rendering::RenderingDevice* device);

	private:
		bool _formats_match(const RDD::TextureFormat& a, const RDD::TextureFormat& b);

		struct CacheEntry {
			RID               rid;
			RDD::TextureFormat format = {};
			RDD::TextureView view = {};
			bool              in_use = false;
		};

		std::vector<CacheEntry> cache;
		RenderingDevice* device;
	};
}
