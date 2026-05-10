#include "cache.h"
#include "util/small_vector.h"

namespace Rendering
{


	RDD::FramebufferID FramebufferCache::_create(const FramebufferKey& key)
	{
		auto frame_buffer = device->create_framebuffer_from_render_pass(key.render_pass, key.attachments, key.width, key.height, key.layers);
		cache[key] = { frame_buffer, current_frame };
		return frame_buffer;
	}


	FramebufferCache::~FramebufferCache()
	{
		//clear();
	}


	RDD::FramebufferID FramebufferCache::get(const FramebufferKey& key)
	{
		auto it = cache.find(key);
		if (it != cache.end()) {
			it->second.last_used_frame = current_frame;
			return it->second.framebuffer;
		}
		return _create(key);
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

	RID TransientTextureCache::acquire(const RDD::TextureFormat& p_format, const RenderingDevice::TextureView& p_view, const Util::SmallVector<Util::SmallVector<uint8_t>>& p_data)
	{
		for (auto& entry : cache) {
			if (!entry.in_use && _formats_match(entry.format, p_format)) {
				entry.in_use = true;
				return entry.rid;  // reuse - no alloc
			}
		}

		CacheEntry newEntry;
		newEntry.format = p_format;
		newEntry.rid = device->texture_create(p_format, p_view, p_data);
		newEntry.in_use = true;
		cache.push_back(newEntry);
		return newEntry.rid;

	}


	void TransientTextureCache::release(RID rid)
	{
		for (auto& entry : cache) {
			if (entry.rid == rid) {
				entry.in_use = false;  // return to pool, not destroyed
				return;
			}
		}
	}


	void TransientTextureCache::flush(Rendering::RenderingDevice* device)
	{
		for (auto& entry : cache)
			device->free_rid(entry.rid);
		cache.clear();
	}


	bool TransientTextureCache::_formats_match(const RDD::TextureFormat& a, const RDD::TextureFormat& b)
	{
		return a.format == b.format &&
			a.width == b.width &&
			a.height == b.height &&
			a.usage_bits == b.usage_bits;
	}

}
