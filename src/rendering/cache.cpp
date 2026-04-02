#include "cache.h"

namespace Rendering
{


	RDD::FramebufferID FramebufferCache::create(const FramebufferKey& key)
	{
		auto frame_buffer = device->create_framebuffer_from_render_pass(key.render_pass, key.attachments, key.width, key.height);
		cache[key] = { frame_buffer, current_frame };
		return frame_buffer;
	}


	FramebufferCache::~FramebufferCache()
	{
		clear();
	}


	RDD::FramebufferID FramebufferCache::get(const FramebufferKey& key)
	{
		auto it = cache.find(key);
		if (it != cache.end()) {
			it->second.last_used_frame = current_frame;
			return it->second.framebuffer;
		}
		return create(key);
	}


	void FramebufferCache::tick(uint32_t p_max_age /*= 8*/)
	{
		for (auto it = cache.begin(); it != cache.end(); ) {
			if (current_frame - it->second.last_used_frame > p_max_age) {
				device->free_framebuffer(it->second.framebuffer);
				it = cache.erase(it);
			}
			else {
				++it;
			}
		}
		++current_frame;
	}

	void FramebufferCache::clear() {
		for (auto& [key, entry] : cache)
			device->free_framebuffer(entry.framebuffer);
		cache.clear();
	}
}
