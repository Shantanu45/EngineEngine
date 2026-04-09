// uniform_buffer.h

#pragma once
#include "rendering/rendering_device.h"

namespace Rendering
{
	template<typename T>
	struct UniformBuffer {

		RID rid;

		// -------------------------------------------------------
		// Lifetime
		// -------------------------------------------------------

		void create(RenderingDevice* device, const char* debug_name = nullptr) {
			rid = device->uniform_buffer_create(sizeof(T));
			if (debug_name)
				device->set_resource_name(rid, debug_name);
		}

		void free(RenderingDevice* device) {
			if (rid.is_valid()) {
				device->free_rid(rid);
				rid = RID();
			}
		}

		// -------------------------------------------------------
		// Upload
		// -------------------------------------------------------

		// Upload a complete struct
		void upload(RenderingDevice* device, const T& data) {
			device->buffer_update(rid, 0, sizeof(T), &data);
		}

		// Upload a sub-range - useful for large structs where only
		// one field changed (e.g. just the light position)
		template<typename Field>
		void upload_field(RenderingDevice* device, Field T::* member, const Field& value) {
			const size_t offset = _offsetof_member(member);
			device->buffer_update(rid, offset, sizeof(Field), &value);
		}

		// -------------------------------------------------------
		// Convenience: create + upload in one call
		// -------------------------------------------------------

		void create_with_data(RenderingDevice* device, const T& data, const char* debug_name = nullptr) {
			create(device, debug_name);
			upload(device, data);
		}

		// -------------------------------------------------------
		// Uniform binding helpers
		// -------------------------------------------------------

		// Returns a pre-filled RD::Uniform ready to pass to uniform_set_create
		RD::Uniform as_uniform(uint32_t binding) const {
			RD::Uniform u;
			u.uniform_type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = binding;
			u.append_id(rid);
			return u;
		}

		bool is_valid() const { return rid.is_valid(); }

	private:
		// Helper to compute byte offset of a member pointer at compile time
		template<typename Field>
		static size_t _offsetof_member(Field T::* member) {
			// Standard-layout types only - fine for UBO structs
			alignas(T) char buf[sizeof(T)] = {};
			T* obj = reinterpret_cast<T*>(buf);
			return reinterpret_cast<size_t>(&(obj->*member))
				- reinterpret_cast<size_t>(obj);
		}
	};
}
