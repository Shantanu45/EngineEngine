#pragma once
#include "util/rid.h"

namespace Rendering { class RenderingDevice; }

// Owning RAII wrapper for RID. Move-only — calls RenderingDevice::free_rid on destruction.
// Use raw RID for non-owning references (parameters, temporaries, map keys).
class RIDHandle {
public:
    RIDHandle() = default;
    explicit RIDHandle(RID r) : _rid(r) {}
    ~RIDHandle() { reset(); }

    RIDHandle(RIDHandle&& o) noexcept : _rid(o._rid) { o._rid = RID(); }
    RIDHandle& operator=(RIDHandle&& o) noexcept {
        if (this != &o) { reset(); _rid = o._rid; o._rid = RID(); }
        return *this;
    }
    // Assign from a raw RID — frees any currently held resource first.
    RIDHandle& operator=(RID r) noexcept {
        if (_rid != r) { reset(); _rid = r; }
        return *this;
    }
    RIDHandle(const RIDHandle&) = delete;
    RIDHandle& operator=(const RIDHandle&) = delete;

    // Frees the held RID and nulls it.
    void reset();

    // Releases ownership without freeing. Use when transferring to a cache or owner.
    RID release() { RID r = _rid; _rid = RID(); return r; }

    RID  get()      const { return _rid; }
    bool is_valid() const { return _rid.is_valid(); }
    bool is_null()  const { return _rid.is_null(); }

    // Implicit conversion so RIDHandle can be passed anywhere a raw RID is expected.
    operator RID() const { return _rid; }

private:
    RID _rid;
};
